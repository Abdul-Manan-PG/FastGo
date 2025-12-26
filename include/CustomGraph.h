#ifndef GRAPH_H
#define GRAPH_H

#include "CustomHash.h"
#include <vector>
#include <map>
#include <string>
#include <cmath>
#include <limits>
#include <algorithm>
#include <queue> 
#include <cstdlib> // For rand()

using namespace std;

const int INF = numeric_limits<int>::max();

// Added vx, vy for physics calculation
struct Node { 
    int id; 
    string name; 
    float x, y; 
    float vx, vy; 
};

struct Edge { int to; int weight; bool isBlocked; };

class Graph {
private:
    SimpleHash& cityRef;
    hashroutes& routeRef;
    map<int, string> idToName;
    map<string, int> nameToId;
    map<int, vector<Edge>> adjList;
    vector<Node> nodes;

    // Physics Constants
    const float WIDTH = 1000.0f;
    const float HEIGHT = 800.0f;
    const float REPULSION = 50000.0f;
    const float STIFFNESS = 0.05f;
    const float DAMPING = 0.9f;
    const float SCALING = 0.2f; // Scale distance (km) to pixels

    pair<string, string> parseRouteKey(string key) {
        size_t dash = key.find('-');
        if (dash != string::npos) return {key.substr(0, dash), key.substr(dash + 1)};
        return {"", ""};
    }

public:
    Graph(SimpleHash& cities, hashroutes& routes) : cityRef(cities), routeRef(routes) {
        refreshGraph();
    }

    void refreshGraph() {
        nodes.clear(); adjList.clear(); idToName.clear(); nameToId.clear();
        vector<City> cityData = cityRef.getAll();
        
        // 1. Initialize Nodes with Random Positions
        for (const auto& c : cityData) {
            Node n; 
            n.id = c.point; 
            n.name = c.name;
            n.x = static_cast<float>(rand() % (int)WIDTH); 
            n.y = static_cast<float>(rand() % (int)HEIGHT);
            n.vx = 0; n.vy = 0;
            nodes.push_back(n);
            idToName[n.id] = n.name;
            nameToId[n.name] = n.id;
        }

        // 2. Build Adjacency List
        for (const auto& r : routeRef.getAllRoutes()) {
            pair<string, string> cities = parseRouteKey(r.key);
            if (nameToId.count(cities.first) && nameToId.count(cities.second)) {
                int u = nameToId[cities.first];
                int v = nameToId[cities.second];
                adjList[u].push_back({v, r.distance, r.isBlocked});
                adjList[v].push_back({u, r.distance, r.isBlocked});
            }
        }

        // 3. Pre-Calculate Layout (The "Fast Loading" Magic)
        optimizeLayout(1000); // Run 1000 physics steps instantly
    }

    // Server-Side Physics Calculation
    void optimizeLayout(int iterations) {
        for (int iter = 0; iter < iterations; iter++) {
            // A. Repulsion (Nodes push apart)
            for (size_t i = 0; i < nodes.size(); i++) {
                for (size_t j = 0; j < nodes.size(); j++) {
                    if (i == j) continue;
                    float dx = nodes[i].x - nodes[j].x;
                    float dy = nodes[i].y - nodes[j].y;
                    float distSq = dx*dx + dy*dy;
                    if (distSq < 0.1f) distSq = 0.1f;
                    float dist = sqrt(distSq);
                    
                    float force = REPULSION / distSq;
                    nodes[i].vx += (dx / dist) * force;
                    nodes[i].vy += (dy / dist) * force;
                }
            }

            // B. Attraction (Edges pull together based on distance)
            for (auto& pair : adjList) {
                int u_idx = -1;
                for(size_t i=0; i<nodes.size(); i++) if(nodes[i].id == pair.first) u_idx = i;
                
                if (u_idx == -1) continue;

                for (const auto& edge : pair.second) {
                    int v_idx = -1;
                    for(size_t i=0; i<nodes.size(); i++) if(nodes[i].id == edge.to) v_idx = i;
                    if (v_idx == -1) continue;

                    float dx = nodes[v_idx].x - nodes[u_idx].x;
                    float dy = nodes[v_idx].y - nodes[u_idx].y;
                    float dist = sqrt(dx*dx + dy*dy);
                    if(dist < 0.1f) dist = 0.1f;

                    // Target length is proportional to real km distance
                    float targetLen = edge.weight * SCALING; 
                    float force = (dist - targetLen) * STIFFNESS;

                    float fx = (dx / dist) * force;
                    float fy = (dy / dist) * force;

                    nodes[u_idx].vx += fx; nodes[u_idx].vy += fy;
                    nodes[v_idx].vx -= fx; nodes[v_idx].vy -= fy;
                }
            }

            // C. Update Positions & Center Gravity
            for (auto& n : nodes) {
                n.x += n.vx;
                n.y += n.vy;
                n.vx *= DAMPING;
                n.vy *= DAMPING;

                // Pull towards center to prevent flying away
                n.x += (WIDTH/2 - n.x) * 0.01f;
                n.y += (HEIGHT/2 - n.y) * 0.01f;
            }
        }
    }

    pair<int, vector<string>> getShortestPath(string startCity, string endCity) {
        if (!nameToId.count(startCity) || !nameToId.count(endCity)) return {-1, {}};
        int start = nameToId[startCity], end = nameToId[endCity];
        map<int, int> dist; map<int, int> parent;
        for (const auto& node : nodes) dist[node.id] = INF;

        dist[start] = 0; parent[start] = -1;
        priority_queue<pair<int, int>, vector<pair<int, int>>, greater<pair<int, int>>> pq;
        pq.push({0, start});

        while (!pq.empty()) {
            int d = pq.top().first; int u = pq.top().second; pq.pop();
            if (d > dist[u]) continue;
            if (u == end) break;
            for (const auto& edge : adjList[u]) {
                if (edge.isBlocked) continue;
                if (dist[u] + edge.weight < dist[edge.to]) {
                    dist[edge.to] = dist[u] + edge.weight;
                    parent[edge.to] = u;
                    pq.push({dist[edge.to], edge.to});
                }
            }
        }
        if (dist[end] == INF) return {-1, {}};
        vector<string> path;
        for (int v = end; v != -1; v = parent[v]) path.push_back(idToName[v]);
        reverse(path.begin(), path.end());
        return {dist[end], path};
    }

    const vector<Node>& getNodes() const { return nodes; }
    const map<int, vector<Edge>>& getAdjList() const { return adjList; }
};

#endif