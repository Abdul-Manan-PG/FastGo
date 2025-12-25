#ifndef FASTGO_H
#define FASTGO_H

#include "Database.h" 
#include "CustomHash.h"

using namespace std;

// Enum for Roles
enum Role { Admin = 0, Manager = 1, Guest = 2 };

class FastGo {
private:
    Role currentRole;
    bool isLoggedIn;

    // Data Structures (In-Memory)
    SimpleHash cityHashTable;
    hashroutes routeHashTable;

    // Database Handlers (Persistence)
    CityDatabase cityDB;
    SaveRoute routeDB;

public:
    // Constructor: Initializes DBs and loads data into Hash Tables
    FastGo() 
        : currentRole(Guest), isLoggedIn(false), 
          cityDB("cities.db"), routeDB("routes.db") 
    {
        cout << "Loading system data..." << endl;
        cityDB.loadToSimpleHash(cityHashTable);
        routeDB.loadToHashTable(routeHashTable);
        cout << "System Ready." << endl;
    }

    // Login Function
    // Uses City Name as Username and stored Password
    bool login(string username, string password) {
        // 1. Hardcoded Master Admin Check
        if (username == "admin" && password == "admin123") {
            currentRole = Admin;
            isLoggedIn = true;
            cout << "Login Successful: Welcome Administrator." << endl;
            return true;
        }

        // 2. Check CityHashTable for Manager Login
        string storedPass = cityHashTable.getPassword(username);
        
        // getPassword returns "-1" if user doesn't exist (as per CustomHash.h)
        if (storedPass != "-1" && storedPass == password) {
            currentRole = Manager;
            isLoggedIn = true;
            cout << "Login Successful: Welcome Manager of " << username << "." << endl;
            return true;
        }

        cout << "Login Failed: Invalid City Name or Password." << endl;
        return false;
    }

    // Add City/Manager (Only Admin can do this)
    void addCity(string cityName, string cityPassword) {
        if (!isLoggedIn || currentRole != Admin) {
            cout << "Access Denied: Only Admins can add Cities." << endl;
            return;
        }

        // Auto-increment logic:
        // If we have 3 cities (0, 1, 2), size is 3, so next ID is 3.
        int newPointId = cityHashTable.getAll().size();

        // Check if city already exists to prevent duplicate IDs or overwrites
        // (Optional: SimpleHash updates existing keys, but for ID logic strictly new is better)
        if (cityHashTable.getPoint(cityName) != -1) {
            cout << "Error: City '" << cityName << "' already exists." << endl;
            return;
        }

        // Insert into Hash Table
        if (cityHashTable.insert(cityName, newPointId, cityPassword)) {
            cout << "Success: City '" << cityName << "' added with Point ID: " << newPointId << endl;
            
            // Save to Database immediately
            cityDB.saveFromSimpleHash(cityHashTable);
        } else {
            cout << "Error: Hash Table full or error inserting." << endl;
        }
    }

    // Add Route (Only Admin can do this)
    void addRoute(string routeKey, int distance) {
        if (!isLoggedIn || currentRole != Admin) {
            cout << "Access Denied: Only Admins can add Routes." << endl;
            return;
        }

        // Insert into Hash Table
        if (routeHashTable.insert(routeKey, distance, false)) {
            cout << "Success: Route '" << routeKey << "' added." << endl;
            
            // Save to Database immediately
            routeDB.saveFromHashTable(routeHashTable);
        } else {
            cout << "Error: Could not add route." << endl;
        }
    }

    // Helper: Block/Unblock Route (Optional, usually Admin or Logic controlled)
    void toggleRouteBlock(string routeKey, bool block) {
        if (!isLoggedIn || currentRole != Admin) {
            cout << "Access Denied: Only Admins can block/unblock routes." << endl;
            return;
        }
        
        if (block) routeHashTable.blockRoute(routeKey);
        else routeHashTable.unblockRoute(routeKey);
        
        routeDB.saveFromHashTable(routeHashTable); // Persist change
        cout << "Route " << routeKey << (block ? " blocked." : " unblocked.") << endl;
    }

    Role getRole() { return currentRole; }
    
    // Getters for main to use if needed
    SimpleHash& getCities() { return cityHashTable; }
    hashroutes& getRoutes() { return routeHashTable; }
};

#endif