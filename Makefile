# ------------------------------------------------
# Makefile for City Routes Logistics System
# ------------------------------------------------

# Compiler to use
CXX = g++

# Compiler Flags:
# -I include  : Tells compiler to look in the 'include' folder for .h files
# -Wall       : Turns on most warning messages (helps debugging)
# -std=c++11  : Ensures support for modern C++ features
# -g          : Adds debugging information (for gdb/debugger)
CXXFLAGS = -I include -Wall -std=c++11 -g

# Linker Flags:
# -lsqlite3   : Links the SQLite library
LDFLAGS = -lsqlite3

# The name of your final executable
# (Windows will automatically add .exe)
TARGET = app

# The source files
SRC = main.cpp

# ------------------------------------------------
# Rules
# ------------------------------------------------

# Default rule: runs when you type 'make'
all: $(TARGET)

# Rule to compile and link the application
$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

# Clean rule: runs when you type 'make clean'
# Removes the executable and the database (for a fresh start)
clean:
	rm -f $(TARGET) $(TARGET).exe city_routes.db