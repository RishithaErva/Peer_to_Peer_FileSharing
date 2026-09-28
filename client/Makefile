CXX = g++
CXXFLAGS = -std=c++11 -Wall -pthread
LDFLAGS = -lssl -lcrypto -pthread

TARGET = client
OBJS = main.o tracker_connector.o file_utils.o download_manager.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

main.o: main.cpp tracker_connector.h file_utils.h download_manager.h
	$(CXX) $(CXXFLAGS) -c main.cpp

tracker_connector.o: tracker_connector.cpp tracker_connector.h
	$(CXX) $(CXXFLAGS) -c tracker_connector.cpp

file_utils.o: file_utils.cpp file_utils.h
	$(CXX) $(CXXFLAGS) -c file_utils.cpp

download_manager.o: download_manager.cpp download_manager.h file_utils.h
	$(CXX) $(CXXFLAGS) -c download_manager.cpp

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all clean
