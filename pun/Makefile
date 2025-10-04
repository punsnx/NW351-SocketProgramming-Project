CC = g++
CFLAGS = -Wall -std=c++11

# SERVER
SERVER_TARGET = server.out
SERVER_SOURCES = server-newlife.cc file-handler.cc checksum.cc packetize.cc

# CLIENT
CLIENT_TARGET = client.out
CLIENT_SOURCES = client.cc file-handler.cc checksum.cc packetize.cc

.PHONY: all server client clean

all: server client

server: $(SERVER_SOURCES)
	$(CC) $(CFLAGS) $(SERVER_SOURCES) -o $(SERVER_TARGET)

client: $(CLIENT_SOURCES)
	$(CC) $(CFLAGS) $(CLIENT_SOURCES) -o $(CLIENT_TARGET)

clean:
	rm -f $(SERVER_TARGET) $(CLIENT_TARGET)