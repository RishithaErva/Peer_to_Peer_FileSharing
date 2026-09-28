CC = gcc
CXX = g++
CXXFLAGS = -std=c++11 -Wall -pthread
CFLAGS = -Wall -pthread
LDFLAGS = -lstdc++ -lssl -lcrypto -pthread

TARGET = tracker
OBJS = main.o operations.o file_operations.o tracker_server.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

main.o: main.cpp operations.h tracker_server.h
	$(CXX) $(CXXFLAGS) -c main.cpp

operations.o: operations.cpp operations.h
	$(CXX) $(CXXFLAGS) -c operations.cpp

file_operations.o: file_operations.cpp file_operations.h
	$(CXX) $(CXXFLAGS) -c file_operations.cpp

tracker_server.o: tracker_server.cpp tracker_server.h operations.h file_operations.h
	$(CXX) $(CXXFLAGS) -c tracker_server.cpp

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all clean
