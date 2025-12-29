#ifndef CUSTOM_MAP_H
#define CUSTOM_MAP_H

#include <iostream>
#include <vector>
#include <list>
#include <map>
#include <string>
using namespace std;

template <typename T>
class CustomMap {
private:
    // Adjacency List: Maps a vertex to a list of pairs (neighbor, weight)
    map<T, list<pair<T, int>>> adjList;
    bool isDirected;

public:
    
    CustomMap(bool directed = false) : isDirected(directed) {}

    void addVertex(T vertex) {
        if (adjList.find(vertex) == adjList.end()) {
            adjList[vertex] = std::list<std::pair<T, int>>();
        }
    }

    void addEdge(T src, T dest, int weight = 1) {
        addVertex(src);
        addVertex(dest);

        adjList[src].push_back({dest, weight});
        
        if (!isDirected) {
            adjList[dest].push_back({src, weight});
        }
    }

    void displayMap() const {
        for (const auto& pair : adjList) {
            std::cout << pair.first << " is connected to: " << std::endl;
            for (const auto& edge : pair.second) {
                std::cout << "  -> " << edge.first << " (weight: " << edge.second << ")" << std::endl;
            }
            std::cout << "---" << std::endl;
        }
    }

    std::list<std::pair<T, int>> getNeighbors(T vertex) {
        if (adjList.find(vertex) != adjList.end()) {
            return adjList[vertex];
        }
        return {};
    }
};

#endif