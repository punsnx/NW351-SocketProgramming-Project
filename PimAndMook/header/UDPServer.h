#ifndef UDPSERVER_HPP
#define UDPSERVER_HPP

#include <iostream>
#include <sys/types.h>
#include <cstdlib>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <netdb.h>
#include <fstream>
#include <sstream>
#include <sys/time.h>
#include <cmath>
#include <ctime>
using namespace std;

const int mss = 1450;

class UDPServer
{
private:
    int serverSocket;
    socklen_t clientSockLen;
    struct sockaddr_in serverAddress;
    struct sockaddr_in clientAddress;
    char buff[10000];
    int fileSize;

public:
    struct reliableUDPData
    {
        uint32_t sequenceNumber;
        uint32_t ackNumber;
        bool ackFlag;
        char data[mss];
    } segment;

    struct timeval sampleRTT, estimatedRTT, devRTT, timeoutInterval;

    void displayError(const char *errorMsg);
    void createSocket();
    void bindAddress(int portNumber);
    void setClientSockLength();
    int receiveRequest();
    char *getRequestedContent();
    int getFileSize(const char *filename);
    void createSegments(char *fileContent, int windowSize);
    void slidingWindow(reliableUDPData *senderBuffer, int senderBufferLen, int windowSize);
    struct timeval calculateTimeout(struct timeval t1, struct timeval t2);
    reliableUDPData setHeader(int seqNo, int ackNo, int flag, char *datagram);
    void sendSegment(reliableUDPData seg);
    reliableUDPData receiveAck();
};

#endif
