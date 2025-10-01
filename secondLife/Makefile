# Makefile for NW351 Socket Programming Project

CXX = g++
CXXFLAGS = -Wall -std=c++11 -I./header

# Client
CLIENT_SRC = client.cc packetize.cc fileio.cc checksum.cc
CLIENT_OBJ = $(CLIENT_SRC:.cc=.o)
CLIENT_TARGET = client

# Server
SERVER_SRC = server.cc packetize.cc fileio.cc checksum.cc
SERVER_OBJ = $(SERVER_SRC:.cc=.o)
SERVER_TARGET = server

all: $(CLIENT_TARGET) $(SERVER_TARGET)

# Build client
$(CLIENT_TARGET): $(CLIENT_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Build server
$(SERVER_TARGET): $(SERVER_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Compile object files
%.o: %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean
clean:
	rm -f $(CLIENT_OBJ) $(CLIENT_TARGET) $(SERVER_OBJ) $(SERVER_TARGET)
