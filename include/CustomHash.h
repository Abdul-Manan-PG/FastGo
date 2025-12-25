#ifndef HASHROUTING_H // 1. Start of Include Guard
#define HASHROUTING_H

#include <iostream>
#include <vector>
#include <string>
#include <utility> // Required for std::pair

using namespace std;

// --- Class 1: Specialized Hash Table for Logistics ---

// Define the possible states of a slot
enum Status
{
    EMPTY,
    OCCUPIED,
    DELETED
};

// A struct to hold all the data for a single route
struct RouteEntry
{
    string key;
    int distance;
    bool isBlocked;
    Status status;
};

class hashroutes
{
private:
    int size;
    int count; // Track number of active items
    vector<RouteEntry> table;

public:
    // Constructor: Initialize vector with EMPTY status
    hashroutes() : size(97), count(0)
    {
        table.resize(size, {"", -1, false, EMPTY});
    }

    // Hash function
    int computekey(const string &key)
    {
        int sum = 0;
        for (char c : key)
        {
            sum += c;
        }
        return sum % size;
    }

    // Insert or Update a route
    bool insert(const string &key, int distance, bool isBlocked = false)
    {
        int computedkey = computekey(key);
        int firstDeletedIndex = -1;

        for (int i = 0; i < size; i++)
        {
            int index = (computedkey + i + (i * i)) % size; // Quadratic Probing

            // A. UPDATE existing key
            if (table[index].status == OCCUPIED && table[index].key == key)
            {
                table[index].distance = distance;
                table[index].isBlocked = isBlocked;
                return true;
            }

            // B. Track TOMBSTONE (DELETED) but keep searching
            if (table[index].status == DELETED)
            {
                if (firstDeletedIndex == -1)
                    firstDeletedIndex = index;
            }

            // C. Found EMPTY slot - Stop searching
            if (table[index].status == EMPTY)
            {
                // Recycle TOMBSTONE if found earlier
                if (firstDeletedIndex != -1)
                {
                    index = firstDeletedIndex;
                }

                table[index] = {key, distance, isBlocked, OCCUPIED};
                count++;
                return true;
            }
        }
        return false; // Table is full
    }

    // Delete a route (TOMBSTONE Logic)
    bool remove(const string &key)
    {
        int computedkey = computekey(key);

        for (int i = 0; i < size; i++)
        {
            int index = (computedkey + i + (i * i)) % size;

            if (table[index].status == EMPTY)
            {
                return false;
            }

            if (table[index].status == OCCUPIED && table[index].key == key)
            {
                table[index].status = DELETED; // Create Tombstone
                count--;
                return true;
            }
        }
        return false;
    }

    // Search and get details
    pair<int, bool> getRouteInfo(const string &key)
    {
        int computedkey = computekey(key);

        for (int i = 0; i < size; i++)
        {
            int index = (computedkey + i + (i * i)) % size;

            if (table[index].status == EMPTY)
                return {-1, false};

            if (table[index].status == OCCUPIED && table[index].key == key)
            {
                return {table[index].distance, table[index].isBlocked};
            }
        }
        return {-1, false};
    }

    // Block a route
    bool blockRoute(const string &key)
    {
        int computedkey = computekey(key);

        for (int i = 0; i < size; i++)
        {
            int index = (computedkey + i + (i * i)) % size;

            if (table[index].status == EMPTY)
                return false;

            if (table[index].status == OCCUPIED && table[index].key == key)
            {
                table[index].isBlocked = true;
                return true;
            }
        }
        return false;
    }

    // Unblock a route
    bool unblockRoute(const string &key)
    {
        int computedkey = computekey(key);

        for (int i = 0; i < size; i++)
        {
            int index = (computedkey + i + (i * i)) % size;

            if (table[index].status == EMPTY)
                return false;

            if (table[index].status == OCCUPIED && table[index].key == key)
            {
                table[index].isBlocked = false;
                return true;
            }
        }
        return false;
    }

    // Retrieve all active routes (for debugging or display)
    vector<RouteEntry> getAllRoutes() const
    {
        vector<RouteEntry> activeRoutes;
        for (const auto &entry : table)
        {
            if (entry.status == OCCUPIED)
            {
                activeRoutes.push_back(entry);
            }
        }
        return activeRoutes;
    }
};

// --- Class 2: Simple Hash Table (String-based) ---

struct City{
    string name;
    int point;
    string password;
};

class SimpleHash
{
private:
    int size;
    vector<City> table;

public:
    SimpleHash(int tableSize = 97) : size(tableSize)
    {
        table.resize(size, {"EMPTY", -1, ""});
    }

    int hashFunction(string key)
    {
        int sum = 0;
        for (char c : key)
        {
            sum += c;
        }
        return sum % size;
    }

    bool insert(string key, int value, string password)
    {
        int hashIndex = hashFunction(key);
        int firstDeleted = -1;

        for (int i = 0; i < size; i++)
        {
            int index = (hashIndex + (i * i)) % size;

            if (table[index].name == key)
            {
                table[index].point = value;
                table[index].password = password;
                return true;
            }

            if (table[index].name == "DELETED")
            {
                if (firstDeleted == -1)
                    firstDeleted = index;
            }

            if (table[index].name == "EMPTY")
            {
                if (firstDeleted != -1)
                    index = firstDeleted;
                table[index] = {key, value, password};
                return true;
            }
        }
        return false;
    }

    bool remove(string key)
    {
        int hashIndex = hashFunction(key);

        for (int i = 0; i < size; i++)
        {
            int index = (hashIndex + (i * i)) % size;

            if (table[index].name == "EMPTY")
                return false;

            if (table[index].name == key)
            {
                table[index].name = "DELETED";
                table[index].point = -1;
                return true;
            }
        }
        return false;
    }

    int getPoint(string key)
    {
        int hashIndex = hashFunction(key);

        for (int i = 0; i < size; i++)
        {
            int index = (hashIndex + (i * i)) % size;

            if (table[index].name == "EMPTY")
                return -1;

            if (table[index].name == key)
            {
                return table[index].point;
            }
        }
        return -1;
    }

    string getPassword(string key)
    {
        int hashIndex = hashFunction(key);

        for (int i = 0; i < size; i++)
        {
            int index = (hashIndex + (i * i)) % size;

            if (table[index].name == "EMPTY")
                return "-1";

            if (table[index].name == key)
            {
                return table[index].password;
            }
        }
        return "-1";
    }


    vector<City> getAll() {
        vector<City> activeData;
        for (const auto& entry : table) {
            // Filter out empty or deleted slots
            if (entry.name != "EMPTY" && entry.name != "DELETED") {
                activeData.push_back(entry);
            }
        }
        return activeData;
    }
};

#endif // 2. End of Include Guard