#include "../header/UDPServer.h"

void UDPServer::displayError(const char *errorMsg)
{
    cerr << "Error: " << errorMsg << endl;
    exit(1);
}

void UDPServer::createSocket()
{
    serverSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (serverSocket < 0)
        displayError("The server socket could not be opened!");
	else
        cout << "[Server] Socket OPENED" << endl;
}

void UDPServer::bindAddress(int portNumber)
{
    int leng = sizeof(serverAddress);
    bzero(&serverAddress, leng);
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(portNumber);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSocket, (struct sockaddr *)&serverAddress, leng) < 0)
        displayError("There is some problem while binding the server socket to an address!");
	else
        cout << "[Server] Socket BOUND to port " << portNumber << endl;
}

void UDPServer::closeSocket() {
    close(serverSocket);
    cout << "[Server] Socket CLOSED" << endl;
}

void UDPServer::setClientSockLength()
{
    clientSockLen = sizeof(struct sockaddr_in);
}

int UDPServer::receiveRequest()
{
    int noOfCharacters = recvfrom(serverSocket, &segment, sizeof(segment), 0, 
                                  (struct sockaddr *)&clientAddress, &clientSockLen);
    if (noOfCharacters < 0)
        displayError("There is some problem in receiving the request!");
    return noOfCharacters;
}

int UDPServer::getFileSize(const char *filename)
{
    ifstream file(filename, ios_base::binary);
    if (!file)
        return -1;
    file.seekg(0, ios_base::end);
    int size = file.tellg();
    file.close();
    return size;
}

char* UDPServer::getRequestedContent()
{
    fileSize = getFileSize(segment.data);
    if (fileSize < 0)
        displayError("File Not Found");

    char *fileContent = new char[fileSize];
    ifstream readFile(segment.data, ios_base::binary);
    readFile.read(fileContent, fileSize);
    return fileContent;
}

UDPServer::reliableUDPData UDPServer::setHeader(int seqNo, int ackNo, int flag, char *datagram)
{
    reliableUDPData udpData;
    udpData.sequenceNumber = seqNo;
    udpData.ackNumber = ackNo;
    udpData.ackFlag = flag;
    strcpy(udpData.data, datagram);
    return udpData;
}

void UDPServer::sendSegment(reliableUDPData seg)
{
    cout << "Sending packet with sequence number: " << seg.sequenceNumber << endl;
    int no = sendto(serverSocket, &seg, sizeof(seg), 0, (struct sockaddr *)&clientAddress, clientSockLen);
    if (no < 0)
        displayError("There is some problem in sending the segment!");
}

UDPServer::reliableUDPData UDPServer::receiveAck()
{
    reliableUDPData ack;
    int no = recvfrom(serverSocket, &ack, sizeof(ack), 0, (struct sockaddr *)&clientAddress, &clientSockLen);
    if (no < 0)
        displayError("There is some problem in receiving the segment!");
    cout << "Received Acknowledgement " << ack.ackNumber 
         << " for sequence number " << ack.sequenceNumber << endl;
    return ack;
}

struct timeval UDPServer::calculateTimeout(struct timeval t1, struct timeval t2)
{
    double alpha = 0.125, beta = 0.25;
    sampleRTT.tv_sec = t2.tv_sec - t1.tv_sec;
    sampleRTT.tv_usec = t2.tv_usec - t1.tv_usec;

    estimatedRTT.tv_sec = ((1 - alpha) * estimatedRTT.tv_sec + alpha * sampleRTT.tv_sec);
    estimatedRTT.tv_usec = ((1 - alpha) * estimatedRTT.tv_usec + alpha * sampleRTT.tv_usec);

    devRTT.tv_sec = ((1 - beta) * devRTT.tv_sec + beta * abs(sampleRTT.tv_sec - estimatedRTT.tv_sec));
    devRTT.tv_usec = ((1 - beta) * devRTT.tv_usec + beta * abs(sampleRTT.tv_usec - estimatedRTT.tv_usec));

    timeoutInterval.tv_sec = estimatedRTT.tv_sec + 4 * devRTT.tv_sec;
    timeoutInterval.tv_usec = estimatedRTT.tv_usec + 4 * devRTT.tv_usec;

    return timeoutInterval;
}

void UDPServer::createSegments(char *fileContent, int windowSize)
{
    int noOfSegments = fileSize / mss;
    char seg[mss];
    uint32_t seqNo = 0;
    uint32_t ackNo = segment.sequenceNumber + 1;
    int ackFlag = 0;

    int senderBufferLen = noOfSegments + 1;
    reliableUDPData *senderBuffer = new reliableUDPData[senderBufferLen];

    for (int j = 0; j < noOfSegments; j++)
    {
        for (int i = j * mss, k = 0; i < (j + 1) * mss && k < mss; i++, k++)
            seg[k] = fileContent[i];

        senderBuffer[j] = setHeader(seqNo, ackNo, ackFlag, seg);
        seqNo++;
    }

    int rem = fileSize % mss;
    for (int s = 0; s < rem; s++)
        seg[s] = fileContent[noOfSegments * mss + s];

    senderBuffer[noOfSegments] = setHeader(seqNo, ackNo, ackFlag, seg);
    slidingWindow(senderBuffer, senderBufferLen, windowSize);
}

void UDPServer::slidingWindow(reliableUDPData *senderBuffer, int senderBufferLen, int windowSize)
{
    uint32_t firstUnAck = 0, nxtSeqNo = 0, dupAckCnt;
    reliableUDPData ack;
    cout << "No of segments to be sent: " << senderBufferLen << endl;

    int cwnd = 1;
    int ssthresh = 64000;
    int segmentSize = sizeof(senderBuffer[0]);
    int noOfSegmentsInWin = windowSize / segmentSize;

    cout << "No of segments in window " << noOfSegmentsInWin << endl;

    int dropPercent = 60;
    int noOfPacketsToDrop = (dropPercent * noOfSegmentsInWin) / 100;
    cout << "No of packets to drop " << noOfPacketsToDrop << endl;

    uint32_t *packetsToDrop = new uint32_t[noOfPacketsToDrop];
    estimatedRTT.tv_sec = 0; estimatedRTT.tv_usec = 0;
    devRTT.tv_sec = 0; devRTT.tv_usec = 0;
    timeoutInterval.tv_sec = 2; timeoutInterval.tv_usec = 0;

    fd_set fds;
    int val = 1;
    struct timeval t1, t2;

    while (nxtSeqNo < (uint32_t)senderBufferLen)
    {
        if (firstUnAck == 0 && nxtSeqNo == 0)
        {
            srand(time(NULL));
            for (int i = 0; i < noOfPacketsToDrop; i++)
                packetsToDrop[i] = rand() % (noOfSegmentsInWin - 1) + 1;
        }

        int minimumSize = (cwnd < noOfSegmentsInWin) ? cwnd : noOfSegmentsInWin;
        gettimeofday(&t1, NULL);

        while (nxtSeqNo < firstUnAck + (uint32_t)minimumSize && nxtSeqNo < (uint32_t)senderBufferLen)
        {
            if (nxtSeqNo == (uint32_t)senderBufferLen - 1)
                senderBuffer[nxtSeqNo].ackFlag = 1;

            bool flag = true;
            for (int i = 0; i < noOfPacketsToDrop; i++)
                if (nxtSeqNo == packetsToDrop[i]) flag = false;

            if (flag)
                sendSegment(senderBuffer[nxtSeqNo]);

            nxtSeqNo++;
        }

        dupAckCnt = 0;
        FD_ZERO(&fds);
        FD_SET(serverSocket, &fds);
        val = select(serverSocket + 1, &fds, NULL, NULL, &timeoutInterval);

        if (val == 0)
        {
            ssthresh = (cwnd * segmentSize) / 2;
            cwnd = 1;
            timeoutInterval.tv_sec *= 2;
            timeoutInterval.tv_usec *= 2;
            continue;
        }
        if (val == -1)
            displayError("There is some problem in receiving the segment!");

        if (FD_ISSET(serverSocket, &fds) && val == 1)
        {
            ack = receiveAck();
            gettimeofday(&t2, NULL);

            if (ack.ackNumber < nxtSeqNo)
            {
                dupAckCnt++;
                while (dupAckCnt < 3)
                {
                    ack = receiveAck();
                    dupAckCnt++;
                }
                for (int i = 0; i < noOfPacketsToDrop; i++)
                    if (ack.ackNumber == packetsToDrop[i])
                        packetsToDrop[i] = UINT32_MAX;
            }

            if (dupAckCnt < 3 && minimumSize == cwnd)
            {
                if ((cwnd * segmentSize) >= ssthresh)
                {
                    cout << "Congestion Avoidance" << endl;
                    cwnd = cwnd + 1;
                }
                else
                {
                    cout << "Slow Start" << endl;
                    cwnd = cwnd * 2;
                }
                timeoutInterval = calculateTimeout(t1, t2);
            }

            firstUnAck = ack.ackNumber;
            nxtSeqNo = ack.ackNumber;
        }
    }

    cout << "File Sent Successfully" << endl;
}
