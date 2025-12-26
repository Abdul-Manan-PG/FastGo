CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2
LDFLAGS = -lsqlite3 -lws2_32 -lmswsock

TARGET = FastGoServer.exe
SRC = main.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

clean:
	del $(TARGET)