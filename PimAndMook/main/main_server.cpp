#include "../header/UDPServer.h"

int main(int noOfArguments,char *argumentList[])
{
    UDPServer server;
    if(noOfArguments < 3)
    {
        server.displayError("The client must provide a port number!");
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
        server.createSegments(fileContent, atoi(argumentList[2]));
        cout << "[Server] File transfer completed" << endl;
    }
    server.closeSocket();
    return 0;
}