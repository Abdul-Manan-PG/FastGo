#ifndef GRAPH_H
#define GRAPH_H

#include "CustomHash.h"
#include <vector>
#include <map>
#include <string>
#include <limits>
#include <algorithm>
#include <queue>

using namespace std;

const int INF = numeric_limits<int>::max();

// Simplified Node: Removed physics velocity (vx, vy)
struct Node
{
    int id;
    string name;
    float x, y;
};

struct Edge
{
    int to;
    int weight;
    bool isBlocked;
};

class Graph
{
private:
    SimpleHash &cityRef;
    hashroutes &routeRef;
    map<int, string> idToName;
    map<string, int> nameToId;
    map<int, vector<Edge>> adjList;
    vector<Node> nodes;

    // Screen Dimensions (Used only for centering new nodes)
    const float WIDTH = 1000.0f;
    const float HEIGHT = 800.0f;

    pair<string, string> parseRouteKey(string key)
    {
        size_t dash = key.find('-');
        if (dash != string::npos)
            return {key.substr(0, dash), key.substr(dash + 1)};
        return {"", ""};
    }

public:
    Graph(SimpleHash &cities, hashroutes &routes) : cityRef(cities), routeRef(routes)
    {
        refreshGraph();
    }

    // Completely non-physics refresh
    // Completely non-physics refresh
    void refreshGraph()
    {
        nodes.clear();
        adjList.clear();
        idToName.clear();
        nameToId.clear();
        vector<City> cityData = cityRef.getAll();

        for (const auto &c : cityData)
        {
            Node n;
            n.id = c.point;
            n.name = c.name;

            // LOGIC CHANGE:
            // If coordinates exist in DB, use them.
            if (c.x != 0.0f && c.y != 0.0f)
            {
                n.x = c.x;
                n.y = c.y;
            }
            else
            {
                // FIX: Randomize position for new/unsaved nodes
                // so they don't stack on top of each other.
                n.x = static_cast<float>(rand() % (int)WIDTH);
                n.y = static_cast<float>(rand() % (int)HEIGHT);
            }

            nodes.push_back(n);
            idToName[n.id] = n.name;
            nameToId[n.name] = n.id;
        }

        // Build Connections (Edges)
        for (const auto &r : routeRef.getAllRoutes())
        {
            pair<string, string> cities = parseRouteKey(r.key);
            if (nameToId.count(cities.first) && nameToId.count(cities.second))
            {
                int u = nameToId[cities.first];
                int v = nameToId[cities.second];
                adjList[u].push_back({v, r.distance, r.isBlocked});
                adjList[v].push_back({u, r.distance, r.isBlocked});
            }
        }

        // No physics optimization called here. Nodes are static.
    }

    // Save current positions (X,Y) to the Database Cache
    void syncToHash()
    {
        for (const auto &n : nodes)
        {
            cityRef.updatePosition(n.name, n.x, n.y);
        }
    }

    // Update a single node's position (Called when dragging drops)
    void updateNodePos(string name, float x, float y)
    {
        if (nameToId.count(name))
        {
            int id = nameToId[name];
            for (auto &n : nodes)
            {
                if (n.id == id)
                {
                    n.x = x;
                    n.y = y;
                    break;
                }
            }
        }
    }

    // --- Pathfinding & Helpers (Unchanged) ---

    pair<int, vector<string>> getShortestPath(string startCity, string endCity)
    {
        if (!nameToId.count(startCity) || !nameToId.count(endCity))
            return {-1, {}};
        int start = nameToId[startCity], end = nameToId[endCity];
        map<int, int> dist;
        map<int, int> parent;
        for (const auto &node : nodes)
            dist[node.id] = INF;

        dist[start] = 0;
        parent[start] = -1;
        priority_queue<pair<int, int>, vector<pair<int, int>>, greater<pair<int, int>>> pq;
        pq.push({0, start});

        while (!pq.empty())
        {
            int d = pq.top().first;
            int u = pq.top().second;
            pq.pop();
            if (d > dist[u])
                continue;
            if (u == end)
                break;
            for (const auto &edge : adjList[u])
            {
                if (edge.isBlocked)
                    continue;
                if (dist[u] + edge.weight < dist[edge.to])
                {
                    dist[edge.to] = dist[u] + edge.weight;
                    parent[edge.to] = u;
                    pq.push({dist[edge.to], edge.to});
                }
            }
        }
        if (dist[end] == INF)
            return {-1, {}};
        vector<string> path;
        for (int v = end; v != -1; v = parent[v])
            path.push_back(idToName[v]);
        reverse(path.begin(), path.end());
        return {dist[end], path};
    }

    string getNextHop(string currentCity, string destCity)
    {
        if (currentCity == destCity)
            return currentCity;
        pair<int, vector<string>> result = getShortestPath(currentCity, destCity);
        if (result.first != -1 && result.second.size() >= 2)
        {
            return result.second[1];
        }
        return "";
    }

    const vector<Node> &getNodes() const { return nodes; }
    const map<int, vector<Edge>> &getAdjList() const { return adjList; }
};

#endif