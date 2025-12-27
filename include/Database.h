#ifndef SQLITEDB_H
#define SQLITEDB_H

#include <iostream>
#include <string>
#include <vector>
#include <sqlite3.h>
#include "CustomHash.h"

using namespace std;

class SaveRoute
{
private:
    sqlite3 *db_ = nullptr;
    const string tableName_ = "Routes";

public:
    SaveRoute(const std::string &filename)
    {
        sqlite3_open(filename.c_str(), &db_);
        const char *sql = "CREATE TABLE IF NOT EXISTS Routes (Key TEXT PRIMARY KEY, Distance INT, IsBlocked INT);";
        sqlite3_exec(db_, sql, nullptr, nullptr, nullptr);
    }
    ~SaveRoute()
    {
        if (db_)
            sqlite3_close(db_);
    }

    void loadToHashTable(hashroutes &ht)
    {
        std::string sql = "SELECT Key, Distance, IsBlocked FROM " + tableName_;
        sqlite3_stmt *stmt;
        if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
            return;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            ht.insert(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0)),
                      sqlite3_column_int(stmt, 1),
                      sqlite3_column_int(stmt, 2) != 0);
        }
        sqlite3_finalize(stmt);
    }

    void saveFromHashTable(hashroutes &ht)
    {
        sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
        std::string sql = "INSERT OR REPLACE INTO " + tableName_ + " (Key, Distance, IsBlocked) VALUES (?, ?, ?);";
        sqlite3_stmt *stmt;
        sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
        for (const auto &entry : ht.getAllRoutes())
        {
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

class CityDatabase
{
private:
    sqlite3 *db_ = nullptr;
    const std::string tableName_ = "Cities";

public:
    CityDatabase(const std::string &filename = "cities.db")
    {
        sqlite3_open(filename.c_str(), &db_);
        // UPDATE: Added X REAL, Y REAL
        const char *sql = "CREATE TABLE IF NOT EXISTS Cities (Name TEXT PRIMARY KEY, Value INT, Password TEXT, X REAL, Y REAL);";
        sqlite3_exec(db_, sql, nullptr, nullptr, nullptr);
    }
    ~CityDatabase()
    {
        if (db_)
            sqlite3_close(db_);
    }

    void loadToSimpleHash(SimpleHash &sh)
    {
        // UPDATE: Select X and Y
        std::string sql = "SELECT Name, Value, Password, X, Y FROM " + tableName_;
        sqlite3_stmt *stmt;
        if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
            return;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const char *pwd = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
            sh.insert(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0)),
                      sqlite3_column_int(stmt, 1),
                      pwd ? pwd : "",
                      (float)sqlite3_column_double(stmt, 3), // Load X
                      (float)sqlite3_column_double(stmt, 4)  // Load Y
            );
        }
        sqlite3_finalize(stmt);
    }

    void saveFromSimpleHash(SimpleHash &sh)
    {
        sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
        // UPDATE: Insert X and Y
        std::string sql = "INSERT OR REPLACE INTO " + tableName_ + " (Name, Value, Password, X, Y) VALUES (?, ?, ?, ?, ?);";
        sqlite3_stmt *stmt;
        sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
        for (const auto &entry : sh.getAll())
        {
            sqlite3_bind_text(stmt, 1, entry.name.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 2, entry.point);
            sqlite3_bind_text(stmt, 3, entry.password.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_double(stmt, 4, entry.x); // Save X
            sqlite3_bind_double(stmt, 5, entry.y); // Save Y
            sqlite3_step(stmt);
            sqlite3_reset(stmt);
        }
        sqlite3_finalize(stmt);
        sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
    }
};

// [In FastGo/include/Database.h, insert before #endif]

struct Rider
{
    int id;
    string username;
    string password;
    string vehicle; // "bike" or "bus"
    string city;
};

class RiderDatabase
{
private:
    sqlite3 *db_ = nullptr;

public:
    RiderDatabase(const string &filename = "riders.db")
    {
        sqlite3_open(filename.c_str(), &db_);
        const char *sql = "CREATE TABLE IF NOT EXISTS Riders (ID INTEGER PRIMARY KEY AUTOINCREMENT, Username TEXT, Password TEXT, Vehicle TEXT, City TEXT);";
        sqlite3_exec(db_, sql, nullptr, nullptr, nullptr);
    }
    ~RiderDatabase()
    {
        if (db_)
            sqlite3_close(db_);
    }

    void addRider(string user, string pass, string vehicle, string city)
    {
        string sql = "INSERT INTO Riders (Username, Password, Vehicle, City) VALUES (?, ?, ?, ?);";
        sqlite3_stmt *stmt;
        sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, user.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, pass.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, vehicle.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, city.c_str(), -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    Rider getRider(string username)
    {
        Rider r = {-1, "", "", "", ""};
        string sql = "SELECT ID, Username, Password, Vehicle, City FROM Riders WHERE Username = ?";
        sqlite3_stmt *stmt;
        if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
            return r;
        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            r.id = sqlite3_column_int(stmt, 0);
            r.username = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
            r.password = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
            r.vehicle = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
            r.city = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4));
        }
        sqlite3_finalize(stmt);
        return r;
    }

    vector<Rider> getRidersByCity(string city) {
        vector<Rider> riders;
        string sql = "SELECT ID, Username, Password, Vehicle, City FROM Riders WHERE City = ?";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return riders;
        
        sqlite3_bind_text(stmt, 1, city.c_str(), -1, SQLITE_STATIC);
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Rider r;
            r.id = sqlite3_column_int(stmt, 0);
            r.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            
            // --- FIX START: Populate missing fields ---
            r.password = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)); 
            r.vehicle = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            r.city = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            // --- FIX END ---
            
            riders.push_back(r);
        }
        sqlite3_finalize(stmt);
        return riders;
    }
};

#endif