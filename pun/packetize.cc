#include "header/packetize.h"
#include <iostream>
#include <cstring>
using namespace std;

Segment *createSegment(void *payload,int part,int length){
    Segment *newSegment = new Segment;
    newSegment->header.srcPort = 0;
    newSegment->header.desPort = 0;
    newSegment->header.length = HEADER_SIZE + length;
    newSegment->header.seqNumber = part;
    newSegment->header.checkSum = 0;
    newSegment->payload = payload;
    return newSegment;
}

vector<Segment*> packetize(void *file,int fileSize,int limitByte){
    int part = (fileSize + limitByte - 1) / limitByte;
    vector<Segment*> segments(part);
    for(int p = 0;p < part;++p){
        int l_idx = p * limitByte;
        void *cur = (void*)((char*)file + l_idx);
        int length = (p == part - 1 && fileSize % limitByte ? fileSize % limitByte : limitByte);
        // cout << p << " " << length << endl;
        segments[p] = createSegment(cur,p,length);
        segments[p]->header.checkSum = calculateChecksum(segments[p]);
    }
    return segments;
}

pair<char*,int> resemble(vector<Segment*> &segments){
    int part = segments.size();
    int totalLength = part * MAX_PAYLOAD_SIZE;
    totalLength -= MAX_PAYLOAD_SIZE - (segments[part-1]->header.length - HEADER_SIZE);
    char *payload = new char[totalLength];
    char *curIdx = payload;
    for(int p = 0;p < part;++p){
        int curSize = segments[p]->header.length - HEADER_SIZE;
        memcpy(curIdx,segments[p]->payload,curSize);
        curIdx += curSize;
    }
    return pair<char*,int>(payload,totalLength);
}

