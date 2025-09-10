# CXX = g++
# CXXFLAGS = -Wall -std=c++11

# all: Server Client

# Server: main/main_server.cpp ReliableUDPServer/UDPServer.cpp
# 	$(CXX) $(CXXFLAGS) -o ReliableUDPServer/Server \
# 	    main/main_server.cpp ReliableUDPServer/UDPServer.cpp

# Client: main/main_client.cpp ReliableUDPClient/UDPClient.cpp
# 	$(CXX) $(CXXFLAGS) -o ReliableUDPClient/Client \
# 	    main/main_client.cpp ReliableUDPClient/UDPClient.cpp

# clean:
# 	rm -f ReliableUDPServer/Server ReliableUDPClient/Client

CXX := g++
CXXFLAGS := -Wall -std=c++11
INCLUDES := -Iheader

SRV_BIN := ReliableUDPServer/server
CLI_BIN := ReliableUDPClient/client

SRV_SRC := main/main_server.cpp ReliableUDPServer/UDPServer.cpp
CLI_SRC := main/main_client.cpp ReliableUDPClient/UDPClient.cpp

.PHONY: all clean dirs run-server run-client

all: dirs $(SRV_BIN) $(CLI_BIN)

dirs:
	@mkdir -p ReliableUDPServer ReliableUDPClient

# build server -> ReliableUDPServer/server
$(SRV_BIN): $(SRV_SRC) header/UDPServer.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(SRV_SRC)

# build client -> ReliableUDPClient/client
$(CLI_BIN): $(CLI_SRC) header/UDPClient.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $(CLI_SRC)

clean:
	rm -f $(SRV_BIN) $(CLI_BIN) \
	      ReliableUDPServer/Server ReliableUDPClient/Client \
	      server client

