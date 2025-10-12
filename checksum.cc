#include "header/checksum.h"
#include <iostream>
using namespace std;

unsigned short calculateChecksum(Segment* segment) {
    unsigned int sum = 0;
    sum += segment->header.srcPort;
    sum += segment->header.desPort;
    sum += segment->header.length;
    sum += segment->header.seqNumber & 0xFFFF;
    sum += (segment->header.seqNumber >> 16) & 0xFFFF;

    int payloadSize = segment->header.length - HEADER_SIZE;
    unsigned short* payload = (unsigned short*)segment->payload;

    //sum payload
    int wordCount = payloadSize / 2;
    for (int i = 0; i < wordCount; i++) {
        sum += payload[i];
    }

    // Handle odd byte
    if (payloadSize % 2 == 1) {
        unsigned char* bytePayload = (unsigned char*)segment->payload;
        sum += bytePayload[payloadSize - 1] << 8;
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return ~sum;
}
