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

    double timeoutSec = 0.000004;   // default 1s; can be tuned
    int maxRetries = 100000;   // retry for requests/NAKs
    int expectedSeq = -1; 

    int dropPercent;  
    int corruptPercent;

    int tryCount = 0;
public:
    ReliableUDPClient(const string& serverIp, int port, int dropP, int corruptP)
    : dropPercent(dropP), corruptPercent(corruptP)
    {
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
        cout << "DROP_PERCENT: " << dropPercent << "%\n";
        cout << "CORRUPT_PERCENT: " << corruptPercent << "%\n";
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

    void resetTryCount() {
        tryCount = 0;
    }

    // ---- wire helpers ----
    bool sendSegment(Segment* seg) {
        // counter++;
        // จำลองการ drop packet ทุก ๆ DROP_EVERY แพ็กเกต
        // if (counter % DROP_EVERY == 0) {
        //     cout << "[SIMULATED DROP] Dropping segment " << seg->header.seqNumber << endl;
        //     return false; // ไม่ส่งออกไป
        // }



        int r = rand() % 100; // ได้ค่า 0-99
        if (r < dropPercent)  {
            cout << "[SIMULATED DROP] Dropping segment " << seg->header.seqNumber << endl;
            return false; // ไม่ส่งออกไป
        }


        int c = rand() % 100;
        if (c < corruptPercent) {
            cout << "[SIMULATED CORRUPTION] Corrupting segment " << seg->header.seqNumber << endl;

            // ตัวอย่าง: แก้ไข checksum (ทำให้ผิดแน่ ๆ)
            seg->header.checkSum ^= 0xFFFF;

            // หรือสุ่มแก้ payload ถ้ามี
            int payloadSize = seg->header.length - HEADER_SIZE;
            if (payloadSize > 0 && seg->payload) {
                int pos = rand() % payloadSize;
                ((char*)seg->payload)[pos] ^= 0xFF; // flip bits ที่ตำแหน่งสุ่ม
            }
        }

        
        int totalSize = seg->header.length;
        char* buffer = new char[totalSize];
        

        memcpy(buffer, &seg->header, sizeof(Header));
        int payloadSize = seg->header.length - HEADER_SIZE;
        if (payloadSize > 0 && seg->payload) {
            memcpy(buffer + sizeof(Header), seg->payload, payloadSize);
        }

        sendto(sockfd, buffer, totalSize, 0, (sockaddr*)&serverAddr, serverLen);
        cout << "<<-- send segment with Sequence Number: " << seg->header.seqNumber << endl;
        delete[] buffer;
        return true;
    }

    // Segment* receiveSegment() {
    //     char buffer[sizeof(Header) + MAX_PAYLOAD_SIZE];
    //     sockaddr_in from{}; 
    //     socklen_t fromLen = sizeof(from);
    //     int n = recvfrom(sockfd, buffer, sizeof(buffer), 0, (sockaddr*)&from, &fromLen);
    //     if (n <= 0) return nullptr; // timeout or error

    //     Segment* seg = new Segment;
    //     memcpy(&seg->header, buffer, sizeof(Header));
    //     int payloadSize = seg->header.length - HEADER_SIZE;
    //     if (payloadSize > 0) {
    //         seg->payload = new char[payloadSize];
    //         memcpy(seg->payload, buffer + sizeof(Header), payloadSize);
    //     } else {
    //         seg->payload = nullptr;
    //     }
    //     cout << "--->> Client receive segment of Sequence Number: " << seg->header.seqNumber << endl;

    //     return seg;
    // }

    // Segment* receiveSegment() {        
        
    //     char buffer[sizeof(Header) + MAX_PAYLOAD_SIZE];

    //     // ✅ ตั้งค่า timeout ให้ recvfrom
    //     struct timeval tv;
    //     tv.tv_sec = (time_t)timeoutSec; // integer part
    //     tv.tv_usec = (suseconds_t)((timeoutSec - tv.tv_sec) * 1e6);
    //     if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv)) < 0) {
    //         perror("setsockopt failed");
    //         tryCount++;
    //         return nullptr;
    //     }
        
    //     sockaddr_in from{};
    //     socklen_t fromLen = sizeof(from);
        
    //     int n = recvfrom(sockfd, buffer, sizeof(buffer), 0, (sockaddr*)&from, &fromLen);
    //     if (n <= 0) {
    //         if (errno == EAGAIN || errno == EWOULDBLOCK) {
    //             cout << "[Timeout] No data received within 1 second" << endl;
    //             if (tryCount > 0) {
    //                cout << "Retry " << tryCount << "/" << maxRetries << endl;
    //             }
    //             tryCount++;
    //         } else {
    //             perror("recvfrom error");
    //         }
    //         return nullptr;
    //     }

    //     Segment* seg = new Segment;
    //     memcpy(&seg->header, buffer, sizeof(Header));

    //     int payloadSize = seg->header.length - HEADER_SIZE;
    //     if (payloadSize > 0) {
    //         seg->payload = new char[payloadSize];
    //         memcpy(seg->payload, buffer + sizeof(Header), payloadSize);
    //     } else {
    //         seg->payload = nullptr;
    //     }

    //     cout << "--->> Client received segment of Sequence Number: "
    //         << seg->header.seqNumber << " (Payload: " << payloadSize << " bytes)" << endl;

    //     return seg;
    // }

    Segment* receiveSegment(bool notRetryReceive) {        
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
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    cout << "[Timeout] No data received." << endl;
                    if (notRetryReceive) {
                        return nullptr;
                    }
                } else {
                    perror("recvfrom error");
                    return nullptr;
                }
            }
        }

        cout << "[ERROR] Max retries reached for receiveSegment\n";
        return nullptr;
    }


    void sendACK(int seq) {
        MetaData meta{}; meta.type = TYPE_ACK;
        Segment* ack = createSegment(&meta, seq, sizeof(MetaData));
        ack->header.checkSum = calculateChecksum(ack);
        
        // if(sendSegment(ack)) {
        // }
        
        sendSegment(ack);
        cout << "<<--- Client send ACK for Sequence Number:" << seq << endl;

        // while(true) {
        //     if (sendSegment(ack)) {
        //         cout << "<<--- Client send ACK for Sequence Number:" << seq << endl;
        //         break;
        //     }
        // }

        delete ack; // payload points to stack memory (meta), do not delete
    }

    void sendNAK(int seq) {
        MetaData meta{}; meta.type = TYPE_NAK;
        Segment* nak = createSegment(&meta, seq, sizeof(MetaData));
        nak->header.checkSum = calculateChecksum(nak);
        // sendSegment(nak);
        // if(sendSegment(nak)) {
        // }

        sendSegment(nak);
        cout << "[INFO] Client send NAK for Sequence Number:" << seq << endl;

        // while(true) {
        //     if (sendSegment(nak)) {
        //         cout << "[INFO] Client send NAK for Sequence Number:" << seq << endl;
        //         break;
        //     }
        // }

        delete nak; // payload points to stack memory (meta), do not delete
    }

    // ---- protocol ----
    // reliable send
    bool requestFile(const string& filename, MetaData& outRespMeta) {
        cout << "\n\n~~~~~~~~~~~~=== Requesting file: " << filename << " ===~~~~~~~~~~~~\n";

        MetaData req{};
        req.type = TYPE_REQUEST;
        memset(req.filename, 0, sizeof(req.filename));
        strncpy(req.filename, filename.c_str(), sizeof(req.filename) - 1);

        Segment* request = createSegment(&req, -1, sizeof(MetaData));
        request->header.checkSum = calculateChecksum(request);

        expectedSeq = -1;
        bool serverRespond = false;

        // tryCount = 0; // เริ่มนับ retry

        while (!serverRespond && tryCount < maxRetries) {
            tryCount++;
            cout << "[INFO] Sending request for file: " << filename 
                << " (Attempt " << tryCount << "/" << maxRetries << ")\n";
            sendSegment(request);

            Segment* seg = receiveSegment(true);  // จะ retry recvfrom ภายในเอง

            if (!seg) {
                cout << "[WARN] Timeout waiting for first ACK, resending request\n";
                continue; // loop จะ resend request

            }

            // ตรวจสอบ checksum
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
            cout << "[ERROR] Max retries reached for requestFile\n";
            return false;
        }

        return true;
    }


    bool receiveMeta(const string& filename, MetaData& outRespMeta) {
        cout << "\n=== Waiting for Metadata from server for file: " << filename << " ===\n";

        // int expectedSeq = 0;
        int retryCount = 0;
        bool serverRespond = false;

        while (true) {
            Segment* seg = receiveSegment(false);

            if (!seg) {
                return false;
            }
            // if (!seg) {
            //     retryCount++;
            //     cout << "[TIMEOUT] Waiting for RESPONSE, retry " << retryCount << "/" << maxRetries
            //         << " (expecting seq " << expectedSeq << ")\n";
            //     if (retryCount >= maxRetries) {
            //         cout << "[ERROR] Max retries exceeded. Aborting.\n";
            //         exit(1);
            //     }
            //     // sendNAK(expectedSeq);
            //     continue;
            // }
            // retryCount = 0;

            // isCorrupt?
            unsigned short recvChk = seg->header.checkSum;
            seg->header.checkSum = 0;
            if (recvChk != calculateChecksum(seg)) {
                cout << "[ERROR] RESPONSE checksum mismatch, sending NAK\n";
                sendNAK(expectedSeq);

                cleanup(seg);
                continue;
            }

            // isCorrectSeq?
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
                        // sendNAK(seg->header.seqNumber);
                        sendACK(seg->header.seqNumber);
                    } else {
                        cout << "[INFO] Future segment " << seg->header.seqNumber << endl;
                        sendNAK(seg->header.seqNumber);
                    }
                }
            }
            cleanup(seg);
        }
    }

    // bool receiveFile(const string& filename, int totalSegments) {
    //     string folder = "clientFiles/";
    //     string filepath = folder + filename;

    //     // สร้าง/เช็คโฟลเดอร์แบบ POSIX (C++11)
    //     if (!ensureDir(folder)) {
    //         cout << "[ERROR] Cannot create/access folder: " << folder << "\n";
    //         return false;
    //     }

    //     vector<char> buf; 
    //     buf.reserve(static_cast<size_t>(totalSegments) * MAX_PAYLOAD_SIZE);

    //     // int expectedSeq = 0;
    //     int received = 0;
    //     int retryCount = 0;

    //     cout << "\n=== Receiving file: " << filename << " (" << totalSegments << " segments expected) ===\n";

    //     while(true) {
    //         Segment* seg = receiveSegment(false);

    //         if (!seg) {
    //             continue;
    //         }

    //         // isCorrupt?
    //         unsigned short recvChk = seg->header.checkSum;
    //         seg->header.checkSum = 0;
    //         if(recvChk != calculateChecksum(seg)) {
    //             cout << "[ERROR] Segment " << seg->header.seqNumber << " checksum mismatch, sending NAK\n";
    //             sendNAK(seg->header.seqNumber);
    //             cleanup(seg);
    //             continue;
    //         }
            

    //         if(seg->payload) {
    //             MetaData* meta = (MetaData*)seg->payload;
                
    //             cout << "[Debug] meta->type = " << meta->type << endl;
    //             if(meta->type == TYPE_COMPLETE) {
    //                 cout << "[INFO] TYPE_COMPLETE received for seq " << seg->header.seqNumber 
    //                 << ", total received segments: " << received << "\n";
    //                 sendACK(seg->header.seqNumber);
    //                 cleanup(seg);
    //                 break;
    //             }
    //         }

    //         int seq = seg->header.seqNumber;
    //         if(seq == expectedSeq) {
    //             int psize = seg->header.length - HEADER_SIZE;
    //             if (psize > 0 && seg->payload) {
    //                 char* p = static_cast<char*>(seg->payload);
    //                 buf.insert(buf.end(), p, p + psize);
    //             }
    //             cout << "[INFO] Received segment " << seq << ", sent ACK\n";
    //             sendACK(seq);
    //             expectedSeq++;
    //             received++;
    //         } else if(seq < expectedSeq) {
    //             cout << "[LOG] Receive Sequence Number: " << seq << ", but Expected for Sequence Number: " << expectedSeq << endl;
    //             cout << "[INFO] Duplicate segment " << seq << ", re-ACK sent\n";
    //             sendACK(seq);
    //         } else {
    //             cout << "[INFO] Future segment " << seq << endl;
    //             // sendNAK(expectedSeq);
    //             continue;
    //         }

    //         cleanup(seg);
    //     }

    //     if (!buf.empty()) {
    //         writeFile(filepath, buf.data(), static_cast<int>(buf.size()));
    //         cout << "[INFO] File saved to: " << filepath << "\n";
    //         return true;
    //     } else {
    //         cout << "[WARN] No data payload collected for file: " << filepath << "\n";
    //         return false;
    //     }
   
    // }


    bool receiveFile(const string& filename, int totalSegments) {
        string folder = "clientFiles/";
        string filepath = folder + filename;
        if (!ensureDir(folder)) return false;

        vector<char> buf;
        buf.reserve(static_cast<size_t>(totalSegments) * MAX_PAYLOAD_SIZE);

        int expectedSeq = 0;
        int received = 0;

        cout << "=== Receiving file: " << filename << " ===\n";

        while (received <= totalSegments) {
            Segment* seg = receiveSegment(false); // retry within receiveSegment

            if (!seg) continue;

            unsigned short recvChk = seg->header.checkSum;
            seg->header.checkSum = 0;

            unsigned short calcChk = calculateChecksum(seg);

            // // Debug: print checksum
            // cout << "[DEBUG] Segment " << seg->header.seqNumber 
            //  << " recvChk=" << recvChk 
            //  << " calcChk=" << calcChk << endl;

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


                        char* p = static_cast<char*>(seg->payload);

                    

    // // ----
    //                     // Debug: print payload (hex หรือ char)
    //                     cout << "[DEBUG] Segment " << seq << " payload: ";
    //                     for (int i = 0; i < psize; ++i) {
    //                         // ถ้าอยากเห็นเป็นตัวอักษร
    //                         cout << p[i];
    //                         // หรือถ้าเป็น binary data: cout << hex << (int)(unsigned char)p[i] << " ";
    //                     }
    //                     cout << endl;

    //                     cout << "====" << endl;

    //                     unsigned char* q = static_cast<unsigned char*>(seg->payload);
    //                     cout << "[DEBUG] Segment " << seq << " payload (hex): ";
    //                     for (int i = 0; i < psize; ++i)
    //                         cout << hex << (int)q[i] << " ";
    //                     cout << dec << endl; // กลับไป decimal

    // // ---



                        
                        if (meta->type != TYPE_COMPLETE) {
                           buf.insert(buf.end(), p, p + psize);
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


        writeFile(filepath, buf.data(), static_cast<int>(buf.size()));
        cout << "[INFO] File saved to: " << filepath << "\n";
        return true;
    }


    bool receiveNonExist(const string& filename, MetaData& outRespMeta) {
        cout << "\n=== File Not Exist, Waiting for TYPE_COMPLETE from server for file: " << filename << " ===\n";

        int retryCount = 0;
        // int expectedSeq = 0;
        while (true) {
            Segment* seg = receiveSegment(false);
            
            if (!seg) {
                // cout << "[Debug] seg = " << seg  << ", !seg = " << !seg << endl;
                // retryCount++;
                // cout << "[TIMEOUT] Client did not receive segment ,waiting for segment, retry " << retryCount << "/" << maxRetries << endl;
                // if (retryCount >= maxRetries) {
                //     cout << "[ERROR] Max retries exceeded. Aborting.\n";
                //     exit(1);
                // }
                // sendNAK(seg->header.seqNumber);
                cout << "[IGNORE]" << endl;
                // continue;
                // continue;
                return false;
            }
            // retryCount = 0;

            unsigned short recvChk = seg->header.checkSum;
            seg->header.checkSum = 0;
            if (recvChk != calculateChecksum(seg)) {
                cout << "[ERROR] RESPONSE checksum mismatch, sending NAK\n";
                sendNAK(expectedSeq);

                cleanup(seg);
                continue;
            }

            cout << "[Debug B] seg->header.seqNumber= "  << seg->header.seqNumber <<
            ", expectedSeq = " << expectedSeq << endl;
            MetaData* meta = (MetaData*)seg->payload;
            cout << "[Debug] Server response: fileExists=" << meta->fileExists << ", fileSize=" << meta->fileSize << ", totalSegments=" << meta->totalSegments << ", maxPayload=" << meta->maxPayloadSize << ", type=" << meta->type << ", fileName=" << meta->filename << endl;

            if(seg->header.seqNumber == expectedSeq){
                cout << "[Debug] seg->payload= " << seg->payload << endl;
                cout << "[Debug] ((MetaData*)complete->payload)->type= " << ((MetaData*)seg->payload)->type << endl;
                if(seg->payload) {
                    MetaData* meta = (MetaData*)seg->payload;
                    cout << "[Debug] meta->type= " << meta->type << endl;
                    
                    if(meta->type == TYPE_COMPLETE && seg->header.seqNumber == expectedSeq) {                
                        outRespMeta = *meta;
                        cout << "[INFO] Server response: fileExists=" << meta->fileExists
                        << ", fileSize=" << meta->fileSize
                        << ", totalSegments=" << meta->totalSegments
                        << ", maxPayload=" << meta->maxPayloadSize << ", type=" << meta->type << "\n";
                        sendACK(seg->header.seqNumber);

                        cleanup(seg);
                        cout << "[SUCCESS] receiveNonExist success" << endl;
                        return true;
                    }
                }
            }
        }       
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
            
            // int retryCount = 0;

            if(!client.requestFile(fname, resp)) {
                return 1;
            }
            client.resetTryCount();

            // ตื่นมาจัดการ retry หน่อย
            // if (!client.requestFile(fname, resp)) {
            //     cout << "[ERROR] Failed sending request or waiting first ACK for file: " << fname << ", resent REQUEST..." << endl;
            //     // retryCount++;
            //     // if (!multiFiles) return 1;
            //     // continue;
            // }
            // retryCount = 0;
            // client.resetTryCount();
            
            while (!client.receiveMeta(fname, resp)) {
                cout << "[ERROR] Failed to receive RESPONSE metadata for file: " << fname << endl;
                // retryCount++;
                // cout << "Retry " << retryCount << "/" << 
                // if (!multiFiles) return 1;
                // continue;
            }
            // retryCount =0;
            client.resetTryCount();

            
            
            cout << "[Debug] resp.fileExists = " << resp.fileExists << endl;
            cout << "[Debug] multiFiles = " << multiFiles << endl;
            cout << "[Debug] resp.fileSize = " << resp.fileSize << endl;
            if (!resp.fileExists) {
                cout << "[INFO] Server reports file NOT FOUND: " << fname << endl;
                if (!multiFiles){
                    // client.receiveNonExist(fname, resp);
                    while(!client.receiveNonExist(fname, resp));
                    return 1;
                } 
                else {
                    while(!client.receiveNonExist(fname, resp));
                    continue;
                }
                // continue;
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