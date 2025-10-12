#ifndef PACKETIZE_H
#define PACKETIZE_H

#include <vector>
#include <utility>
#include "segment.h"
#include "checksum.h"
using namespace std;

Segment *createSegment(void *payload, int part, int length);
vector<Segment*> packetize(void *file, int fileSize, int limitByte);
pair<char*,int> resemble(vector<Segment*> &segments);

#endif