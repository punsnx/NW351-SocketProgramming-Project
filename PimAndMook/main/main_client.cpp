#include "../header/UDPClient.h"

int main(int noOfArguments,char *argumentList[])
{
    UDPClient client;
    if(noOfArguments < 5)
    {
        client.displayError("Invalid arguments!");
    }
    client.createSocket();
    int portNo = atoi(argumentList[2]);
    client.getServerInfo(argumentList[1]);
    client.setServerAddress(portNo);
    client.createRequest(argumentList[3]);
    client.sendRequest();
    client.readResponse(atoi(argumentList[4]), argumentList[3]);
    return 0;
}
