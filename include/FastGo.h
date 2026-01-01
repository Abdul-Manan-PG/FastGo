#ifndef FASTGO_H
#define FASTGO_H

#include "Database.h"
#include "CustomHash.h"
#include "Package.h"     // Ensure this is the updated version with History/RoutePlan columns
#include "CustomGraph.h" // Needed for pathfinding calculations
#include <ctime>
#include <sstream>
#include <vector>
#include <string>

using namespace std;

enum Role
{
    Admin = 0,
    Manager = 1,
    Guest = 2
};

class FastGo
{
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

    RiderDatabase riderDB; // Add this member
    string currentRiderUser;

    // --- Helper: Get Current Time as String ---
    string getCurrentTime()
    {
        time_t now = time(nullptr);
        tm *ltm = localtime(&now);

        char buffer[25];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", ltm);

        return string(buffer);
    }

    // --- Helper: Convert Vector to Comma-Separated String ---
    // Used to store the "Future Route" list in the database
    string vecToString(const vector<string> &vec)
    {
        stringstream ss;
        for (size_t i = 0; i < vec.size(); ++i)
        {
            ss << vec[i];
            if (i != vec.size() - 1)
                ss << ",";
        }
        return ss.str();
    }

public:
    FastGo() : currentRole(Guest), cityDB("cities.db"), routeDB("routes.db"), pkgDB("packages.db")
    {
        cityDB.loadToSimpleHash(cityHashTable);
        routeDB.loadToHashTable(routeHashTable);
    }

    // --- Authentication ---
    string login(string username, string password)
    {
        // 1. Admin Check
        if (username == "admin" && password == "admin123")
        {
            currentRole = Admin;
            currentUserCity = "";
            return "Success: Admin Login";
        }

        // 2. Manager Check
        string storedPass = cityHashTable.getPassword(username);
        if (storedPass != "-1" && storedPass == password)
        {
            currentRole = Manager;
            currentUserCity = username;
            return "Success: Manager Login";
        }

        // 3. Rider Check (NEW)
        Rider r = riderDB.getRider(username);
        if (r.id != -1 && r.password == password)
        {
            currentRole = Guest; // Treat Rider as Guest or add a RIDER role to Enum
            currentRiderUser = username;
            currentUserCity = r.city; // Rider operates in this city
            return "Success: Rider Login";
        }

        return "Error: Invalid Credentials";
    }

    // // --- Helper: Get Current Time as String ---
    // string getCurrentTime()
    // {
    //     time_t now = time(0);
    //     tm *ltm = localtime(&now);
    //     char buffer[80];
    //     // CHANGED: Removed seconds (%S) to match "Date then hour : minute" requirement
    //     strftime(buffer, 80, "%Y-%m-%d %H:%M", ltm);
    //     return string(buffer);
    // }

    string getLoggedCity() { return currentUserCity; }

    // --- Package Management ---

    // 1. Create Package
    // Calculates the INITIAL route plan immediately so it can be tracked
    // 1. Create Package [UPDATED WITH PRICING]
    void createPackage(string sender, string receiver, string addr, string dest, int type, double weight, Graph &graph)
    {
        Package p;
        p.sender = sender;
        p.receiver = receiver;
        p.address = addr;
        p.sourceCity = currentUserCity;
        p.destCity = dest;
        p.currentCity = currentUserCity;
        p.type = type;
        p.weight = weight;
        p.status = CREATED;
        p.ticks = 0;
        p.riderId = 0;
        p.attempts = 0;

        // --- NEW: Calculate Price ---
        // Formula: Base($10) + (Weight * $2) + Priority Surcharge
        double basePrice = 5.0;
        double weightCost = weight * 1.2;
        double priorityCost = 0.0;

        if (type == OVERNIGHT)
            priorityCost = 20.0;
        else if (type == TWODAY)
            priorityCost = 10.0;
        // Normal = 0

        p.price = basePrice + weightCost + priorityCost;
        // ----------------------------

        p.historyStr = currentUserCity + "|" + getCurrentTime();

        auto res = graph.getShortestPath(currentUserCity, dest);
        if (res.first != -1)
            p.routeStr = vecToString(res.second);
        else
            p.routeStr = "";

        pkgDB.addPackage(p);
    }

    // 2. Simple Status Update [UPDATED FOR RETURN]
    void updatePkgStatusSimple(int id, int status)
    {
        Package p = pkgDB.getPackage(id);
        if (p.id != -1)
        {
            // If returning, update status to RETURNED (8) and add history
            if (status == RETURNED)
            {
                string newHist = p.historyStr + ",RETURNED TO SENDER|" + getCurrentTime();
                pkgDB.updateStatusAndRoute(id, 8, p.currentCity, newHist, "");
            }
            else
            {
                pkgDB.updateStatusAndRoute(id, status, p.currentCity, p.historyStr, p.routeStr);
            }
        }
    }

    // --- NEW: ADMIN STATS HELPER ---
    struct AdminStats
    {
        double revenue;
        int delivered;
        int inTransit;
        int failed;
    };

    AdminStats getSystemStats()
    {
        vector<Package> all = pkgDB.getAllPackages();
        AdminStats stats = {0.0, 0, 0, 0};

        for (const auto &p : all)
        {
            // Sum Revenue (Price) for valid packages (exclude cancelled if any, or just sum all created)
            stats.revenue += p.price;

            if (p.status == DELIVERED)
                stats.delivered++;
            else if (p.status == IN_TRANSIT || p.status == OUT_FOR_DELIVERY)
                stats.inTransit++;
            else if (p.status == FAILED || p.status == RETURNED)
                stats.failed++;
        }
        return stats;
    }

    // --- THE CORE SIMULATION LOOP ---
    // Moves packages, updates history, and recalculates future routes
    vector<string> runTimeStep(Graph &graph)
    {
        vector<string> logs;
        vector<Package> packages = pkgDB.getAllPackages();

        for (auto &p : packages)
        {
            // Only move packages that are physically moving
            if (p.status != LOADED && p.status != IN_TRANSIT)
                continue;

            // 1. Check Priority Speed (Ticks)
            p.ticks++;
            bool shouldMove = false;
            // Overnight = Move every tick (Fastest)
            // 2-Day = Move every 2 ticks
            // Normal = Move every 3 ticks
            if (p.type == OVERNIGHT && p.ticks >= 1)
                shouldMove = true;
            else if (p.type == TWODAY && p.ticks >= 2)
                shouldMove = true;
            else if (p.type == NORMAL && p.ticks >= 3)
                shouldMove = true;

            if (shouldMove)
            {
                // Determine Next Step dynamically
                // We ask the graph for the best "Next Hop" based on current blocked roads
                string nextCity = graph.getNextHop(p.currentCity, p.destCity);

                // Reset ticks for next movement cycle
                pkgDB.updateTicks(p.id, 0);

                if (nextCity == "")
                {
                    // Road Blocked or Disconnected
                    logs.push_back("Pkg #" + to_string(p.id) + " WAITING at " + p.currentCity + " (No Route Available)");
                }
                else if (nextCity == p.currentCity)
                {
                    // Safety check: If graph says next hop is self, we are likely at dest or stuck
                    if (p.currentCity == p.destCity)
                    {
                        string newHist = p.historyStr + "," + nextCity + "|" + getCurrentTime();
                        pkgDB.updateStatusAndRoute(p.id, ARRIVED, nextCity, newHist, ""); // Clear future route
                        logs.push_back("Pkg #" + to_string(p.id) + " ARRIVED at destination " + nextCity);
                    }
                }
                else
                {
                    // --- EXECUTE MOVE ---

                    // 1. Update Status
                    int newStatus = (nextCity == p.destCity) ? ARRIVED : IN_TRANSIT;

                    // 2. Append to History (Green Line)
                    // Format: "OldHistory,NewCity|Time"
                    string newHist = p.historyStr + "," + nextCity + "|" + getCurrentTime();

                    // 3. Recalculate Future Route (Blue Line)
                    // Now that we are at 'nextCity', what is the path to 'destCity'?
                    string newRoute = "";
                    if (newStatus != ARRIVED)
                    {
                        auto res = graph.getShortestPath(nextCity, p.destCity);
                        if (res.first != -1)
                        {
                            newRoute = vecToString(res.second);
                        }
                    }

                    // 4. Save Changes to DB
                    pkgDB.updateStatusAndRoute(p.id, newStatus, nextCity, newHist, newRoute);
                    logs.push_back("Pkg #" + to_string(p.id) + " moved to " + nextCity);
                }
            }
            else
            {
                // *** FIX ADDED HERE ***
                // If the package did not move, save the incremented ticks to the DB.
                // Otherwise, the counter resets to 0 on the next reload, and it never moves.
                pkgDB.updateTicks(p.id, p.ticks);
            }
        }
        return logs;
    }

    // --- Getters ---

    // For Tracking (Single ID)
    Package getPackageDetails(int id)
    {
        return pkgDB.getPackage(id);
    }

    // For Managers (Filtered by their city)
    vector<Package> getPackagesForManager(string city)
    {
        vector<Package> all = pkgDB.getAllPackages();
        vector<Package> filtered;
        for (const auto &p : all)
        {
            // Show if it originated here, is currently here, or is destined here
            if (p.sourceCity == city || p.currentCity == city || p.destCity == city)
            {
                filtered.push_back(p);
            }
        }
        return filtered;
    }

    // For Admin (All packages)
    vector<Package> getAllPackages()
    {
        return pkgDB.getAllPackages();
    }

    // --- Pass-through functions for Graph/City Management ---

    // Updates X,Y in DB for Drag & Drop
    void updateCityPosition(string name, float x, float y)
    {
        cityHashTable.updatePosition(name, x, y);
        cityDB.saveFromSimpleHash(cityHashTable);
    }

    string addCity(string cityName, string cityPassword)
    {
        if (currentRole != Admin)
            return "Error: Access Denied";
        int newPointId = cityHashTable.getAll().size();
        if (cityHashTable.getPoint(cityName) != -1)
            return "Error: City Exists";

        if (cityHashTable.insert(cityName, newPointId, cityPassword))
        {
            cityDB.saveFromSimpleHash(cityHashTable);
            return "Success: City Added";
        }
        return "Error: Hash Full";
    }

    string addRoute(string routeKey, int distance)
    {
        if (currentRole != Admin)
            return "Error: Access Denied";
        if (routeHashTable.insert(routeKey, distance, false))
        {
            routeDB.saveFromHashTable(routeHashTable);
            return "Success: Route Added";
        }
        return "Error: Failed to Add";
    }

    string toggleRouteBlock(string routeKey, bool block)
    {
        if (currentRole != Admin)
            return "Error: Access Denied";
        if (block ? routeHashTable.blockRoute(routeKey) : routeHashTable.unblockRoute(routeKey))
        {
            routeDB.saveFromHashTable(routeHashTable);
            return block ? "Success: Blocked" : "Success: Unblocked";
        }
        return "Error: Route Not Found";
    }

    Role getRole() { return currentRole; }
    SimpleHash &getCities() { return cityHashTable; }
    hashroutes &getRoutes() { return routeHashTable; }

    // Force save (useful after reset or bulk updates)
    void saveCitiesToDB() { cityDB.saveFromSimpleHash(cityHashTable); }

    // 1. New Assignment Logic
    string assignPackagesToRider(int riderId, const vector<int> &pkgIds)
    {
        int count = 0;

        // 1. Find the Rider
        // Retrieve the list of riders in the current city from the DB
        vector<Rider> cityRiders = riderDB.getRidersByCity(currentUserCity);
        bool riderFound = false;
        for (const auto &r : cityRiders)
        {
            if (r.id == riderId)
            {
                riderFound = true;
                break;
            }
        }
        if (!riderFound)
            return "Error: Rider not found in your city";

        // 2. Loop through provided Package IDs
        for (int pkgId : pkgIds)
        {
            // Retrieve the specific package from the DB
            Package p = pkgDB.getPackage(pkgId);

            // Check if valid and ready for assignment (Status 3=Arrived or 6=At Hub)
            if (p.id != -1 && (p.status == 3 || p.status == 6))
            {
                // Assign the rider in the database and update status to OUT_FOR_DELIVERY (7)
                pkgDB.assignRider(pkgId, riderId);
                count++;
            }
        }

        if (count == 0)
            return "No valid packages were assigned.";
        return "Successfully assigned " + to_string(count) + " packages.";
    }

    // 2. Rider Action Logic (Delivery/Failure)
    // 2. Rider Action Logic (Delivery/Failure)
    string riderAction(int pkgId, string action)
    {
        Package p = pkgDB.getPackage(pkgId);
        if (p.id == -1)
            return "Error";

        if (action == "delivered")
        {
            // FIX: Use ',' to start a NEW history entry.
            // Was: p.historyStr + "|DELIVERED..." which merged it into the previous city.
            string newHist = p.historyStr + ",DELIVERED|" + getCurrentTime();
            pkgDB.updateStatusAndRoute(pkgId, DELIVERED, p.currentCity, newHist, "");
            return "Delivered";
        }
        else if (action == "failed")
        {
            int attempts = p.attempts + 1;
            if (attempts >= 3)
            {
                // FIX: Use ',' here too
                string newHist = p.historyStr + ",RETURNED (3 Failures)|" + getCurrentTime();
                pkgDB.updateAttempts(pkgId, attempts, FAILED); // Return to sender
                return "Returned";
            }
            else
            {
                // Note: We don't necessarily add history for a retry,
                // but you could add ",Attempt Failed|" if you wanted.
                pkgDB.updateAttempts(pkgId, attempts, OUT_FOR_DELIVERY); // Keep trying
                return "Attempt Recorded";
            }
        }
        return "Unknown";
    }

    // 3. Add Rider
    void addRider(string u, string p, string v)
    {
        riderDB.addRider(u, p, v, currentUserCity);
    }

    // 1. Get all riders assigned to the current manager's city
    vector<Rider> getRidersForManager()
    {
        // Uses the existing RiderDatabase method
        return riderDB.getRidersByCity(currentUserCity);
    }

    // 2. Get packages assigned to the currently logged-in rider
    vector<Package> getPackagesForLoggedRider()
    {
        // Identify the rider based on the logged-in username
        Rider r = riderDB.getRider(currentRiderUser);
        if (r.id == -1)
            return {}; // Rider not found

        vector<Package> all = pkgDB.getAllPackages();
        vector<Package> assigned;

        for (const auto &p : all)
        {
            // Filter: Must match Rider ID and be currently "Out for Delivery"
            if (p.riderId == r.id && p.status == OUT_FOR_DELIVERY)
            {
                assigned.push_back(p);
            }
        }
        return assigned;
    }
};

#endif