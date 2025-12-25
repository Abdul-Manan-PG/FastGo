#ifndef SQLITEDB_H
#define SQLITEDB_H

#include <iostream>
#include <string>
#include <vector>
#include <sqlite3.h>
#include "CustomHash.h" 

using namespace std;

class SaveRoute {
private:
    sqlite3* db_ = nullptr;
    const string tableName_ = "Routes";
public:
    SaveRoute(const std::string& filename) {
        if (sqlite3_open(filename.c_str(), &db_) != SQLITE_OK) {
            std::cerr << "DB Open Error: " << sqlite3_errmsg(db_) << std::endl;
        } else {
            const char* sql = "CREATE TABLE IF NOT EXISTS Routes ("
                              "Key TEXT PRIMARY KEY, "
                              "Distance INT, "
                              "IsBlocked INT);";
            char* err = nullptr;
            sqlite3_exec(db_, sql, nullptr, nullptr, &err);
            if (err) {
                std::cerr << "Create Table Error: " << err << std::endl;
                sqlite3_free(err);
            }
        }
    }

    ~SaveRoute() {
        if (db_) sqlite3_close(db_);
    }

    // Load data FROM Database INTO HashRoutes
    void loadToHashTable(hashroutes& ht) {
        std::string sql = "SELECT Key, Distance, IsBlocked FROM " + tableName_;
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Load Error" << std::endl;
            return;
        }

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::string key = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            int dist = sqlite3_column_int(stmt, 1);
            bool blocked = sqlite3_column_int(stmt, 2) != 0;

            ht.insert(key, dist, blocked);
        }
        sqlite3_finalize(stmt);
        // std::cout << "Routes loaded successfully." << std::endl;
    }

    // Save data FROM HashRoutes INTO Database
    void saveFromHashTable(hashroutes& ht) {
        sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

        std::string sql = "INSERT OR REPLACE INTO " + tableName_ + " (Key, Distance, IsBlocked) VALUES (?, ?, ?);";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);

        vector<RouteEntry> allData = ht.getAllRoutes();

        for (const auto& entry : allData) {
            sqlite3_bind_text(stmt, 1, entry.key.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 2, entry.distance);
            sqlite3_bind_int(stmt, 3, entry.isBlocked ? 1 : 0);
            
            sqlite3_step(stmt);
            sqlite3_reset(stmt);
        }

        sqlite3_finalize(stmt);
        sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
        std::cout << "Routes saved to database." << std::endl;
    }
};

class CityDatabase {
private:
    sqlite3* db_ = nullptr;
    const std::string tableName_ = "Cities";

public:
    CityDatabase(const std::string& filename = "cities.db") {
        if (sqlite3_open(filename.c_str(), &db_) != SQLITE_OK) {
            std::cerr << "City DB Open Error: " << sqlite3_errmsg(db_) << std::endl;
        } else {
            // Updated Table: Includes Password now
            const char* sql = "CREATE TABLE IF NOT EXISTS Cities ("
                              "Name TEXT PRIMARY KEY, "
                              "Value INT, "
                              "Password TEXT);"; 
            char* err = nullptr;
            sqlite3_exec(db_, sql, nullptr, nullptr, &err);
            if (err) {
                std::cerr << "Create City Table Error: " << err << std::endl;
                sqlite3_free(err);
            }
        }
    }

    ~CityDatabase() {
        if (db_) sqlite3_close(db_);
    }

    // Load data FROM "cities.db" INTO SimpleHash
    void loadToSimpleHash(SimpleHash& sh) {
        // Now selecting Password as well
        std::string sql = "SELECT Name, Value, Password FROM " + tableName_;
        sqlite3_stmt* stmt;

        if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Load City Error" << std::endl;
            return;
        }

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::string key = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            int val = sqlite3_column_int(stmt, 1);
            // Handle null passwords gracefully
            const char* pwdText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            std::string pwd = (pwdText) ? pwdText : "";

            sh.insert(key, val, pwd);
        }
        sqlite3_finalize(stmt);
        // std::cout << "Cities loaded successfully." << std::endl;
    }

    // Save data FROM SimpleHash INTO "cities.db"
    void saveFromSimpleHash(SimpleHash& sh) {
        sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

        std::string sql = "INSERT OR REPLACE INTO " + tableName_ + " (Name, Value, Password) VALUES (?, ?, ?);";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);

        // Get data (SimpleHash::getAll returns vector<City>)
        vector<City> allData = sh.getAll();

        for (const auto& entry : allData) {
            sqlite3_bind_text(stmt, 1, entry.name.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 2, entry.point);
            sqlite3_bind_text(stmt, 3, entry.password.c_str(), -1, SQLITE_STATIC);
            
            sqlite3_step(stmt);
            sqlite3_reset(stmt);
        }

        sqlite3_finalize(stmt);
        sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
        std::cout << "Cities updated in database." << std::endl;
    }
};

#endif