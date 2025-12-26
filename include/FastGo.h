#ifndef FASTGO_H
#define FASTGO_H

#include "Database.h" 
#include "CustomHash.h"
#include "Package.h" // Include the new header
#include "CustomGraph.h" // Need access to graph for routing

using namespace std;

enum Role { Admin = 0, Manager = 1, Guest = 2 };

class FastGo {
private:
    Role currentRole;
    string currentUserCity; // Track which manager is logged in
    SimpleHash cityHashTable;
    hashroutes routeHashTable;
    CityDatabase cityDB;
    SaveRoute routeDB;
    PackageDatabase pkgDB; // NEW

public:
    FastGo() : currentRole(Guest), cityDB("cities.db"), routeDB("routes.db"), pkgDB("packages.db") {
        cityDB.loadToSimpleHash(cityHashTable);
        routeDB.loadToHashTable(routeHashTable);
    }

    // --- Login Update ---
    string login(string username, string password) {
        if (username == "admin" && password == "admin123") {
            currentRole = Admin;
            currentUserCity = "";
            return "Success: Admin Login";
        }
        string storedPass = cityHashTable.getPassword(username);
        if (storedPass != "-1" && storedPass == password) {
            currentRole = Manager;
            currentUserCity = username; // Store city name as user
            return "Success: Manager Login";
        }
        return "Error: Invalid Credentials";
    }

    string getLoggedCity() { return currentUserCity; }

    // --- Package Management ---
    
    void createPackage(string sender, string receiver, string addr, string dest, int type, double weight) {
        Package p;
        p.sender = sender; p.receiver = receiver; p.address = addr;
        p.sourceCity = currentUserCity;
        p.destCity = dest;
        p.currentCity = currentUserCity;
        p.type = type; p.weight = weight;
        p.status = CREATED;
        p.ticks = 0;
        pkgDB.addPackage(p);
    }

    void loadPackage(int id) {
        // Manager loads package onto truck -> Status becomes IN_TRANSIT (or LOADED waiting for next shift)
        pkgDB.updateStatus(id, LOADED); 
    }

    void deliverPackage(int id) {
        pkgDB.updateStatus(id, DELIVERED);
    }
    
    void returnPackage(int id) {
        // Send back to sender: Swap Source and Dest, Reset status
        // For simplicity, we just mark FAILED here, or you could implement logic to reverse route
        pkgDB.updateStatus(id, FAILED); 
    }

    vector<Package> getPackagesForManager(string city) {
        vector<Package> all = pkgDB.getAllPackages();
        vector<Package> filtered;
        for (const auto& p : all) {
            // Outgoing (Created/Loaded at this city) OR Incoming (Arrived at this city) OR History (Involved this city)
            if (p.sourceCity == city || p.currentCity == city || p.destCity == city) {
                filtered.push_back(p);
            }
        }
        return filtered;
    }

    vector<Package> getAllPackages() { return pkgDB.getAllPackages(); }

    // --- THE SIMULATION ENGINE ---
    
    // Returns a log of what happened
    vector<string> runTimeStep(Graph& graph) {
        vector<string> logs;
        vector<Package> packages = pkgDB.getAllPackages();

        for (auto& p : packages) {
            // Only move packages that are LOADED or IN_TRANSIT
            if (p.status != LOADED && p.status != IN_TRANSIT) continue;

            // 1. Priority Logic
            p.ticks++;
            bool shouldMove = false;
            
            // Type 1 (Overnight): Every tick (1 click)
            // Type 2 (2-Day): Every 2 ticks
            // Type 3 (Normal): Every 3 ticks
            if (p.type == OVERNIGHT && p.ticks >= 1) shouldMove = true;
            else if (p.type == TWODAY && p.ticks >= 2) shouldMove = true;
            else if (p.type == NORMAL && p.ticks >= 3) shouldMove = true;

            if (shouldMove) {
                // Reset ticks
                pkgDB.updateTicks(p.id, 0); 

                // 2. Dynamic Routing (Calculate at every point)
                string nextCity = graph.getNextHop(p.currentCity, p.destCity);

                if (nextCity == "") {
                    logs.push_back("Pkg #" + to_string(p.id) + " waiting at " + p.currentCity + " (No Route)");
                } 
                else if (nextCity == p.currentCity) {
                     // Already at destination, just ensure status is Arrived
                     pkgDB.updateStatus(p.id, ARRIVED, nextCity);
                     logs.push_back("Pkg #" + to_string(p.id) + " arrived at " + nextCity);
                } 
                else {
                    // Move to next city
                    pkgDB.updateStatus(p.id, IN_TRANSIT, nextCity); // Update location + status
                    logs.push_back("Pkg #" + to_string(p.id) + " moved to " + nextCity);
                    
                    // Check if it just arrived at final destination
                    if (nextCity == p.destCity) {
                        pkgDB.updateStatus(p.id, ARRIVED, nextCity);
                        logs.push_back("Pkg #" + to_string(p.id) + " REACHED DESTINATION " + nextCity);
                    }
                }
            }
        }
        return logs;
    }
    
    // ... (Keep existing addCity, updateCityPosition, etc.) ...
    void updateCityPosition(string name, float x, float y) {
        cityHashTable.updatePosition(name, x, y);
        cityDB.saveFromSimpleHash(cityHashTable);
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
    void saveCitiesToDB() { cityDB.saveFromSimpleHash(cityHashTable); }
};

#endif