#include "header/client.h"


class ReliableUDPClient {
private:
    int sockfd;                           // Socket descriptor
    sockaddr_in serverAddr{};             // Server address
    socklen_t serverLen{};                // Server address length

    double timeoutSec = 0.000002;         // Timeout for recvfromtuned
    int maxRetries = 100000;              // Max retry attempts
    int expectedSeq = -1;                 // Next expected sequence number

    int dropPercent;                     // Simulated drop probability  
    int corruptPercent;                  // Simulated corruption probability

    int tryCount = 0;                    // Track number of request attempts
public:
    // Constructor: create UDP socket, initialize server address
    ReliableUDPClient(const string& serverIp, int port, int dropP, int corruptP)
    : dropPercent(dropP), corruptPercent(corruptP)
    {
        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd < 0) {
            perror("socket");
            throw runtime_error("Cannot create socket");
        }

        // Setup server address
        memset(&serverAddr, 0, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);
        if (inet_pton(AF_INET, serverIp.c_str(), &serverAddr.sin_addr) != 1) {
            throw runtime_error("Invalid server IP");
        }
        serverLen = sizeof(serverAddr);

        // set default recv timeout
        setRecvTimeout(timeoutSec);

        cout << "=== Simple UDP Client ===\n";
        cout << "Server: " << serverIp << ":" << port << "\n";
        cout << "DROP_PERCENT: " << dropPercent << "%\n";
        cout << "CORRUPT_PERCENT: " << corruptPercent << "%\n";
        cout << "MAX_PAYLOAD_SIZE (local compile-time): " << MAX_PAYLOAD_SIZE << " bytes\n";
        cout << "HEADER_SIZE: " << HEADER_SIZE << " bytes\n";
    }

    // Destructor: close socket
    ~ReliableUDPClient() {
        if (sockfd >= 0) close(sockfd);
    }

    // Set socket recv timeout
    void setRecvTimeout(double seconds) {
        // set timeout
        struct timeval tv;
        // Split into seconds and microseconds
        tv.tv_sec = (time_t)seconds; // integer part
        tv.tv_usec = (suseconds_t)((seconds - tv.tv_sec) * 1e6); // fractional part to microseconds
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }

    void resetTryCount() {
        tryCount = 0;
    }

    // Send one segment with simulated drop/corruption
    bool sendSegment(Segment* seg) {
        // Simulate DROP
        int r = rand() % 100; // ได้ค่า 0-99
        if (r < dropPercent)  {
            cout << "[SIMULATED DROP] Dropping segment " << seg->header.seqNumber << endl;
            return false; // ไม่ส่งออกไป
        }


        // Simulate CORRUPTION
        int c = rand() % 100;
        if (c < corruptPercent) {
            cout << "[SIMULATED CORRUPTION] Corrupting segment " << seg->header.seqNumber << endl;

            seg->header.checkSum ^= 0xFFFF; // Flip checksum

            // Randomly flip payload byte
            int payloadSize = seg->header.length - HEADER_SIZE;
            if (payloadSize > 0 && seg->payload) {
                int pos = rand() % payloadSize;
                ((char*)seg->payload)[pos] ^= 0xFF; // flip bits ที่ตำแหน่งสุ่ม
            }
        }

        // Build raw buffer to send
        int totalSize = seg->header.length;
        char* buffer = new char[totalSize];
        

        memcpy(buffer, &seg->header, sizeof(Header));
        int payloadSize = seg->header.length - HEADER_SIZE;
        if (payloadSize > 0 && seg->payload) {
            memcpy(buffer + sizeof(Header), seg->payload, payloadSize);
        }

        // Send via UDP
        sendto(sockfd, buffer, totalSize, 0, (sockaddr*)&serverAddr, serverLen);
        cout << "<<-- send segment with Sequence Number: " << seg->header.seqNumber << endl;
        delete[] buffer;
        return true;
    }

    // Receive one segment (retry internally)
    Segment* receiveSegment(bool notRetryReceive) {        
        char buffer[sizeof(Header) + MAX_PAYLOAD_SIZE];

        // Setup timeout
        struct timeval tv;
        tv.tv_sec = (time_t)timeoutSec; // integer part
        tv.tv_usec = (suseconds_t)((timeoutSec - tv.tv_sec) * 1e6);
        if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv)) < 0) {
            perror("setsockopt failed");
            return nullptr;
        }

        sockaddr_in from{};
        socklen_t fromLen = sizeof(from);

        int retry = 0;
        while (retry < maxRetries) {
            int n = recvfrom(sockfd, buffer, sizeof(buffer), 0, (sockaddr*)&from, &fromLen);
            if (n > 0) {
                retry = 0;  // reset tryCount on successful receive
                Segment* seg = new Segment;
                memcpy(&seg->header, buffer, sizeof(Header));
                int payloadSize = seg->header.length - HEADER_SIZE;
                if (payloadSize > 0) {
                    seg->payload = new char[payloadSize];
                    memcpy(seg->payload, buffer + sizeof(Header), payloadSize);
                } else {
                    seg->payload = nullptr;
                }
                cout << "--->> Client received segment of Sequence Number: "
                    << seg->header.seqNumber << " (Payload: " << payloadSize << " bytes)" << endl;
                return seg;
            } else {
                // Timeout or error
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    cout << "[Timeout] No data received.";
                    retry++;
                    if (notRetryReceive) {
                        cout << endl;
                        return nullptr;
                        }
                        
                    cout << " Retry " << retry << "/" << maxRetries << endl;
                } else {
                    perror("recvfrom error");
                    return nullptr;
                }
            }
        }

        cout << "[INFO] Max retries reached for receiveSegment\n";
        return nullptr;
    }

    // Receive segment with limited retry
    Segment* receiveSegmentWithLimit(int limit) {        
        char buffer[sizeof(Header) + MAX_PAYLOAD_SIZE];

        // ตั้งค่า timeout ให้ recvfrom
        struct timeval tv;
        tv.tv_sec = (time_t)timeoutSec; // integer part
        tv.tv_usec = (suseconds_t)((timeoutSec - tv.tv_sec) * 1e6);
        if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv)) < 0) {
            perror("setsockopt failed");
            return nullptr;
        }

        sockaddr_in from{};
        socklen_t fromLen = sizeof(from);

        int retry = 0;
        while (retry < limit) {
            int n = recvfrom(sockfd, buffer, sizeof(buffer), 0, (sockaddr*)&from, &fromLen);
            if (n > 0) {
                retry = 0;  // reset tryCount on successful receive
                Segment* seg = new Segment;
                memcpy(&seg->header, buffer, sizeof(Header));
                int payloadSize = seg->header.length - HEADER_SIZE;
                if (payloadSize > 0) {
                    seg->payload = new char[payloadSize];
                    memcpy(seg->payload, buffer + sizeof(Header), payloadSize);
                } else {
                    seg->payload = nullptr;
                }
                cout << "--->> Client received segment of Sequence Number: "
                    << seg->header.seqNumber << " (Payload: " << payloadSize << " bytes)" << endl;
                return seg;
            } else {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    cout << "[Timeout] No data received." << endl;
                    if(retry > limit)
                    {
                        cout << "[Debug] Max adjust retries reached for receive segment." << endl;
                        return nullptr;
                    }
                    retry++;
                
                } else {
                    perror("recvfrom error");
                    return nullptr;
                }
            }
        }

        cout << "[INFO] Max retries reached for receiveSegment\n";
        return nullptr;
    }


    // Send ACK for seq
    void sendACK(int seq) {
        MetaData meta{}; meta.type = TYPE_ACK;
        Segment* ack = createSegment(&meta, seq, sizeof(MetaData));
        ack->header.checkSum = calculateChecksum(ack);
        
        sendSegment(ack);
        cout << "<<--- Client send ACK for Sequence Number:" << seq << endl;

        delete ack; // payload points to stack memory (meta), do not delete
    }

    // Send NAK for seq
    void sendNAK(int seq) {
        MetaData meta{}; meta.type = TYPE_NAK;
        Segment* nak = createSegment(&meta, seq, sizeof(MetaData));
        nak->header.checkSum = calculateChecksum(nak);

        sendSegment(nak);
        cout << "[INFO] Client send NAK for Sequence Number:" << seq << endl;

        delete nak; // payload points to stack memory (meta), do not delete
    }

    // -------------------------
    // Protocol Implementation
    // -------------------------

    // Step 1: Request file from server
    bool requestFile(const string& filename, MetaData& outRespMeta) {
        cout << "\n\n~~~~~~~~~~~~=== Requesting file: " << filename << " ===~~~~~~~~~~~~\n";

        // Build request
        MetaData req{};
        req.type = TYPE_REQUEST;
        memset(req.filename, 0, sizeof(req.filename));
        strncpy(req.filename, filename.c_str(), sizeof(req.filename) - 1);

        Segment* request = createSegment(&req, -1, sizeof(MetaData));
        request->header.checkSum = calculateChecksum(request);

        bool serverRespond = false;
        
        while (!serverRespond && tryCount < maxRetries) {
            expectedSeq = -1;
            tryCount++;
            cout << "[INFO] Sending request for file: " << filename 
                << " (Attempt " << tryCount << "/" << maxRetries << ")\n";
            sendSegment(request);

            // Wait for first ACK
            Segment* seg = receiveSegment(true);  // จะ retry recvfrom ภายในเอง

            if (!seg) {
                cout << "[WARN] Timeout waiting for first ACK, resending request\n";
                continue; // loop จะ resend request

            }

            // Verify type
            unsigned short recvChk = seg->header.checkSum;
            seg->header.checkSum = 0;
            if (recvChk != calculateChecksum(seg)) {
                cout << "[ERROR] First ACK checksum mismatch, resending request\n";
                cleanup(seg);
                continue; // resend request
            }

            // ตรวจสอบ sequence และ type
            if (seg->header.seqNumber == expectedSeq) {
                if (seg->payload) {
                    MetaData* meta = (MetaData*)seg->payload;
                    if (meta->type == TYPE_ACK) {
                        outRespMeta = *meta;
                        cout << "[INFO] Received first ACK from server\n";
                        serverRespond = true;
                        cleanup(seg);
                        break;
                    } else if (meta->type == TYPE_NAK) {
                        cout << "[INFO] Received NAK from server, resending request\n";
                    } else {
                        cout << "[ERROR] Unexpected server response, expect seq= " << expectedSeq << ", got seq=" << seg->header.seqNumber << ", type=" << meta->type << ", resending request\n";
                   
                    }
                }
            } else {
                cout << "[WARN] Unexpected sequence number " << seg->header.seqNumber 
                    << ", expected " << expectedSeq << ", resending request\n";
            }

            cleanup(seg);
        }

        delete request;

        if (!serverRespond) {
            cout << "[INFO] Max retries reached for requestFile\n";
            return false;
        }

        return true;
    }


    // Step 2: Receive Metadata
    bool receiveMeta(const string& filename, MetaData& outRespMeta) {
        cout << "\n=== Waiting for Metadata from server for file: " << filename << " ===\n";

        int retryCount = 0;
        bool serverRespond = false;

        while (true) {
            Segment* seg = receiveSegment(false);

            if (!seg) {
                return false;
            }

            // Verify checksum: isCorrupt?
            unsigned short recvChk = seg->header.checkSum;
            seg->header.checkSum = 0;
            if (recvChk != calculateChecksum(seg)) {
                cout << "[ERROR] RESPONSE checksum mismatch, sending NAK\n";
                sendNAK(expectedSeq);

                cleanup(seg);
                continue;
            }

            // Verify sequence: isCorrectSeq?
            if(seg->header.seqNumber == expectedSeq){
                if(seg->payload) {
                    MetaData* meta = (MetaData*)seg->payload;
                    if(seg->header.seqNumber == expectedSeq) {    
                        if (meta->type == TYPE_RESPONSE) {
                            outRespMeta = *meta;
                            cout << "[INFO] Server response: fileExists=" << meta->fileExists
                            << ", fileSize=" << meta->fileSize
                            << ", totalSegments=" << meta->totalSegments
                            << ", maxPayload=" << meta->maxPayloadSize << ", type=" << meta->type << ", fileName=" << meta->filename << endl;
                            if (meta->filename == filename) {
                                sendACK(seg->header.seqNumber);
                                cleanup(seg);
                                expectedSeq++;
                                return true;
                            } else {
                                cout << "[Debug] drop segment due to unmatch filename" << endl;
                            }
                            
                        }            
                    } else if(seg->header.seqNumber < expectedSeq) {
                        cout << "[LOG] Receive Sequence Number: " << seg->header.seqNumber << ", but Expected for Sequence Number: " << expectedSeq << endl;
                        cout << "[INFO] Duplicate segment " << seg->header.seqNumber << ", re-ACK sent\n";
                        sendACK(seg->header.seqNumber);
                    } else {
                        cout << "[INFO] Future segment " << seg->header.seqNumber << endl;
                        sendNAK(seg->header.seqNumber);
                    }
                }
            }
            cleanup(seg);
        }

            if(handleNextProcess(filename)){
                cout << "[Debug] Ready for next process." << endl;
            return true;
        }
    }

    // Step 3: Receive File Data
    bool receiveFile(const string& filename, int totalSegments) {
        string folder = "clientFiles/";
        string filepath = folder + filename;
        if (!ensureDir(folder)) return false;

        vector<Segment*> parts; //++
        parts.reserve(static_cast<size_t>(totalSegments)); //++

        int expectedSeq = 0;
        int received = 0;

        cout << "\n=== Receiving file: " << filename << " ===\n";

        while (received <= totalSegments) {
            Segment* seg = receiveSegment(false); // retry within receiveSegment

            if (!seg) continue;

            // Verify checksum
            unsigned short recvChk = seg->header.checkSum;
            seg->header.checkSum = 0;

            unsigned short calcChk = calculateChecksum(seg);

            if (recvChk != calculateChecksum(seg)) {
                cout << "[ERROR] Segment " << seg->header.seqNumber << " checksum mismatch, sending NAK\n";
                sendNAK(seg->header.seqNumber);
                cleanup(seg);
                continue; // ไม่ append payload
            }

            int seq = seg->header.seqNumber;

            MetaData* meta = (MetaData*)seg->payload;

            if (seq == expectedSeq) {
                if (meta->type != TYPE_NAK) {

                    int psize = seg->header.length - HEADER_SIZE;
                    if (psize > 0 && seg->payload) {                     
                        if (meta->type != TYPE_COMPLETE) {
                            // Save segment copy
                            char* dup = new char[psize]; //++
                            memcpy(dup, seg->payload, psize); //++
                            Segment* part = createSegment(dup, seq, psize); //++
                            part->header.checkSum = calculateChecksum(part); //++
                            parts.push_back(part); //++
                        }
                        sendACK(seq);
                        expectedSeq++;
                        received++;
                        cout << "[INFO] Received segment " << seq << ", ACK sent\n";
                    }
                }
            } else if (seq < expectedSeq) {
                cout << "[INFO] Duplicate segment " << seq << ", re-ACK sent\n";
                sendACK(seq);
            } else {
                cout << "[INFO] Future segment " << seq << ", send NAK for expected " << expectedSeq << endl;
                sendNAK(expectedSeq);
            }

            
            cleanup(seg);
        }

        // Assemble all parts
        auto assembled = resemble(parts); // pair<char*, int> (payload, size) //++
        writeFile(filepath, assembled.first, assembled.second); // เขียนไฟล์ลงดิสก์ //++
        cout << "[INFO] File saved to: " << filepath << "\n";

        // เก็บกวาด parts และ buffer ที่ resemble คืนมา //++
        // Cleanup
        for (auto* s : parts) {
            if (s) {
                if (s->payload) delete[] (char*)s->payload;
                delete s;
            }
        }
        delete[] assembled.first; //++
        
        if(handleNextProcess(filename)){
            cout << "[SUCCESS] Receiving success" << endl;
            cout << "[Debug] Ready to send next file request" << endl;
            return true;
        }

    }

    // Handle TYPE_COMPLETE or extra segments
    bool handleNextProcess(const string& filename) {
        // receive until server not sent anything
        while (true) {
            int limit = 5;
            Segment* seg = receiveSegmentWithLimit(limit);

            if (seg == nullptr) {
                return true;
            }

            MetaData* meta = (MetaData*)seg->payload;

            if (meta->filename == filename) {
                switch(meta->type) {
                    case TYPE_COMPLETE:
                        cout << "[Debug] server clear" << endl;
                        cout << "[SUCCESS] success" << endl;
                        sendACK(seg->header.seqNumber);  
                    case TYPE_ACK:
                        continue;
                        
                    case TYPE_REQUEST:
                        sendACK(seg->header.seqNumber);
                        continue;
                        
                    default:
                        cout << "Unknown type in segment" << endl;
                }
            }
        }
    }
  

    // Case: file doesn’t exist on server
    bool receiveNonExist(const string& filename, MetaData& outRespMeta) {
        cout << "\n=== File Not Exist, Waiting for TYPE_COMPLETE from server for file: " << filename << " ===\n";

        int retryCount = 0;
        while (true) {
            Segment* seg = receiveSegment(false);
            
            if (!seg) {
                cout << "[IGNORE]" << endl;
                return false;
            }

            unsigned short recvChk = seg->header.checkSum;
            seg->header.checkSum = 0;
            if (recvChk != calculateChecksum(seg)) {
                cout << "[ERROR] RESPONSE checksum mismatch, sending NAK\n";
                sendNAK(expectedSeq);

                cleanup(seg);
                continue;
            }

            MetaData* meta = (MetaData*)seg->payload;

            if(seg->header.seqNumber == expectedSeq){
                if(seg->payload) {
                    MetaData* meta = (MetaData*)seg->payload;
                    
                    if(meta->type == TYPE_COMPLETE && seg->header.seqNumber == expectedSeq) {                
                        outRespMeta = *meta;

                        cleanup(seg);
                        cout << "[Debug] got TYPE_COMPLETE" << endl; 
                        cout << "[SUCCESS] receiveNonExist success" << endl;

                        if(handleNextProcess(filename)){
                            cout << "[Debug] Ready to send next file request" << endl;
                        }

                        return true;
                    }
                }
            }
        }  

    }


private:
        // Cleanup allocated memory for segment
        static void cleanup(Segment* seg) {
            if (!seg) return;
            if (seg->payload) delete[] (char*)seg->payload;
            delete seg;
        }
        // Ensure directory exists, create if not
        static bool ensureDir(const string& path) {
            struct stat st{};
            errno = 0;
            if (stat(path.c_str(), &st) == 0) {
                return S_ISDIR(st.st_mode);          // มีอยู่และเป็นไดเรกทอรี
            }
            if (errno == ENOENT) {                    // ไม่มีอยู่ → สร้างใหม่
                return mkdir(path.c_str(), 0755) == 0;
            }
            return false;                             // มีข้อผิดพลาดอื่น
        }
};

// ======================
// Main program
// ======================
int main(int argc, char* argv[]) {
    if (argc < 6) {
            cout << "Usage: " << argv[0] 
                << " <server_ip> <port> <drop_percent> <corrupt_percent> <file1> [file2 ...]" 
                << endl;
            return 1;
        }

    string serverIp = argv[1];
    int port = atoi(argv[2]);
    int dropPercent = atoi(argv[3]);
    int corruptPercent = atoi(argv[4]);

    try {
        ReliableUDPClient client(serverIp, port, dropPercent, corruptPercent);

        int filesCount = argc - 5;
        bool multiFiles = (filesCount > 1);

        for (int i = 5; i < argc; ++i) {
            string fname = argv[i];
            MetaData resp{}; 
            
            client.resetTryCount();

            if(!client.requestFile(fname, resp)) {
                return 1;
            }
            client.resetTryCount();

            while (!client.receiveMeta(fname, resp)) {
                cout << "[ERROR] Failed to receive RESPONSE metadata for file: " << fname << endl;
            }

            client.resetTryCount();

            
            
            cout << "[Debug] multiFiles = " << (multiFiles ? "true" : "false") << endl;

            if (!resp.fileExists) {
                cout << "[INFO] Server reports file NOT FOUND: " << fname << endl;
                if (!multiFiles){

                    while(!client.receiveNonExist(fname, resp));
                    return 1;
                } 
                else {
                    while(!client.receiveNonExist(fname, resp));
                    continue;
                }
            }

            if (resp.fileSize < 0 || resp.totalSegments < 0) {
                cout << "[ERROR] Invalid file meta for " << fname
                     << " (size=" << resp.fileSize
                     << ", totalSegments=" << resp.totalSegments << ")" << endl;
                if (!multiFiles) return 1;
                continue;
            }

            string effectiveName = (resp.filename[0] ? string(resp.filename) : fname);
            if (!client.receiveFile(effectiveName, resp.totalSegments)) {
                cout << "[ERROR] Failed while receiving file: " << effectiveName << endl;
                if (!multiFiles) return 1;
            }
        }
    } catch (const exception& ex) {
        cout << "Fatal: " << ex.what() << endl;
        return 2;
    }

    return 0;
}