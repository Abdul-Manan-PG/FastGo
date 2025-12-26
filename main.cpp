#define CROW_MAIN
#include "include/crow_all.h"
#include "include/FastGo.h"
#include "include/CustomGraph.h"

int main() {
    crow::SimpleApp app;
    FastGo appCore; 
    Graph graph(appCore.getCities(), appCore.getRoutes());

    auto refresh = [&]() { 
        graph.refreshGraph();
        graph.syncToHash();       
        appCore.saveCitiesToDB(); 
    };

    // --- Static Files ---
    CROW_ROUTE(app, "/")([](const crow::request&, crow::response& res){
        res.set_static_file_info("static/index.html"); res.end();
    });
    CROW_ROUTE(app, "/<string>")([](const crow::request&, crow::response& res, string filename){
        res.set_static_file_info("static/" + filename); res.end();
    });

    // --- Core API ---
    CROW_ROUTE(app, "/api/login").methods(crow::HTTPMethod::Post)
    ([&](const crow::request& req){
        auto x = crow::json::load(req.body);
        if (!x) return crow::response(400);
        string resStr = appCore.login(x["username"].s(), x["password"].s());
        crow::json::wvalue res;
        if (resStr.find("Success") != string::npos) {
            res["status"] = "success";
            res["role"] = (appCore.getRole() == Admin) ? "admin" : "manager";
            res["city"] = appCore.getLoggedCity(); // Send city name back to JS
        } else { res["status"] = "error"; res["message"] = resStr; }
        return crow::response(res);
    });

    // --- Package APIs ---

    CROW_ROUTE(app, "/api/add_package").methods(crow::HTTPMethod::Post)
    ([&](const crow::request& req){
        auto x = crow::json::load(req.body);
        if(!x) return crow::response(400);
        appCore.createPackage(
            x["sender"].s(), x["receiver"].s(), x["address"].s(),
            x["dest"].s(), x["type"].i(), x["weight"].d()
        );
        return crow::response(200);
    });

    CROW_ROUTE(app, "/api/manager_packages")
    ([&](const crow::request& req){
        string city = req.url_params.get("city");
        vector<Package> pkgs = appCore.getPackagesForManager(city);
        
        crow::json::wvalue res;
        for (size_t i = 0; i < pkgs.size(); i++) {
            res[i]["id"] = pkgs[i].id;
            res[i]["sender"] = pkgs[i].sender;
            res[i]["dest"] = pkgs[i].destCity;
            res[i]["current"] = pkgs[i].currentCity;
            res[i]["status"] = pkgs[i].status;
            res[i]["type"] = pkgs[i].type;
        }
        return crow::response(res);
    });

    CROW_ROUTE(app, "/api/update_pkg_status").methods(crow::HTTPMethod::Post)
    ([&](const crow::request& req){
        auto x = crow::json::load(req.body);
        int id = x["id"].i();
        string action = x["action"].s(); // "load", "deliver", "return"
        
        if (action == "load") appCore.loadPackage(id);
        else if (action == "deliver") appCore.deliverPackage(id);
        else if (action == "return") appCore.returnPackage(id);
        
        return crow::response(200);
    });

    // --- Simulation API (Admin) ---

    CROW_ROUTE(app, "/api/next_shift").methods(crow::HTTPMethod::Post)
    ([&](){
        if(appCore.getRole() != Admin) return crow::response(403);
        // Pass the graph reference so packages can calculate paths
        vector<string> logs = appCore.runTimeStep(graph);
        
        crow::json::wvalue res;
        for(size_t i=0; i<logs.size(); i++) res["logs"][i] = logs[i];
        return crow::response(res);
    });

    CROW_ROUTE(app, "/api/city_packages")
    ([&](const crow::request& req){
        string city = req.url_params.get("city");
        vector<Package> all = appCore.getAllPackages();
        crow::json::wvalue res;
        int count = 0;
        for(const auto& p : all) {
            if(p.currentCity == city) {
                res[count]["id"] = p.id;
                res[count]["dest"] = p.destCity;
                res[count]["status"] = p.status;
                res[count]["type"] = p.type;
                count++;
            }
        }
        return crow::response(res);
    });

    // --- Existing Map APIs --- (Keep unchanged)
    CROW_ROUTE(app, "/api/add_city").methods(crow::HTTPMethod::Post)
    ([&](const crow::request& req){
        auto x = crow::json::load(req.body);
        string msg = appCore.addCity(x["name"].s(), x["password"].s());
        refresh();
        crow::json::wvalue res; res["message"] = msg;
        return crow::response(res);
    });
    CROW_ROUTE(app, "/api/add_route").methods(crow::HTTPMethod::Post)
    ([&](const crow::request& req){
        auto x = crow::json::load(req.body);
        string msg = appCore.addRoute(x["key"].s(), x["distance"].i());
        refresh();
        crow::json::wvalue res; res["message"] = msg;
        return crow::response(res);
    });
    CROW_ROUTE(app, "/api/toggle_block").methods(crow::HTTPMethod::Post)
    ([&](const crow::request& req){
        auto x = crow::json::load(req.body);
        string key = x["key"].s();
        bool shouldBlock = x["block"].b();
        string msg = appCore.toggleRouteBlock(key, shouldBlock);
        refresh(); 
        crow::json::wvalue res; res["message"] = msg;
        return crow::response(res);
    });
    CROW_ROUTE(app, "/api/update_node").methods(crow::HTTPMethod::Post)
    ([&](const crow::request& req){
        auto x = crow::json::load(req.body);
        appCore.updateCityPosition(x["name"].s(), (float)x["x"].d(), (float)x["y"].d());
        graph.updateNodePos(x["name"].s(), (float)x["x"].d(), (float)x["y"].d());
        return crow::response(200);
    });
    CROW_ROUTE(app, "/api/map")
    ([&](){
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
        return res;
    });
    CROW_ROUTE(app, "/api/path")
    ([&](const crow::request& req){
        string start = req.url_params.get("start");
        string end = req.url_params.get("end");
        auto result = graph.getShortestPath(start, end);
        crow::json::wvalue res;
        if (result.first != -1) {
            res["found"] = true;
            res["distance"] = result.first;
            for(size_t i = 0; i < result.second.size(); i++) res["path"][i] = result.second[i];
        } else { res["found"] = false; }
        return crow::response(res);
    });
    
    app.port(8080).multithreaded().run();
}