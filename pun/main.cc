#include <iostream>
#include "header/project-header.h"
using namespace std;

int main(){
    string fileName = "sample/file.txt";
    pair<char*,int> res = readFile(fileName);
    if(!res.second)return 0;
    //garuntee file can open and have content length > 0
    char *s = res.first;
    int fileSize = res.second;
    cout << "FILE SIZE : " << fileSize << endl;
    cout << s << endl;

    vector<Segment*> segments = packetize((void*)s,fileSize,MAX_PAYLOAD_SIZE);

    for(auto seg : segments){
        cout << "Segment " << seg->header.seqNumber << " checksum: " << seg->header.checkSum << endl;
        
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

    writeFile("sample/output.txt",payload,fileSize);
    return 0;
}


