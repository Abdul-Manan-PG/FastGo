#ifndef PACKAGE_H
#define PACKAGE_H

#include <string>
#include <vector>
#include <sqlite3.h>
#include <sstream>
#include <ctime>
#include <iostream>

using namespace std;

enum PackageType { OVERNIGHT = 1, TWODAY = 2, NORMAL = 3 };
enum PkgStatus { CREATED = 0, LOADED = 1, IN_TRANSIT = 2, ARRIVED = 3, DELIVERED = 4, FAILED = 5 };

struct Package {
    int id;
    string sender;
    string receiver;
    string address;
    string sourceCity;
    string destCity;
    string currentCity;
    int type; 
    double weight;
    int status;
    int ticks;
    
    // NEW: History (format: "City|Time") and Route (format: "City")
    string historyStr; 
    string routeStr;   
};

class PackageDatabase {
private:
    sqlite3* db_ = nullptr;
    const string tableName_ = "Packages";

public:
    PackageDatabase(const string& filename = "packages.db") {
        sqlite3_open(filename.c_str(), &db_);
        // Updated Table Schema
        const char* sql = "CREATE TABLE IF NOT EXISTS Packages ("
                          "ID INTEGER PRIMARY KEY AUTOINCREMENT, "
                          "Sender TEXT, Receiver TEXT, Address TEXT, "
                          "SourceCity TEXT, DestCity TEXT, CurrentCity TEXT, "
                          "Type INT, Weight REAL, Status INT, Ticks INT, "
                          "History TEXT, RoutePlan TEXT);"; // New Columns
        sqlite3_exec(db_, sql, nullptr, nullptr, nullptr);
    }
    ~PackageDatabase() { if (db_) sqlite3_close(db_); }

    void addPackage(const Package& p) {
        string sql = "INSERT INTO Packages (Sender, Receiver, Address, SourceCity, DestCity, CurrentCity, Type, Weight, Status, Ticks, History, RoutePlan) "
                     "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, 0, ?, ?);";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, p.sender.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, p.receiver.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, p.address.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, p.sourceCity.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, p.destCity.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 6, p.currentCity.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 7, p.type);
        sqlite3_bind_double(stmt, 8, p.weight);
        sqlite3_bind_int(stmt, 9, p.status);
        sqlite3_bind_text(stmt, 10, p.historyStr.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 11, p.routeStr.c_str(), -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    void updateStatusAndRoute(int id, int status, string currentCity, string history, string routePlan) {
        string sql = "UPDATE Packages SET Status = ?, CurrentCity = ?, History = ?, RoutePlan = ? WHERE ID = ?";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
        sqlite3_bind_int(stmt, 1, status);
        sqlite3_bind_text(stmt, 2, currentCity.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, history.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, routePlan.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 5, id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    void updateTicks(int id, int ticks) {
        string sql = "UPDATE Packages SET Ticks = ? WHERE ID = ?";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
        sqlite3_bind_int(stmt, 1, ticks);
        sqlite3_bind_int(stmt, 2, id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    // Helper to extract a single package row
    Package extractPackage(sqlite3_stmt* stmt) {
        Package p;
        p.id = sqlite3_column_int(stmt, 0);
        p.sender = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        p.receiver = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        p.address = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        p.sourceCity = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        p.destCity = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        p.currentCity = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        p.type = sqlite3_column_int(stmt, 7);
        p.weight = sqlite3_column_double(stmt, 8);
        p.status = sqlite3_column_int(stmt, 9);
        p.ticks = sqlite3_column_int(stmt, 10);
        
        const char* h = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 11));
        p.historyStr = h ? h : "";
        
        const char* r = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 12));
        p.routeStr = r ? r : "";
        return p;
    }

    Package getPackage(int id) {
        Package p = {-1};
        string sql = "SELECT * FROM Packages WHERE ID = ?";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return p;
        sqlite3_bind_int(stmt, 1, id);
        if (sqlite3_step(stmt) == SQLITE_ROW) p = extractPackage(stmt);
        sqlite3_finalize(stmt);
        return p;
    }

    vector<Package> getAllPackages() {
        vector<Package> pkgs;
        string sql = "SELECT * FROM Packages";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return pkgs;
        while (sqlite3_step(stmt) == SQLITE_ROW) pkgs.push_back(extractPackage(stmt));
        sqlite3_finalize(stmt);
        return pkgs;
    }
};

#endif