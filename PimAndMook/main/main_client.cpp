#include "../header/UDPClient.h"

static const int kWindowSizeBytes = 5000;

int main(int noOfArguments,char *argumentList[])
{
    UDPClient client;
    if(noOfArguments < 4)
    {
        client.displayError("Usage: client <server_ip> <port> <filename>");
    }
    client.createSocket();
    int portNo = atoi(argumentList[2]);
    client.getServerInfo(argumentList[1]);
    client.setServerAddress(portNo);
    client.createRequest(argumentList[3]);
    client.sendRequest();
    client.readResponse(kWindowSizeBytes, argumentList[3]);

    cout << "[Client] File received successfully" << endl;
    client.closeSocket();
    return 0;
}