#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <vector>
#include <sstream>
#include "header/project-header.h"

using namespace std;

#define TYPE_REQUEST 1
#define TYPE_RESPONSE 2
#define TYPE_DATA 3
#define TYPE_ACK 4
#define TYPE_NAK 5
#define TYPE_COMPLETE 6

struct MetaData {
    int type;
    char filename[256];
    bool fileExists;
    int fileSize;
    int totalSegments;
    int windowSize;
    int maxPayloadSize;
};

class ReliableUDPServer {
private:
    int serverSocket;
    sockaddr_in serverAddr;
    sockaddr_in clientAddr;
    socklen_t clientLen;
    
    // file transfer state
    vector<Segment*> fileSegments;
    char* currentFileData;      // store file data
    int currentFileSize;        // size of current file
    int currentSegment;         // which segment we're sending
    bool transferActive;        // are we transferring?
    string currentFilename;     // name of current file
    
    // server config
    int port;
    int maxRetries;
    int timeoutSeconds;

public:
    ReliableUDPServer(int portNum) : port(portNum) {
        clientLen = sizeof(clientAddr);
        currentSegment = 0;
        transferActive = false;
        maxRetries = 3;
        timeoutSeconds = 1;
        serverSocket = -1;
        currentFileData = nullptr;
        currentFileSize = 0;
        
        cout << "=== Simple UDP Server ===" << endl;
        cout << "Port: " << port << endl;
        cout << "MAX_PAYLOAD_SIZE: " << MAX_PAYLOAD_SIZE << " bytes" << endl;
        cout << "HEADER_SIZE: " << HEADER_SIZE << " bytes" << endl;
    }
    
    bool init() {
        serverSocket = socket(AF_INET, SOCK_DGRAM, 0);
        if(serverSocket < 0) {
            cout << "Error: Cannot create socket" << endl;
            return false;
        }
        
        memset(&serverAddr, 0, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(port);
        
        if(::bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
            cout << "Error: Cannot bind to port " << port << endl;
            return false;
        }
        
        cout << "Server listening on port " << port << "..." << endl;
        return true;
    }
    
    void start() {
        cout << "Waiting for client requests..." << endl;
        
        while(true) {
            Segment* receivedSeg = receiveSegment();
            
            if(receivedSeg) {
                handleSegment(receivedSeg);
                delete receivedSeg;
            }
        }
    }

private:
    Segment* receiveSegment() {
        // create buffer for receiving
        char buffer[sizeof(Header) + MAX_PAYLOAD_SIZE];
        
        int n = recvfrom(serverSocket, buffer, sizeof(buffer), 0,
                        (sockaddr*)&clientAddr, &clientLen);
        
        if(n <= 0) {
            return nullptr;
        }
        
        // create segment from received data
        Segment* seg = new Segment;
        
        // copy header
        memcpy(&seg->header, buffer, sizeof(Header));
        
        // allocate and copy payload
        int payloadSize = seg->header.length - HEADER_SIZE;
        if(payloadSize > 0) {
            seg->payload = new char[payloadSize];
            memcpy(seg->payload, buffer + sizeof(Header), payloadSize);
        } else {
            seg->payload = nullptr;
        }
        
        return seg;
    }
    
    void sendSegment(Segment* seg) {
        // create buffer to send
        int totalSize = seg->header.length;
        char* buffer = new char[totalSize];
        
        // copy header
        memcpy(buffer, &seg->header, sizeof(Header));
        
        // copy payload if exists
        int payloadSize = seg->header.length - HEADER_SIZE;
        if(payloadSize > 0 && seg->payload) {
            memcpy(buffer + sizeof(Header), seg->payload, payloadSize);
        }
        
        // send
        sendto(serverSocket, buffer, totalSize, 0,
               (sockaddr*)&clientAddr, clientLen);
        
        delete[] buffer;
    }
    
    void handleSegment(Segment* seg) {
        cout << "\nReceived segment - Seq: " << seg->header.seqNumber 
             << ", Length: " << seg->header.length << endl;
        
        // verify checksum
        unsigned short receivedChecksum = seg->header.checkSum;
        seg->header.checkSum = 0;
        unsigned short calculatedChecksum = calculateChecksum(seg);
        
        if(receivedChecksum != calculatedChecksum) {
            cout << "Segment corrupt! Sending NAK" << endl;
            sendNAK(seg->header.seqNumber);
            return;
        }
        
        // get message type from payload
        if(seg->payload) {
            MetaData* meta = (MetaData*)seg->payload;
            
            switch(meta->type) {
                case TYPE_REQUEST:
                    handleFileRequest(seg, meta);
                    break;
                    
                case TYPE_ACK:
                    handleAck(seg);
                    break;
                    
                case TYPE_NAK:
                    handleNak(seg);
                    break;
                    
                default:
                    cout << "Unknown type in segment" << endl;
            }
        }
    }
    
    void handleFileRequest(Segment* seg, MetaData* meta) {
        currentFilename = meta->filename;
        cout << "\n=== Got request for file: " << currentFilename << " ===" << endl;
        
        // cleanup old file data if exists
        if(currentFileData) {
            delete[] currentFileData;
            currentFileData = nullptr;
        }
        
        // read file using your function
        pair<char*, int> fileData = readFile(currentFilename);
        currentFileData = fileData.first;
        currentFileSize = fileData.second;
        
        // prepare response metadata
        MetaData responseMeta;
        memset(&responseMeta, 0, sizeof(responseMeta));
        responseMeta.type = TYPE_RESPONSE;
        strcpy(responseMeta.filename, currentFilename.c_str());
        
        if(currentFileSize > 0) {
            // file exists
            responseMeta.fileExists = true;
            responseMeta.fileSize = currentFileSize;
            responseMeta.maxPayloadSize = MAX_PAYLOAD_SIZE;
            
            // create segments for file
            cleanupSegments();
            fileSegments = packetize(currentFileData, currentFileSize, MAX_PAYLOAD_SIZE);
            responseMeta.totalSegments = fileSegments.size();
            
            cout << "File exists! Size: " << currentFileSize << " bytes" << endl;
            cout << "Created " << fileSegments.size() << " segments" << endl;
            
            currentSegment = 0;
            transferActive = true;
        } else {
            // file doesn't exist
            responseMeta.fileExists = false;
            responseMeta.fileSize = 0;
            responseMeta.totalSegments = 0;
            
            cout << "File does not exist!" << endl;
            transferActive = false;
        }
        
        // create response segment
        Segment* response = createSegment(&responseMeta, 0, sizeof(MetaData));
        response->header.checkSum = calculateChecksum(response);
        
        // send response
        sendSegmentReliable(response);
        
        // cleanup response segment
        delete[] (char*)response->payload;
        delete response;
        
        // if file exists, start sending data
        if(transferActive && fileSegments.size() > 0) {
            sendNextDataSegment();
        }
    }
    
    void sendNextDataSegment() {
        if(currentSegment >= fileSegments.size()) {
            cout << "All segments sent!" << endl;
            
            // send completion message
            MetaData completeMeta;
            completeMeta.type = TYPE_COMPLETE;
            strcpy(completeMeta.filename, currentFilename.c_str());
            
            Segment* complete = createSegment(&completeMeta, currentSegment, sizeof(MetaData));
            complete->header.checkSum = calculateChecksum(complete);
            sendSegment(complete);
            
            
            delete complete;
            
            transferActive = false;
            return;
        }
        
        // get current segment
        Segment* seg = fileSegments[currentSegment];
        
        cout << "Sending data segment " << currentSegment 
             << " (size: " << (seg->header.length - HEADER_SIZE) << " bytes)" << endl;
        
        // send the segment
        sendSegment(seg);
    }
    
    void handleAck(Segment* seg) {
        cout << "Got ACK for segment " << seg->header.seqNumber << endl;
        
        if(seg->header.seqNumber == currentSegment) {
            // correct ACK, move to next segment
            currentSegment++;
            
            if(currentSegment < fileSegments.size()) {
                sendNextDataSegment();
            } else {
                cout << "File transfer complete!" << endl;
                transferActive = false;
                cleanupSegments();
            }
        } else {
            // wrong ACK sequence
            cout << "Wrong ACK sequence. Expected: " << currentSegment 
                 << ", Got: " << seg->header.seqNumber << endl;
            sendNextDataSegment();
        }
    }
    
    void handleNak(Segment* seg) {
        cout << "Got NAK for segment " << seg->header.seqNumber << endl;
        
        // resend current segment
        if(transferActive) {
            sendNextDataSegment();
        }
    }
    
    void sendACK(int seqNum) {
        MetaData ackMeta;
        ackMeta.type = TYPE_ACK;
        
        Segment* ack = createSegment(&ackMeta, seqNum, sizeof(int));
        ack->header.checkSum = calculateChecksum(ack);
        
        sendSegment(ack);
        
        
        delete ack;
    }
    
    void sendNAK(int seqNum) {
        MetaData nakMeta;
        nakMeta.type = TYPE_NAK;
        
        Segment* nak = createSegment(&nakMeta, seqNum, sizeof(int));
        nak->header.checkSum = calculateChecksum(nak);
        
        sendSegment(nak);
        
        
        delete nak;
    }
    
    bool sendSegmentReliable(Segment* seg) {
        int tryCount = 0;
        
        while(tryCount < maxRetries) {
            // send the segment
            sendSegment(seg);
            cout << "Sent segment, waiting for ACK..." << endl;
            
            // set timeout
            struct timeval tv;
            tv.tv_sec = timeoutSeconds;
            tv.tv_usec = 0;
            setsockopt(serverSocket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            
            // wait for response
            Segment* response = receiveSegment();
            
            if(response) {
                // verify checksum
                unsigned short receivedChecksum = response->header.checkSum;
                response->header.checkSum = 0;
                unsigned short calculatedChecksum = calculateChecksum(response);
                
                if(receivedChecksum == calculatedChecksum) {
                    MetaData* meta = (MetaData*)response->payload;
                    if(meta->type == TYPE_ACK) {
                        cout << "Got ACK!" << endl;
                        delete[] (char*)response->payload;
                        delete response;
                        return true;
                    } else if(meta->type == TYPE_NAK) {
                        cout << "Got NAK, retrying..." << endl;
                    }
                } else {
                    cout << "Corrupt response, retrying..." << endl;
                }
                
                delete[] (char*)response->payload;
                delete response;
            } else {
                cout << "Timeout, retrying..." << endl;
            }
            
            tryCount++;
        }
        
        cout << "Failed after " << maxRetries << " tries" << endl;
        return false;
    }
    
    void cleanupSegments() {
        for(auto seg : fileSegments) {
            // don't delete payload as it points to file buffer
            delete seg;
        }
        fileSegments.clear();
    }
};

int main(int argc, char* argv[]) {
    if(argc != 2) {
        cout << "Usage: " << argv[0] << " <port>" << endl;
        cout << "Example: " << argv[0] << " 8080" << endl;
        return 1;
    }
    
    int port = atoi(argv[1]);
    
    ReliableUDPServer server(port);
    
    if(!server.init()) {
        return 1;
    }
    
    server.start();
    
    return 0;
}