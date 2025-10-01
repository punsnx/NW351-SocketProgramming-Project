#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>

#include "project-header.h"      // expects Header, Segment, MAX_PAYLOAD_SIZE, HEADER_SIZE
#include "packetize.h"           // for createSegment()
#include "checksum.h"            // for calculateChecksum()