CXX = g++
CXXFLAGS = -Wall -std=c++11

all: Server Client

Server: main/main_server.cpp ReliableUDPServer/UDPServer.cpp
	$(CXX) $(CXXFLAGS) -o ReliableUDPServer/Server \
	    main/main_server.cpp ReliableUDPServer/UDPServer.cpp

Client: main/main_client.cpp ReliableUDPClient/UDPClient.cpp
	$(CXX) $(CXXFLAGS) -o ReliableUDPClient/Client \
	    main/main_client.cpp ReliableUDPClient/UDPClient.cpp

clean:
	rm -f ReliableUDPServer/Server ReliableUDPClient/Client
