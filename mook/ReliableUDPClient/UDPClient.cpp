#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
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
    if(argc < 5){
        cerr << "Usage: ./Client <server_ip> <port> <filename> <advertised_window>" << endl;
        return 1;
    }

    const char* serverIP = argv[1];
    int port = atoi(argv[2]);
    const char* filename = argv[3];
    int windowSize = atoi(argv[4]);

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock < 0){ perror("socket"); return 1; }

    sockaddr_in serverAddr;
    socklen_t addrLen = sizeof(serverAddr);
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    inet_pton(AF_INET, serverIP, &serverAddr.sin_addr);
    serverAddr.sin_port = htons(port);

    // --- สร้าง request ---
    Segment request{};
    strncpy(request.data, filename, sizeof(request.data)-1);
    request.data[sizeof(request.data)-1] = '\0';   // ✅ บังคับจบ string
    request.ackFlag = false;
    request.header.seqNumber = 0;
    request.header.length = strlen(request.data);

    sendto(sock, &request, sizeof(request), 0, (sockaddr*)&serverAddr, addrLen);

    ofstream outFile(filename, ios::binary);
    if(!outFile.is_open()){ cerr << "Cannot open file to write!" << endl; return 1; }

    uint32_t expectedSeq = 0;
    while(true){
        Segment seg;
        int n = recvfrom(sock, &seg, sizeof(seg), 0, (sockaddr*)&serverAddr, &addrLen);
        if(n <= 0) break;

        if(seg.header.seqNumber == expectedSeq) {
            outFile.write(seg.data, seg.header.length);
            expectedSeq++;

            // ส่ง ACK
            Segment ack{};
            ack.header.srcPort = port;
            ack.header.destPort = ntohs(serverAddr.sin_port);
            ack.header.seqNumber = seg.header.seqNumber;
            ack.ackNumber = expectedSeq;
            ack.ackFlag = true;
            ack.header.length = 0;
            sendto(sock, &ack, sizeof(ack), 0, (sockaddr*)&serverAddr, addrLen);
        }
    }

    outFile.close();
    close(sock);
    cout << "File received successfully!" << endl;
    return 0;
}
