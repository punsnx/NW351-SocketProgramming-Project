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
        server.receiveRequest();
        char* fileContent = server.getRequestedContent();
        server.createSegments(fileContent, atoi(argumentList[2]));
    }
    return 0;
}
