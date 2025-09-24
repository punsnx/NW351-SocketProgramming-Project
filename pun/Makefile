CC = g++
CFLAGS = -Wall -std=c++11
TARGET = a.out
SOURCES = server.cc file-handler.cc checksum.cc packetize.cc

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES) -o $(TARGET)

# clean:
# 	rm -f $(TARGET)

# .PHONY: clean