#include "../header/UDPServer.h"

static const int kWindowSizeBytes = 5000; 

int main(int noOfArguments,char *argumentList[])
{
    UDPServer server;
    if(noOfArguments < 2)
    {
        server.displayError("Usage: server <port>");
    }
    server.createSocket();
    int portNo = atoi(argumentList[1]);
    server.bindAddress(portNo);
    server.setClientSockLength();
    while(1)
    {
        cout << "[Server] Waiting for client request..." << endl;
        server.receiveRequest();
        char* fileContent = server.getRequestedContent();
        cout << "[Server] Start sending file: " << server.segment.data << endl;
        server.createSegments(fileContent, kWindowSizeBytes);
        cout << "[Server] File transfer completed" << endl;
    }
    server.closeSocket();
    return 0;
}