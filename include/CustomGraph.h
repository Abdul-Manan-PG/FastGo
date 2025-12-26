#ifndef CUSTOMGRAPH_H
#define CUSTOMGRAPH_H

#include "CustomHash.h"
#include <vector>
#include <map>
#include <string>
#include <cmath>
#include <limits>
#include <algorithm>
#include <queue> 

using namespace std;

const int INF = numeric_limits<int>::max();

struct Node { int id; string name; float x, y; };
struct Edge { int to; int weight; bool isBlocked; };

class Graph {
private:
    SimpleHash& cityRef;
    hashroutes& routeRef;
    map<int, string> idToName;
    map<string, int> nameToId;
    map<int, vector<Edge>> adjList;
    vector<Node> nodes;

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
        int count = cityData.size();
        float centerX = 400, centerY = 300, radius = 250;
        float angleStep = 2 * 3.14159f / (count > 0 ? count : 1);

        for (int i = 0; i < count; i++) {
            Node n; n.id = cityData[i].point; n.name = cityData[i].name;
            n.x = centerX + radius * cos(i * angleStep);
            n.y = centerY + radius * sin(i * angleStep);
            nodes.push_back(n);
            idToName[n.id] = n.name;
            nameToId[n.name] = n.id;
        }

        for (const auto& r : routeRef.getAllRoutes()) {
            pair<string, string> cities = parseRouteKey(r.key);
            if (nameToId.count(cities.first) && nameToId.count(cities.second)) {
                int u = nameToId[cities.first];
                int v = nameToId[cities.second];
                adjList[u].push_back({v, r.distance, r.isBlocked});
                adjList[v].push_back({u, r.distance, r.isBlocked});
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

    // --- Data Accessors for GUI ---
    const vector<Node>& getNodes() const { return nodes; }
    const map<int, vector<Edge>>& getAdjList() const { return adjList; }
    int getIdByName(string name) { return nameToId.count(name) ? nameToId[name] : -1; }
    Node getNodeById(int id) {
        for(auto& n : nodes) if(n.id == id) return n;
        return {0, "", 0, 0};
    }
};

#endif