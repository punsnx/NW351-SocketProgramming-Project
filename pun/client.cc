#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fstream>

#include "header/client.h"

using namespace std;


class ReliableUDPClient {
private:
    int sockfd;
    sockaddr_in serverAddr{};
    socklen_t serverLen{};

    int timeoutSec = 1;   // default 1s; can be tuned
    int maxRetries = 5;   // retry for requests/NAKs

public:
    ReliableUDPClient(const string& serverIp, int port) {
        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd < 0) {
            perror("socket");
            throw runtime_error("Cannot create socket");
        }

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
        cout << "MAX_PAYLOAD_SIZE (local compile-time): " << MAX_PAYLOAD_SIZE << " bytes\n";
        cout << "HEADER_SIZE: " << HEADER_SIZE << " bytes\n";
    }

    ~ReliableUDPClient() {
        if (sockfd >= 0) close(sockfd);
    }

    void setRecvTimeout(int seconds) {
        timeval tv{};
        tv.tv_sec = seconds;
        tv.tv_usec = 0;
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }

    // ---- wire helpers ----
    void sendSegment(Segment* seg) {
        int totalSize = seg->header.length;
        char* buffer = new char[totalSize];

        memcpy(buffer, &seg->header, sizeof(Header));
        int payloadSize = seg->header.length - HEADER_SIZE;
        if (payloadSize > 0 && seg->payload) {
            memcpy(buffer + sizeof(Header), seg->payload, payloadSize);
        }

        sendto(sockfd, buffer, totalSize, 0, (sockaddr*)&serverAddr, serverLen);
        cout << "<<--- send segment with Sequence Number: " << seg->header.seqNumber << endl;
        delete[] buffer;
    }

    Segment* receiveSegment() {
        char buffer[sizeof(Header) + MAX_PAYLOAD_SIZE];
        sockaddr_in from{}; 
        socklen_t fromLen = sizeof(from);
        int n = recvfrom(sockfd, buffer, sizeof(buffer), 0, (sockaddr*)&from, &fromLen);
        if (n <= 0) return nullptr; // timeout or error

        Segment* seg = new Segment;
        memcpy(&seg->header, buffer, sizeof(Header));
        int payloadSize = seg->header.length - HEADER_SIZE;
        if (payloadSize > 0) {
            seg->payload = new char[payloadSize];
            memcpy(seg->payload, buffer + sizeof(Header), payloadSize);
        } else {
            seg->payload = nullptr;
        }
        cout << "--->> Client receive segment of Sequence Number: " << seg->header.seqNumber << endl;
        return seg;
    }

    void sendACK(int seq) {
        MetaData meta{}; meta.type = TYPE_ACK;
        Segment* ack = createSegment(&meta, seq, (int)sizeof(int));
        ack->header.checkSum = calculateChecksum(ack);
        sendSegment(ack);
        cout << "<<--- Client send ACK for Sequence Number:" << seq << endl;
        delete ack; // payload points to stack memory (meta), do not delete
    }

    void sendNAK(int seq) {
        MetaData meta{}; meta.type = TYPE_NAK;
        Segment* nak = createSegment(&meta, seq, (int)sizeof(int));
        nak->header.checkSum = calculateChecksum(nak);
        sendSegment(nak);
        cout << "Client send NAK for Sequence Number:" << seq << endl;
        delete nak; // payload points to stack memory (meta), do not delete
    }

    // ---- protocol ----
    // reliable send
    bool requestFile(const string& filename, MetaData& outRespMeta) {
        cout << "\n=== Requesting file: " << filename << " ===\n";

        MetaData req{};
        req.type = TYPE_REQUEST;
        memset(req.filename, 0, sizeof(req.filename));
        strncpy(req.filename, filename.c_str(), sizeof(req.filename) - 1);

        Segment* request = createSegment(&req, 0, sizeof(MetaData));
        request->header.checkSum = calculateChecksum(request);

        cout << "[INFO] Sending request for file: " << filename << endl;
        sendSegment(request);
        delete request;

        int expectedSeq = 0;
        int retryCount = 0;
        bool serverRespond = false;

        while (true) {
            Segment* seg = receiveSegment();
            if (!seg) {
                retryCount++;
                cerr << "[TIMEOUT] Waiting for First ACK, retry " << retryCount << "/" << maxRetries
                    << " (expecting seq " << expectedSeq << ")\n";
                if (retryCount >= maxRetries) {
                    cerr << "[ERROR] Max retries exceeded. Aborting.\n";
                    exit(1);
                }
                sendNAK(expectedSeq);
                continue;
            }
            retryCount = 0;

            // isCorrupt?
            unsigned short recvChk = seg->header.checkSum;
            seg->header.checkSum = 0;
            if (recvChk != calculateChecksum(seg)) {
                cerr << "[ERROR] First ACK checksum mismatch, sending NAK\n";
                sendNAK(expectedSeq);
                cleanup(seg);
                continue;
            }

            // isCorrectSeq?
            if(seg->header.seqNumber == expectedSeq){
                if(!serverRespond) { 
                MetaData* meta = (MetaData*)seg->payload;
                // isACK?
                if(meta->type == TYPE_ACK) {
                    sendACK(seg->header.seqNumber);
                    outRespMeta = *meta;
                    cout << "[INFO] Recieve First ACK from Server\n";
                    cleanup(seg);
                    // return true;
                    serverRespond = true;
                    return true;
                } else {
                    cerr << "[ERROR] Server did not send First ACK\n";
                    sendNAK(expectedSeq);
                    continue;
                }
            }
            }
            
            // check type==ack ก่อน ถ้าไม่ ack --> timer until timelimit
            
            // if(serverRespond) {

            // else {
            //     if(seg->payload) {
            //         MetaData* meta = (MetaData*)seg->payload;
            //         if(meta->type == TYPE_RESPONSE && seg->header.seqNumber == expectedSeq) {
            //             sendACK(seg->header.seqNumber);
            //             outRespMeta = *meta;
            //             cout << "[INFO] Server response received: fileExists=" << meta->fileExists
            //                 << ", fileSize=" << meta->fileSize
            //                 << ", totalSegments=" << meta->totalSegments
            //                 << ", maxPayload=" << meta->maxPayloadSize << "\n";
            //             cleanup(seg);
            //             return true;
            //         }
            //     }
            // }

            cerr << "[INFO] Unexpected packet, sending NAK\n";
            sendNAK(expectedSeq);
            cleanup(seg);
        }
    }

    bool receiveMeta(const string& filename, MetaData& outRespMeta) {
        cout << "\n=== Waiting for Metadata from server for file: " << filename << " ===\n";

        int expectedSeq = 0;
        int retryCount = 0;
        bool serverRespond = false;

        while (true) {
            Segment* seg = receiveSegment();
            if (!seg) {
                retryCount++;
                cerr << "[TIMEOUT] Waiting for RESPONSE, retry " << retryCount << "/" << maxRetries
                    << " (expecting seq " << expectedSeq << ")\n";
                if (retryCount >= maxRetries) {
                    cerr << "[ERROR] Max retries exceeded. Aborting.\n";
                    exit(1);
                }
                sendNAK(expectedSeq);
                continue;
            }
            retryCount = 0;
            
            
            //************************* NOTE ... implement is already receive packet ?

            // isCorrupt?
            unsigned short recvChk = seg->header.checkSum;
            seg->header.checkSum = 0;
            if (recvChk != calculateChecksum(seg)) {
                cerr << "[ERROR] RESPONSE checksum mismatch, sending NAK\n";
                sendNAK(expectedSeq);
                cleanup(seg);
                continue;
            }

            // isCorrectSeq?
            if(seg->header.seqNumber == expectedSeq){
                if(seg->payload) {
                    MetaData* meta = (MetaData*)seg->payload;
                    if(meta->type == TYPE_RESPONSE && seg->header.seqNumber == expectedSeq) {                
                        sendACK(seg->header.seqNumber);
                        outRespMeta = *meta;
                        cout << "[INFO] Server response: fileExists=" << meta->fileExists
                            << ", fileSize=" << meta->fileSize
                            << ", totalSegments=" << meta->totalSegments
                            << ", maxPayload=" << meta->maxPayloadSize << "\n";
                        cleanup(seg);
                        return true;
                    }
                }
            }
        }
}

    bool receiveFile(const string& filename, int totalSegments) {
        string folder = "clientFiles/";
        string filepath = folder + filename;

        // สร้าง/เช็คโฟลเดอร์แบบ POSIX (C++11)
        if (!ensureDir(folder)) {
            std::cerr << "[ERROR] Cannot create/access folder: " << folder << "\n";
            return false;
        }

        ofstream ofs(filepath, ios::binary);
        if(!ofs) {
            cerr << "[ERROR] Cannot open file: " << filepath << "\n";
            return false;
    }


        int expectedSeq = 0;
        int received = 0;

        cout << "\n=== Receiving file: " << filename << " (" << totalSegments << " segments expected) ===\n";

        while(true) {
            
            Segment* seg = receiveSegment();
            if(!seg) {
                cerr << "[TIMEOUT] No segment received for seq " << expectedSeq << ", sending NAK\n";
                sendNAK(expectedSeq);
                continue;
            }

            unsigned short recvChk = seg->header.checkSum;
            seg->header.checkSum = 0;
            if(recvChk != calculateChecksum(seg)) {
                cerr << "[ERROR] Segment " << seg->header.seqNumber << " checksum mismatch, sending NAK\n";
                sendNAK(expectedSeq);
                cleanup(seg);
                continue;
            }

            if(seg->payload) {
                MetaData* meta = (MetaData*)seg->payload;
                if(meta->type == TYPE_COMPLETE) {
                    cout << "[INFO] TYPE_COMPLETE received for seq " << seg->header.seqNumber
                        << ", total received segments: " << received << "\n";
                    sendACK(seg->header.seqNumber);
                    cleanup(seg);
                    break;
                }
            }

            int seq = seg->header.seqNumber;
            if(seq == expectedSeq) {
                if(seg->payload && seg->header.length > HEADER_SIZE) {
                    ofs.write((char*)seg->payload, seg->header.length - HEADER_SIZE);
                }
                cout << "[INFO] Received segment " << seq << ", sent ACK\n";
                sendACK(seq);
                expectedSeq++;
                received++;
            } else if(seq < expectedSeq) {
                cout << "[LOG] Receive Sequence Number: " << seq << ", but Expected for Sequence Number: " << expectedSeq << endl;
                cout << "[INFO] Duplicate segment " << seq << ", re-ACK sent\n";
                sendACK(seq);
            } else {
                cout << "[INFO] Future segment " << seq << ", sent NAK for seq " << expectedSeq << "\n";
                sendNAK(expectedSeq);
            }

            cleanup(seg);
        }

        ofs.close();
        cout << "[INFO] File saved to: " << filepath << "\n";
        return true;
    }



private:
    static void cleanup(Segment* seg) {
        if (!seg) return;
        if (seg->payload) delete[] (char*)seg->payload;
        delete seg;
    }

        static bool ensureDir(const std::string& path) {
        struct stat st{};
        if (stat(path.c_str(), &st) == 0) {
            return S_ISDIR(st.st_mode);          // มีอยู่และเป็นไดเรกทอรี
        }
        if (errno == ENOENT) {                    // ไม่มีอยู่ → สร้างใหม่
            return mkdir(path.c_str(), 0755) == 0;
        }
        return false;                             // มีข้อผิดพลาดอื่น
    }


};



int main(int argc, char* argv[]) {
    if (argc < 4) {
        cerr << "Usage: " << argv[0] << " <server_ip> <port> <file1> [file2 ...]" << endl;
        return 1;
    }

    string serverIp = argv[1];
    int port = atoi(argv[2]);

    try {
        ReliableUDPClient client(serverIp, port);

        for (int i = 3; i < argc; ++i) {
            string fname = argv[i];
            MetaData resp{};
            if (!client.requestFile(fname, resp)) {
                cerr << "Failed to get RESPONSE for file: " << fname << "\n";
                continue;
            }
            if (!client.receiveMeta(fname, resp)) {
                if (!resp.fileExists || resp.fileSize <= 0 || resp.totalSegments <= 0) {
                    cout << "Server reports file not found: " << fname << "\n";
                    continue;
                }
            }
            client.receiveFile(resp.filename[0] ? string(resp.filename) : fname, resp.totalSegments);
        }
    } catch (const exception& ex) {
        cerr << "Fatal: " << ex.what() << "\n";
        return 2;
    }

    return 0;
}
