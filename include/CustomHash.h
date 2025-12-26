#ifndef HASHROUTING_H 
#define HASHROUTING_H

#include <vector>
#include <string>

using namespace std;

enum Status { EMPTY, OCCUPIED, DELETED };

struct RouteEntry {
    string key;
    int distance;
    bool isBlocked;
    Status status;
};

class hashroutes {
private:
    int size;
    int count; 
    vector<RouteEntry> table;

public:
    hashroutes() : size(97), count(0) {
        table.resize(size, {"", -1, false, EMPTY});
    }

    int computekey(const string &key) const {
        int sum = 0;
        for (char c : key) sum += c;
        return sum % size;
    }

    bool insert(const string &key, int distance, bool isBlocked = false) {
        int computedkey = computekey(key);
        int firstDeletedIndex = -1;

        for (int i = 0; i < size; i++) {
            int index = (computedkey + i + (i * i)) % size; 

            if (table[index].status == OCCUPIED && table[index].key == key) {
                table[index].distance = distance;
                table[index].isBlocked = isBlocked;
                return true;
            }
            if (table[index].status == DELETED && firstDeletedIndex == -1) firstDeletedIndex = index;
            if (table[index].status == EMPTY) {
                if (firstDeletedIndex != -1) index = firstDeletedIndex;
                table[index] = {key, distance, isBlocked, OCCUPIED};
                count++;
                return true;
            }
        }
        return false; 
    }

    bool blockRoute(const string &key) { return updateBlockStatus(key, true); }
    bool unblockRoute(const string &key) { return updateBlockStatus(key, false); }

    bool updateBlockStatus(const string &key, bool status) {
        int computedkey = computekey(key);
        for (int i = 0; i < size; i++) {
            int index = (computedkey + i + (i * i)) % size;
            if (table[index].status == EMPTY) return false;
            if (table[index].status == OCCUPIED && table[index].key == key) {
                table[index].isBlocked = status;
                return true;
            }
        }
        return false;
    }

    vector<RouteEntry> getAllRoutes() const {
        vector<RouteEntry> activeRoutes;
        for (const auto &entry : table) {
            if (entry.status == OCCUPIED) activeRoutes.push_back(entry);
        }
        return activeRoutes;
    }
};

struct City {
    string name;
    int point;
    string password;
    float x; // NEW
    float y; // NEW
};

class SimpleHash {
private:
    int size;
    vector<City> table;

public:
    SimpleHash(int tableSize = 97) : size(tableSize) {
        table.resize(size, {"EMPTY", -1, ""});
    }

    int hashFunction(const string& key) const {
        int sum = 0;
        for (char c : key) sum += c;
        return sum % size;
    }

    // 1. UPDATE: Insert now takes x and y (default to 0.0f)
    bool insert(string key, int value, string password, float x = 0.0f, float y = 0.0f) {
        int hashIndex = hashFunction(key);
        int firstDeleted = -1;

        for (int i = 0; i < size; i++) {
            int index = (hashIndex + (i * i)) % size;
            if (table[index].name == key) {
                table[index].point = value;
                table[index].password = password;
                // Don't overwrite X/Y with 0 if it already exists and input is 0
                if (x != 0.0f) table[index].x = x;
                if (y != 0.0f) table[index].y = y;
                return true;
            }
            if (table[index].name == "DELETED" && firstDeleted == -1) firstDeleted = index;
            if (table[index].name == "EMPTY") {
                if (firstDeleted != -1) index = firstDeleted;
                table[index] = {key, value, password, x, y};
                return true;
            }
        }
        return false;
    }

    // 2. NEW: Method to update coordinates specifically
    void updatePosition(string key, float x, float y) {
        int hashIndex = hashFunction(key);
        for (int i = 0; i < size; i++) {
            int index = (hashIndex + (i * i)) % size;
            if (table[index].name == "EMPTY") return;
            if (table[index].name == key) {
                table[index].x = x;
                table[index].y = y;
                return;
            }
        }
    }

    int getPoint(string key) {
        int hashIndex = hashFunction(key);
        for (int i = 0; i < size; i++) {
            int index = (hashIndex + (i * i)) % size;
            if (table[index].name == "EMPTY") return -1;
            if (table[index].name == key) return table[index].point;
        }
        return -1;
    }

    string getPassword(string key) {
        int hashIndex = hashFunction(key);
        for (int i = 0; i < size; i++) {
            int index = (hashIndex + (i * i)) % size;
            if (table[index].name == "EMPTY") return "-1";
            if (table[index].name == key) return table[index].password;
        }
        return "-1";
    }

    vector<City> getAll() {
        vector<City> activeData;
        for (const auto& entry : table) {
            if (entry.name != "EMPTY" && entry.name != "DELETED") activeData.push_back(entry);
        }
        return activeData;
    }
};

#endif