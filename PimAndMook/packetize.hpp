#pragma once
#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
using namespace std;
#define HEADER_SIZE 12
#define MAX_PAYLOAD_SIZE 1         
//#define MAX_PAYLOAD_SIZE 1450
//UDP header
#pragma pack(push, 1)
typedef struct _Header {
    unsigned short srcPort;
    unsigned short desPort;
    unsigned short length;    
    unsigned short checkSum;  
    unsigned int seqNumber;
} Header;
#pragma pack(pop)

typedef struct _Segment {
    Header header;
    void *payload;  
}Segment;

pair<char*,int> readFile(string fileName) {
    ifstream file(fileName);
    //ifstream file(fileName, ios::binary);
    if (!file.is_open()) 
        //return {nullptr, 0};
        return pair<char*,int>(0,0);
    file.seekg(0,ios::end);
    streamsize fsize = file.tellg();
    file.seekg(0,ios::beg);
    char* s = (fsize > 0) ? new char[fsize] : nullptr;
    if (fsize > 0) file.read(s, fsize);
     return pair<char*,int>(s,fsize);
}

bool writeFile(string fileName,char *payload,int fileSize){
    //ofstream file(fileName, ios::binary);
    ofstream file(fileName);
    if (!file.is_open()) return false;
    if (fileSize > 0) file.write(payload, fileSize);
    return true;
}

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
