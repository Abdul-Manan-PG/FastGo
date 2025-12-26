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
        sqlite3_open(filename.c_str(), &db_);
        const char* sql = "CREATE TABLE IF NOT EXISTS Routes (Key TEXT PRIMARY KEY, Distance INT, IsBlocked INT);";
        sqlite3_exec(db_, sql, nullptr, nullptr, nullptr);
    }
    ~SaveRoute() { if (db_) sqlite3_close(db_); }

    void loadToHashTable(hashroutes& ht) {
        std::string sql = "SELECT Key, Distance, IsBlocked FROM " + tableName_;
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            ht.insert(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)),
                      sqlite3_column_int(stmt, 1),
                      sqlite3_column_int(stmt, 2) != 0);
        }
        sqlite3_finalize(stmt);
    }

    void saveFromHashTable(hashroutes& ht) {
        sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
        std::string sql = "INSERT OR REPLACE INTO " + tableName_ + " (Key, Distance, IsBlocked) VALUES (?, ?, ?);";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
        for (const auto& entry : ht.getAllRoutes()) {
            sqlite3_bind_text(stmt, 1, entry.key.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 2, entry.distance);
            sqlite3_bind_int(stmt, 3, entry.isBlocked ? 1 : 0);
            sqlite3_step(stmt);
            sqlite3_reset(stmt);
        }
        sqlite3_finalize(stmt);
        sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
    }
};

class CityDatabase {
private:
    sqlite3* db_ = nullptr;
    const std::string tableName_ = "Cities";
public:
    CityDatabase(const std::string& filename = "cities.db") {
        sqlite3_open(filename.c_str(), &db_);
        const char* sql = "CREATE TABLE IF NOT EXISTS Cities (Name TEXT PRIMARY KEY, Value INT, Password TEXT);"; 
        sqlite3_exec(db_, sql, nullptr, nullptr, nullptr);
    }
    ~CityDatabase() { if (db_) sqlite3_close(db_); }

    void loadToSimpleHash(SimpleHash& sh) {
        std::string sql = "SELECT Name, Value, Password FROM " + tableName_;
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* pwd = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            sh.insert(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)),
                      sqlite3_column_int(stmt, 1),
                      pwd ? pwd : "");
        }
        sqlite3_finalize(stmt);
    }

    void saveFromSimpleHash(SimpleHash& sh) {
        sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
        std::string sql = "INSERT OR REPLACE INTO " + tableName_ + " (Name, Value, Password) VALUES (?, ?, ?);";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
        for (const auto& entry : sh.getAll()) {
            sqlite3_bind_text(stmt, 1, entry.name.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 2, entry.point);
            sqlite3_bind_text(stmt, 3, entry.password.c_str(), -1, SQLITE_STATIC);
            sqlite3_step(stmt);
            sqlite3_reset(stmt);
        }
        sqlite3_finalize(stmt);
        sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
    }
};
#endif