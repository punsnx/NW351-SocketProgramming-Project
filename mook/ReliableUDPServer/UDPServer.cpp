#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <fstream>
using namespace std;

struct Header {
    uint16_t srcPort;
    uint16_t destPort;
    uint16_t length;
    uint16_t checkSum;
    uint32_t seqNumber;

    Header(uint16_t s = 0, uint16_t d = 0) 
        : srcPort(s), destPort(d), length(0), checkSum(0), seqNumber(0) {}
};

struct Segment {
    Header header;
    bool ackFlag;
    uint32_t ackNumber;
    char data[1450];
};

int main(int argc, char *argv[]) {
    if(argc < 3){
        cerr << "Usage: ./Server <port> <advertised_window>" << endl;
        return 1;
    }

    int port = atoi(argv[1]);
    int windowSize = atoi(argv[2]);

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock < 0){ perror("socket"); return 1; }

    sockaddr_in serverAddr, clientAddr;
    socklen_t addrLen = sizeof(clientAddr);
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if(bind(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0){
        perror("bind"); return 1;
    }

    cout << "Server listening on port " << port << endl;

    Segment reqSeg{};
    int n = recvfrom(sock, &reqSeg, sizeof(reqSeg), 0, (sockaddr*)&clientAddr, &addrLen);
    if(n <= 0){ cerr << "Failed to receive request" << endl; return 1; }

    // ✅ ใช้ length จาก header ป้องกัน garbage
    string filename(reqSeg.data, reqSeg.header.length);
    cout << "Client requested file: " << filename << endl;

    ifstream file(filename, ios::binary);
    if(!file.is_open()){
        cerr << "Cannot open file: " << filename << endl;
        return 1;
    }

    uint32_t seq = 0;
    while(true){
        Segment seg{};
        file.read(seg.data, sizeof(seg.data));
        streamsize bytesRead = file.gcount();
        if(bytesRead <= 0) break;

        seg.header.seqNumber = seq;
        seg.header.length = bytesRead;

        sendto(sock, &seg, sizeof(seg), 0, (sockaddr*)&clientAddr, addrLen);
        seq++;

        // (optional: รอ ACK แต่เวอร์ชันนี้ยังไม่ได้ implement window/sliding)
    }

    cout << "File sent successfully!" << endl;
    file.close();
    close(sock);
    return 0;
}
