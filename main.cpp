#include <iostream>
#include <limits> // For numeric limits
#include "include/FastGo.h"

using namespace std;

// Helper to clear input buffer
void clearInput() {
    cin.clear();
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

void adminMenu(FastGo& app) {
    int choice;
    do {
        cout << "\n=== ADMIN DASHBOARD ===" << endl;
        cout << "1. Add New City (Manager)" << endl;
        cout << "2. Add New Route" << endl;
        cout << "3. Block/Unblock Route" << endl;
        cout << "4. Logout" << endl;
        cout << "Enter choice: ";
        cin >> choice;

        if (choice == 1) {
            string name, pass;
            cout << "Enter City Name: ";
            cin >> name;
            cout << "Enter Password for Manager: ";
            cin >> pass;
            
            // This will auto-assign the Point ID (0, 1, 2...) inside FastGo.h
            app.addCity(name, pass);

        } else if (choice == 2) {
            string key;
            int dist;
            cout << "Enter Route Key (e.g., A-B): ";
            cin >> key;
            cout << "Enter Distance: ";
            cin >> dist;
            
            app.addRoute(key, dist);

        } else if (choice == 3) {
            string key;
            int action;
            cout << "Enter Route Key: ";
            cin >> key;
            cout << "1. Block | 0. Unblock: ";
            cin >> action;
            
            app.toggleRouteBlock(key, (action == 1));

        } else if (choice != 4) {
            cout << "Invalid choice." << endl;
        }

    } while (choice != 4);
    cout << "Logged out." << endl;
}

void managerMenu(FastGo& app, string cityName) {
    cout << "\n=== MANAGER DASHBOARD (" << cityName << ") ===" << endl;
    cout << "You are successfully logged in as a Manager." << endl;
    cout << "Point ID: " << app.getCities().getPoint(cityName) << endl;
    
    // Placeholder for future manager tasks
    cout << "\nPress Enter to logout...";
    clearInput(); 
    cin.get();
}

int main() {
    FastGo app; // Load Database automatically

    int choice;
    while (true) {
        cout << "\n--- MAIN LOGIN ---" << endl;
        cout << "1. Login" << endl;
        cout << "2. Exit" << endl;
        cout << "Enter choice: ";
        cin >> choice;

        if (choice == 1) {
            string user, pass;
            cout << "Username (or City Name): ";
            cin >> user;
            cout << "Password: ";
            cin >> pass;

            if (app.login(user, pass)) {
                if (app.getRole() == Admin) {
                    adminMenu(app);
                } else if (app.getRole() == Manager) {
                    managerMenu(app, user);
                }
            } 
        } else if (choice == 2) {
            break;
        } else {
            cout << "Invalid choice." << endl;
            clearInput();
        }
    }

    return 0;
}