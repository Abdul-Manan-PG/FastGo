#define CROW_MAIN
#include "include/crow_all.h"
#include "include/FastGo.h"
#include "include/CustomGraph.h"
#include <sstream>

// Helper to split strings (e.g., "City|Time,City|Time" -> vector)
vector<string> split(string s, char delimiter)
{
    vector<string> tokens;
    string token;
    istringstream tokenStream(s);
    while (getline(tokenStream, token, delimiter))
    {
        tokens.push_back(token);
    }
    return tokens;
}

int main()
{
    crow::SimpleApp app;

    // --- System Core ---
    FastGo appCore;
    Graph graph(appCore.getCities(), appCore.getRoutes());

    auto refresh = [&]()
    {
        graph.refreshGraph();
        graph.syncToHash();
        appCore.saveCitiesToDB();
    };

    // --- STATIC FILE SERVING ---
    CROW_ROUTE(app, "/")([](const crow::request &, crow::response &res)
                         {
        res.set_static_file_info("static/index.html"); res.end(); });
    CROW_ROUTE(app, "/<string>")([](const crow::request &, crow::response &res, string filename)
                                 {
        res.set_static_file_info("static/" + filename); res.end(); });

    // --- AUTH ---
    CROW_ROUTE(app, "/api/login").methods(crow::HTTPMethod::Post)([&](const crow::request &req)
                                                                  {
        auto x = crow::json::load(req.body);
        if (!x) return crow::response(400);
        string resStr = appCore.login(x["username"].s(), x["password"].s());
        
        crow::json::wvalue res;
        if (resStr.find("Success") != string::npos) {
            res["status"] = "success";
            
            // --- FIX: Correctly identify role ---
            Role r = appCore.getRole();
            if (r == Admin) res["role"] = "admin";
            else if (r == Manager) res["role"] = "manager";
            else res["role"] = "rider"; // Assuming Guest/2 is used for Riders
            
            res["city"] = appCore.getLoggedCity();
        } else { 
            res["status"] = "error"; 
            res["message"] = resStr; 
        }
        return crow::response(res); });

    // --- PACKAGE MANAGEMENT ---

    // 1. Create Package (Calculates Initial Route immediately)
    CROW_ROUTE(app, "/api/add_package").methods(crow::HTTPMethod::Post)([&](const crow::request &req)
                                                                        {
        auto x = crow::json::load(req.body);
        if(!x) return crow::response(400);
        // We pass 'graph' so it can calculate the initial future path (Blue line)
        appCore.createPackage(
            x["sender"].s(), x["receiver"].s(), x["address"].s(),
            x["dest"].s(), x["type"].i(), x["weight"].d(), graph
        );
        return crow::response(200); });

    // 2. Track Package (Parses History & Route Strings)
    CROW_ROUTE(app, "/api/track_package")
    ([&](const crow::request &req)
     {
        string idStr = req.url_params.get("id");
        if(idStr.empty()) return crow::response(400);
        
        Package p = appCore.getPackageDetails(stoi(idStr));
        crow::json::wvalue res;

        if(p.id != -1) {
            res["found"] = true;
            res["id"] = p.id;
            res["sender"] = p.sender; res["receiver"] = p.receiver;
            res["source"] = p.sourceCity; res["dest"] = p.destCity;
            res["current"] = p.currentCity;
            res["status"] = p.status; res["type"] = p.type;

            // Parse History: "CityA|Time,CityB|Time"
            vector<string> histEntries = split(p.historyStr, ',');
            for(size_t i=0; i<histEntries.size(); i++) {
                vector<string> parts = split(histEntries[i], '|');
                res["history"][i]["city"] = parts[0];
                res["history"][i]["time"] = (parts.size() > 1) ? parts[1] : "";
            }

            // Parse Future Route: "CityC,CityD"
            vector<string> planEntries = split(p.routeStr, ',');
            for(size_t i=0; i<planEntries.size(); i++) {
                res["future"][i] = planEntries[i];
            }
        } else {
            res["found"] = false;
        }
        return crow::response(res); });

    // 3. Manager Packages
    CROW_ROUTE(app, "/api/manager_packages")
    ([&](const crow::request &req)
     {
        string city = req.url_params.get("city");
        vector<Package> pkgs = appCore.getPackagesForManager(city);
        crow::json::wvalue res;
        for (size_t i = 0; i < pkgs.size(); i++) {
            res[i]["id"] = pkgs[i].id;
            res[i]["sender"] = pkgs[i].sender;
            res[i]["dest"] = pkgs[i].destCity;
            res[i]["current"] = pkgs[i].currentCity;
            res[i]["status"] = pkgs[i].status;
        }
        return crow::response(res); });

    // 4. Admin Packages (All)
    CROW_ROUTE(app, "/api/admin_packages")
    ([&]()
     {
        if(appCore.getRole() != Admin) return crow::response(403);
        vector<Package> all = appCore.getAllPackages();
        crow::json::wvalue res;
        for(size_t i=0; i<all.size(); i++) {
            res[i]["id"] = all[i].id;
            res[i]["current"] = all[i].currentCity;
            res[i]["dest"] = all[i].destCity;
            res[i]["status"] = all[i].status;
        }
        return crow::response(res); });

    // 5. Update Status (Load/Deliver)
    CROW_ROUTE(app, "/api/update_pkg_status").methods(crow::HTTPMethod::Post)([&](const crow::request &req)
                                                                              {
        auto x = crow::json::load(req.body);
        int status = 0;
        string action = x["action"].s();
        if (action == "load") status = 1;      // LOADED
        else if (action == "deliver") status = 4; // DELIVERED
        else if (action == "return") status = 5;  // FAILED/RETURN
        
        appCore.updatePkgStatusSimple(x["id"].i(), status);
        return crow::response(200); });

    // --- SIMULATION ---
    CROW_ROUTE(app, "/api/next_shift").methods(crow::HTTPMethod::Post)([&]()
                                                                       {
        if(appCore.getRole() != Admin) return crow::response(403);
        // Run physics/logic step. Graph is passed to calculate new routes dynamically.
        vector<string> logs = appCore.runTimeStep(graph);
        
        crow::json::wvalue res;
        for(size_t i=0; i<logs.size(); i++) res["logs"][i] = logs[i];
        return crow::response(res); });

    // --- MAP & GRAPH UTILS ---
    CROW_ROUTE(app, "/api/map")
    ([&]()
     {
        crow::json::wvalue res;
        const auto& nodes = graph.getNodes();
        const auto& adj = graph.getAdjList();
        for (size_t i = 0; i < nodes.size(); i++) {
            res["nodes"][i]["id"] = nodes[i].id;
            res["nodes"][i]["name"] = nodes[i].name;
            res["nodes"][i]["x"] = nodes[i].x;
            res["nodes"][i]["y"] = nodes[i].y;
        }
        int edgeCount = 0;
        for (const auto& pair : adj) {
            int u = pair.first;
            for (const auto& edge : pair.second) {
                if (edge.to < u) continue;
                res["edges"][edgeCount]["source"] = u;
                res["edges"][edgeCount]["target"] = edge.to;
                res["edges"][edgeCount]["weight"] = edge.weight;
                res["edges"][edgeCount]["blocked"] = edge.isBlocked;
                edgeCount++;
            }
        }
        return res; });

    CROW_ROUTE(app, "/api/add_city").methods(crow::HTTPMethod::Post)([&](const crow::request &req)
                                                                     {
        auto x = crow::json::load(req.body);
        string msg = appCore.addCity(x["name"].s(), x["password"].s());
        refresh();
        crow::json::wvalue res; res["message"] = msg;
        return crow::response(res); });

    CROW_ROUTE(app, "/api/add_route").methods(crow::HTTPMethod::Post)([&](const crow::request &req)
                                                                      {
        auto x = crow::json::load(req.body);
        string msg = appCore.addRoute(x["key"].s(), x["distance"].i());
        refresh();
        crow::json::wvalue res; res["message"] = msg;
        return crow::response(res); });

    CROW_ROUTE(app, "/api/toggle_block").methods(crow::HTTPMethod::Post)([&](const crow::request &req)
                                                                         {
        auto x = crow::json::load(req.body);
        string msg = appCore.toggleRouteBlock(x["key"].s(), x["block"].b());
        refresh(); 
        crow::json::wvalue res; res["message"] = msg;
        return crow::response(res); });

    CROW_ROUTE(app, "/api/update_node").methods(crow::HTTPMethod::Post)([&](const crow::request &req)
                                                                        {
        auto x = crow::json::load(req.body);
        string name = x["name"].s();
        float newX = (float)x["x"].d();
        float newY = (float)x["y"].d();
        appCore.updateCityPosition(name, newX, newY);
        graph.updateNodePos(name, newX, newY);
        return crow::response(200); });

    // --- NEW RIDER ROUTES ---

    // Get list of riders for the manager's city
    CROW_ROUTE(app, "/api/get_riders")
    ([&]()
     {
        vector<Rider> riders = appCore.getRidersForManager();
        crow::json::wvalue res;
        for(size_t i=0; i<riders.size(); i++) {
            res[i]["id"] = riders[i].id;
            res[i]["username"] = riders[i].username;
            res[i]["vehicle"] = riders[i].vehicle;
        }
        return crow::response(res); });

    // Create a new rider
    CROW_ROUTE(app, "/api/add_rider").methods(crow::HTTPMethod::Post)([&](const crow::request &req)
                                                                      {
        auto x = crow::json::load(req.body);
        appCore.addRider(x["username"].s(), x["password"].s(), x["vehicle"].s());
        return crow::response(200); });

    // Assign packages to a rider
    CROW_ROUTE(app, "/api/assign_packages").methods(crow::HTTPMethod::Post)([&](const crow::request &req)
                                                                            {
        auto x = crow::json::load(req.body);
        string msg = appCore.assignPackagesToRider(x["riderId"].i());
        crow::json::wvalue res; res["message"] = msg;
        return crow::response(res); });

    // Get packages assigned to the logged-in rider
    CROW_ROUTE(app, "/api/rider_packages")
    ([&]()
     {
        vector<Package> pkgs = appCore.getPackagesForLoggedRider();
        crow::json::wvalue res;
        for(size_t i=0; i<pkgs.size(); i++) {
            res[i]["id"] = pkgs[i].id;
            res[i]["address"] = pkgs[i].address;
            res[i]["receiver"] = pkgs[i].receiver;
            res[i]["attempts"] = pkgs[i].attempts;
        }
        return crow::response(res); });

    // Handle Rider Actions (Delivered/Failed)
    CROW_ROUTE(app, "/api/rider_action").methods(crow::HTTPMethod::Post)([&](const crow::request &req)
                                                                         {
        auto x = crow::json::load(req.body);
        string msg = appCore.riderAction(x["id"].i(), x["action"].s());
        crow::json::wvalue res; res["message"] = msg;
        return crow::response(res); });

    app.port(8080).multithreaded().run();
}