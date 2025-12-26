#ifndef FASTGO_H
#define FASTGO_H

#include "Database.h" 
#include "CustomHash.h"
#include "Package.h" // Ensure this is the updated version with History/RoutePlan columns
#include "CustomGraph.h" // Needed for pathfinding calculations
#include <ctime>
#include <sstream>
#include <vector>
#include <string>

using namespace std;

enum Role { Admin = 0, Manager = 1, Guest = 2 };

class FastGo {
private:
    Role currentRole;
    string currentUserCity; // Tracks which city a Manager belongs to
    
    // Data Structures
    SimpleHash cityHashTable;
    hashroutes routeHashTable;
    
    // Persistence
    CityDatabase cityDB;
    SaveRoute routeDB;
    PackageDatabase pkgDB;

    // --- Helper: Get Current Time as String ---
    string getCurrentTime() {
        time_t now = time(0);
        tm *ltm = localtime(&now);
        char buffer[80];
        // Format: YYYY-MM-DD HH:MM:SS
        strftime(buffer, 80, "%Y-%m-%d %H:%M:%S", ltm);
        return string(buffer);
    }

    // --- Helper: Convert Vector to Comma-Separated String ---
    // Used to store the "Future Route" list in the database
    string vecToString(const vector<string>& vec) {
        stringstream ss;
        for(size_t i = 0; i < vec.size(); ++i) {
            ss << vec[i];
            if(i != vec.size() - 1) ss << ",";
        }
        return ss.str();
    }

public:
    FastGo() : currentRole(Guest), cityDB("cities.db"), routeDB("routes.db"), pkgDB("packages.db") {
        cityDB.loadToSimpleHash(cityHashTable);
        routeDB.loadToHashTable(routeHashTable);
    }

    // --- Authentication ---
    string login(string username, string password) {
        if (username == "admin" && password == "admin123") {
            currentRole = Admin;
            currentUserCity = "";
            return "Success: Admin Login";
        }
        string storedPass = cityHashTable.getPassword(username);
        if (storedPass != "-1" && storedPass == password) {
            currentRole = Manager;
            currentUserCity = username; // Manager is tied to a specific city
            return "Success: Manager Login";
        }
        return "Error: Invalid Credentials";
    }

    string getLoggedCity() { return currentUserCity; }

    // --- Package Management ---

    // 1. Create Package
    // Calculates the INITIAL route plan immediately so it can be tracked
    void createPackage(string sender, string receiver, string addr, string dest, int type, double weight, Graph& graph) {
        Package p;
        p.sender = sender; p.receiver = receiver; p.address = addr;
        p.sourceCity = currentUserCity;
        p.destCity = dest;
        p.currentCity = currentUserCity;
        p.type = type; p.weight = weight;
        p.status = CREATED;
        p.ticks = 0;

        // A. Initial History: "SourceCity|Time"
        p.historyStr = currentUserCity + "|" + getCurrentTime();

        // B. Initial Future Route: Calculate path from Source -> Dest
        auto res = graph.getShortestPath(currentUserCity, dest);
        if (res.first != -1) {
            // Save the full list of cities as the planned route
            p.routeStr = vecToString(res.second);
        } else {
            p.routeStr = ""; // No route currently available
        }

        pkgDB.addPackage(p);
    }

    // 2. Simple Status Update (Load, Deliver, Return)
    // Used when the manager clicks buttons. Does not move the package physically.
    void updatePkgStatusSimple(int id, int status) {
        Package p = pkgDB.getPackage(id);
        if (p.id != -1) {
            // Preserve existing location, history, and route. Only change status.
            pkgDB.updateStatusAndRoute(id, status, p.currentCity, p.historyStr, p.routeStr);
        }
    }

    // --- THE CORE SIMULATION LOOP ---
    // Moves packages, updates history, and recalculates future routes
    vector<string> runTimeStep(Graph& graph) {
        vector<string> logs;
        vector<Package> packages = pkgDB.getAllPackages();

        for (auto& p : packages) {
            // Only move packages that are physically moving
            if (p.status != LOADED && p.status != IN_TRANSIT) continue;

            // 1. Check Priority Speed (Ticks)
            p.ticks++;
            bool shouldMove = false;
            // Overnight = Move every tick (Fastest)
            // 2-Day = Move every 2 ticks
            // Normal = Move every 3 ticks
            if (p.type == OVERNIGHT && p.ticks >= 1) shouldMove = true;
            else if (p.type == TWODAY && p.ticks >= 2) shouldMove = true;
            else if (p.type == NORMAL && p.ticks >= 3) shouldMove = true;

            if (shouldMove) {
                // Determine Next Step dynamically
                // We ask the graph for the best "Next Hop" based on current blocked roads
                string nextCity = graph.getNextHop(p.currentCity, p.destCity);
                
                // Reset ticks for next movement cycle
                pkgDB.updateTicks(p.id, 0); 

                if (nextCity == "") {
                    // Road Blocked or Disconnected
                    logs.push_back("Pkg #" + to_string(p.id) + " WAITING at " + p.currentCity + " (No Route Available)");
                } 
                else if (nextCity == p.currentCity) {
                     // Safety check: If graph says next hop is self, we are likely at dest or stuck
                     if (p.currentCity == p.destCity) {
                        string newHist = p.historyStr + "," + nextCity + "|" + getCurrentTime();
                        pkgDB.updateStatusAndRoute(p.id, ARRIVED, nextCity, newHist, ""); // Clear future route
                        logs.push_back("Pkg #" + to_string(p.id) + " ARRIVED at destination " + nextCity);
                     }
                } 
                else {
                    // --- EXECUTE MOVE ---
                    
                    // 1. Update Status
                    int newStatus = (nextCity == p.destCity) ? ARRIVED : IN_TRANSIT;
                    
                    // 2. Append to History (Green Line)
                    // Format: "OldHistory,NewCity|Time"
                    string newHist = p.historyStr + "," + nextCity + "|" + getCurrentTime();
                    
                    // 3. Recalculate Future Route (Blue Line)
                    // Now that we are at 'nextCity', what is the path to 'destCity'?
                    string newRoute = "";
                    if (newStatus != ARRIVED) {
                        auto res = graph.getShortestPath(nextCity, p.destCity);
                        if (res.first != -1) {
                            newRoute = vecToString(res.second);
                        }
                    }

                    // 4. Save Changes to DB
                    pkgDB.updateStatusAndRoute(p.id, newStatus, nextCity, newHist, newRoute);
                    logs.push_back("Pkg #" + to_string(p.id) + " moved to " + nextCity);
                }
            }
        }
        return logs;
    }

    // --- Getters ---
    
    // For Tracking (Single ID)
    Package getPackageDetails(int id) { 
        return pkgDB.getPackage(id); 
    }

    // For Managers (Filtered by their city)
    vector<Package> getPackagesForManager(string city) {
        vector<Package> all = pkgDB.getAllPackages();
        vector<Package> filtered;
        for (const auto& p : all) {
            // Show if it originated here, is currently here, or is destined here
            if (p.sourceCity == city || p.currentCity == city || p.destCity == city) {
                filtered.push_back(p);
            }
        }
        return filtered;
    }

    // For Admin (All packages)
    vector<Package> getAllPackages() { 
        return pkgDB.getAllPackages(); 
    }
    
    // --- Pass-through functions for Graph/City Management ---

    // Updates X,Y in DB for Drag & Drop
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
    
    // Force save (useful after reset or bulk updates)
    void saveCitiesToDB() { cityDB.saveFromSimpleHash(cityHashTable); }
};

#endif