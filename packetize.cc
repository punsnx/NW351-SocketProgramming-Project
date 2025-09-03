#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
using namespace std;
#define HEADER_SIZE 12 //bytes
#define MAX_PAYLOAD_SIZE 1 //bytes
//UDP header
typedef struct _Header{
    unsigned short srcPort;
    unsigned short desPort;
    unsigned short length;
    unsigned short checkSum;
    unsigned int seqNumber;
}Header;

typedef struct _Segment{
    Header header;
    void *payload;
}Segment;

pair<char*,int> readFile(string fileName) {
    ifstream file(fileName);
    if(!file.is_open()){
        return pair<char*,int>(0,0);
    }
    file.seekg(0,ios::end);
    streamsize fsize = file.tellg();
    file.seekg(0,ios::beg);

    char *s = new char[fsize];
    file.read(s,fsize);
    
    return pair<char*,int>(s,fsize);
}

void writeFile(string FileName,char *payload,int fileSize){
    ofstream file(FileName);
    if(!file.is_open())return;
    file.write(payload,fileSize);
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


int main(){
    string fileName = "file.txt";
    pair<char*,int> res = readFile(fileName);
    if(!res.second)return 0;
    //garuntee file can open and have content length > 0
    char *s = res.first;
    int fileSize = res.second;
    cout << "FILE SIZE : " << fileSize << endl;
    cout << s << endl;

    vector<Segment*> segments = packetize((void*)s,fileSize,MAX_PAYLOAD_SIZE);
    for(auto seg : segments){
        int l = seg->header.length - HEADER_SIZE;
        char *payload = (char*)seg->payload;
        for(int i = 0;i < l;++i){
            cout << payload[i];
        }
        cout << endl;
    }
    res = resemble(segments);
    char *payload = res.first;
    fileSize = res.second;

    cout << "FILE SIZE : " << fileSize << endl;
    cout << payload << endl;

    writeFile("output.txt",payload,fileSize);
    
    return 0;
}