#ifndef FASTGO_H
#define FASTGO_H

#include "Database.h" 
#include "CustomHash.h"

using namespace std;

enum Role { Admin = 0, Manager = 1, Guest = 2 };

class FastGo {
private:
    Role currentRole;
    SimpleHash cityHashTable;
    hashroutes routeHashTable;
    CityDatabase cityDB;
    SaveRoute routeDB;

public:
    FastGo() : currentRole(Guest), cityDB("cities.db"), routeDB("routes.db") {
        cityDB.loadToSimpleHash(cityHashTable);
        routeDB.loadToHashTable(routeHashTable);
    }

    string login(string username, string password) {
        if (username == "admin" && password == "admin123") {
            currentRole = Admin;
            return "Success: Admin Login";
        }
        string storedPass = cityHashTable.getPassword(username);
        if (storedPass != "-1" && storedPass == password) {
            currentRole = Manager;
            return "Success: Manager Login";
        }
        return "Error: Invalid Credentials";
    }

    string addCity(string cityName, string cityPassword) {
        if (currentRole != Admin) return "Error: Access Denied";
        int newPointId = cityHashTable.getAll().size();
        if (cityHashTable.getPoint(cityName) != -1) return "Error: City Exists";

        if (cityHashTable.insert(cityName, newPointId, cityPassword)) {
            cityDB.saveFromSimpleHash(cityHashTable);
            return "Success: City Added";
        }
        return "Error: Hash Full";
    }

    void saveCitiesToDB() {
        cityDB.saveFromSimpleHash(cityHashTable);
    }  

    void updateCityPosition(string name, float x, float y) {
        // Update memory
        cityHashTable.updatePosition(name, x, y);
        // Save to Disk
        cityDB.saveFromSimpleHash(cityHashTable);
    }

    string addRoute(string routeKey, int distance) {
        if (currentRole != Admin) return "Error: Access Denied";
        if (routeHashTable.insert(routeKey, distance, false)) {
            routeDB.saveFromHashTable(routeHashTable);
            return "Success: Route Added";
        }
        return "Error: Failed to Add";
    }

    string toggleRouteBlock(string routeKey, bool block) {
        if (currentRole != Admin) return "Error: Access Denied";
        if (block ? routeHashTable.blockRoute(routeKey) : routeHashTable.unblockRoute(routeKey)) {
            routeDB.saveFromHashTable(routeHashTable);
            return block ? "Success: Blocked" : "Success: Unblocked";
        }
        return "Error: Route Not Found";
    }

    Role getRole() { return currentRole; }
    SimpleHash& getCities() { return cityHashTable; }
    hashroutes& getRoutes() { return routeHashTable; }
};

#endif