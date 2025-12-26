#define CROW_MAIN
#include "include/crow_all.h"
#include "include/FastGo.h"
#include "include/CustomGraph.h"

int main() {
    crow::SimpleApp app;

    // --- System Core ---
    FastGo appCore; 
    Graph graph(appCore.getCities(), appCore.getRoutes());

    // --- REFRESH LAMBDA (DEFINED ONLY ONCE) ---
    auto refresh = [&]() { 
        graph.refreshGraph();
        graph.syncToHash();       
        appCore.saveCitiesToDB(); 
    };

    // --- STATIC FILE SERVING ---
    CROW_ROUTE(app, "/")
    ([](const crow::request&, crow::response& res){
        res.set_static_file_info("static/index.html");
        res.end();
    });

    CROW_ROUTE(app, "/<string>")
    ([](const crow::request&, crow::response& res, string filename){
        res.set_static_file_info("static/" + filename);
        res.end();
    });

    // --- API ENDPOINTS ---

    // Login
    CROW_ROUTE(app, "/api/login").methods(crow::HTTPMethod::Post)
    ([&](const crow::request& req){
        auto x = crow::json::load(req.body);
        if (!x) return crow::response(400, "Invalid JSON");
        string user = x["username"].s();
        string pass = x["password"].s();
        string result = appCore.login(user, pass);
        
        crow::json::wvalue res;
        if (result.find("Success") != string::npos) {
            res["status"] = "success";
            res["role"] = (appCore.getRole() == Admin) ? "admin" : "manager";
        } else {
            res["status"] = "error";
            res["message"] = result;
        }
        return crow::response(res);
    });

    // Add City
    CROW_ROUTE(app, "/api/add_city").methods(crow::HTTPMethod::Post)
    ([&](const crow::request& req){
        auto x = crow::json::load(req.body);
        if (!x) return crow::response(400);
        string msg = appCore.addCity(x["name"].s(), x["password"].s());
        refresh(); // Uses the new refresh logic
        crow::json::wvalue res; res["message"] = msg;
        return crow::response(res);
    });

    // Add Route
    CROW_ROUTE(app, "/api/add_route").methods(crow::HTTPMethod::Post)
    ([&](const crow::request& req){
        auto x = crow::json::load(req.body);
        if (!x) return crow::response(400);
        string msg = appCore.addRoute(x["key"].s(), x["distance"].i());
        refresh(); 
        crow::json::wvalue res; res["message"] = msg;
        return crow::response(res);
    });

    // Get Map
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

    // Get Path
    CROW_ROUTE(app, "/api/path")
    ([&](const crow::request& req){
        string start = req.url_params.get("start");
        string end = req.url_params.get("end");
        if (start.empty() || end.empty()) return crow::response(400);

        auto result = graph.getShortestPath(start, end);
        crow::json::wvalue res;
        if (result.first != -1) {
            res["found"] = true;
            res["distance"] = result.first;
            for(size_t i = 0; i < result.second.size(); i++) res["path"][i] = result.second[i];
        } else {
            res["found"] = false;
        }
        return crow::response(res);
    });

    // Traffic Control (Block/Unblock)
    CROW_ROUTE(app, "/api/toggle_block").methods(crow::HTTPMethod::Post)
    ([&](const crow::request& req){
        auto x = crow::json::load(req.body);
        if (!x) return crow::response(400);

        string key = x["key"].s();
        bool shouldBlock = x["block"].b();

        string msg = appCore.toggleRouteBlock(key, shouldBlock);
        refresh(); 

        crow::json::wvalue res; 
        res["message"] = msg;
        return crow::response(res);
    });

    // --- NEW: Drag & Drop Update Endpoint ---
    CROW_ROUTE(app, "/api/update_node").methods(crow::HTTPMethod::Post)
    ([&](const crow::request& req){
        auto x = crow::json::load(req.body);
        if (!x) return crow::response(400);

        string name = x["name"].s();
        double newX = x["x"].d();
        double newY = x["y"].d();

        // 1. Save to DB
        appCore.updateCityPosition(name, (float)newX, (float)newY);
        
        // 2. Update live Graph memory
        graph.updateNodePos(name, (float)newX, (float)newY);

        return crow::response(200);
    });

    app.port(8080).multithreaded().run();
}