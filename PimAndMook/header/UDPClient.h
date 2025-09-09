#ifndef UDPCLIENT_HPP
#define UDPCLIENT_HPP

#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <cstdlib>
#include <unistd.h>
#include <string.h>
#include <fstream>
#include <sstream>
#include <signal.h>
using namespace std;

class UDPClient
{
private:
    int clientSocket;
    unsigned int len;
    struct sockaddr_in serverAddress, clientAddress;
    struct hostent *host;

public:
    struct reliableUDPData
    {
        uint32_t sequenceNumber;
        uint32_t ackNumber;
        bool ackFlag;
        char data[1450];
    } udpSegment;

    void displayError(const char *errorMsg);
    void createSocket();
    void getServerInfo(char *hostname);
    void setServerAddress(int portNo);
    void createRequest(string filename);
    int sendRequest();
    void readResponse(int windowSize, char *outputFile);
    void sendAck(reliableUDPData segment);
    void writeToFile(reliableUDPData *response, int receiveBufferInd, char *filename);
    void closeSocket();
};

#endif
