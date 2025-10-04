// client_multi.cc
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <chrono>
#include <map>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "header/project-header.h"  // ต้องมี Header, Segment, MAX_PAYLOAD_SIZE, HEADER_SIZE,
// และฟังก์ชัน createSegment(Segment* or factory), calculateChecksum(Segment*)

using namespace std;

// ---- ต้องให้สอดคล้องกับ server.cc / diagram ----
#define TYPE_REQUEST  1
#define TYPE_RESPONSE 2
#define TYPE_DATA     3
#define TYPE_ACK      4
#define TYPE_NAK      5
#define TYPE_COMPLETE 6

struct MetaData {
    int  type;
    char filename[256];
    bool fileExists;
    int  fileSize;
    int  totalSegments;
    int  windowSize;
    int  maxPayloadSize;
};

class ReliableUDPClient {
public:
    ReliableUDPClient(const string& ip, int port, int timeoutSec = 1, int maxRetries = 3)
        : serverIp(ip), serverPort(port), timeoutSeconds(timeoutSec), maxRetries(maxRetries) {
        sockfd = -1;
        memset(&serverAddr, 0, sizeof(serverAddr));
    }

    bool init() {
        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd < 0) {
            cerr << "Error: cannot create socket\n";
            return false;
        }
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port   = htons(serverPort);
        if (inet_pton(AF_INET, serverIp.c_str(), &serverAddr.sin_addr) <= 0) {
            cerr << "Error: invalid server ip\n";
            return false;
        }
        return true;
    }

    bool requestAndDownload(const string& filename) {
        // 1) ส่งคำขอ
        MetaData req{};
        req.type = TYPE_REQUEST;
        strncpy(req.filename, filename.c_str(), sizeof(req.filename)-1);

        Segment* seg = createSegment(&req, /*seq*/0, sizeof(MetaData));
        seg->header.checkSum = 0;
        seg->header.checkSum = calculateChecksum(seg);
        if (!sendSegment(seg)) { 
            cleanup(seg);
            cerr << "[REQ] send failed for '" << filename << "'\n";
            return false;
        }
        cleanup(seg);

        // 2) รอ RESPONSE
        MetaData resp{};
        int respSeq = -1;
        if (!waitResponse(resp, respSeq)) {
            cerr << "[RES] invalid/missing RESPONSE for '" << filename << "'\n";
            return false;
        }
        // ACK RESPONSE เพื่อให้เซิร์ฟเวอร์เริ่มส่งข้อมูล
        sendACK(respSeq);

        if (!resp.fileExists) {
            cout << "Server: file not found → " << filename << "\n";
            return false;
        }

        // 3) เตรียมเขียนไฟล์
        ofstream out(filename, ios::binary);
        if (!out) {
            cerr << "Error: cannot open output '" << filename << "'\n";
            return false;
        }
        cout << "Downloading '" << filename << "' "
             << "(size=" << resp.fileSize
             << ", segments=" << resp.totalSegments
             << ", maxPayload=" << resp.maxPayloadSize << ")\n";

        // 4) รับ DATA/COMPLETE
        int expectedSeq = 0;
        int writtenBytes = 0;

        timeval tv{.tv_sec = timeoutSeconds, .tv_usec = 0};
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        // Receive packet p
        while (true) {
            Segment* inSeg = receiveSegment();
            if (!inSeg) {
                // timeout
                cerr << "Timeout waiting seq=" << expectedSeq
                     << " for '" << filename << "' → NAK\n";
                sendNAK(expectedSeq);
                continue;
            }

            // Is Corrupt?
            unsigned short recvCsum = inSeg->header.checkSum;
            inSeg->header.checkSum = 0;
            unsigned short calcCsum = calculateChecksum(inSeg);

            if (recvCsum != calcCsum) {
                cerr << "Corrupt segment (seq=" << inSeg->header.seqNumber
                     << ") for '" << filename << "' → NAK\n";
                sendNAK(expectedSeq);
                cleanup(inSeg);
                continue;
            }

            // COMPLETE?
            bool isComplete = false;
            int psize = inSeg->header.length - HEADER_SIZE;
            if (psize >= (int)sizeof(MetaData) && inSeg->payload) {
                MetaData* meta = (MetaData*)inSeg->payload;
                if (meta->type == TYPE_COMPLETE) {
                    isComplete = true;
                }
            }

            if (isComplete) {
                sendACK(inSeg->header.seqNumber);
                cleanup(inSeg);
                cout << "Completed '" << filename << "' (" << writtenBytes << " bytes)\n";
                break;
            }

            //isCorrect Seq?
            int seq = inSeg->header.seqNumber;
            if (seq == expectedSeq) {
                if (psize > 0 && inSeg->payload) {
                    out.write(inSeg->payload, psize);
                    writtenBytes += psize;
                }
                sendACK(seq);
                expectedSeq++;
            } else {
                cerr << "Out-of-order for '" << filename
                     << "': expected=" << expectedSeq << " got=" << seq << " → NAK\n";
                sendNAK(expectedSeq);
            }

            cleanup(inSeg);
        }

        out.close();
        return true;
    }

private:
    int sockfd;
    sockaddr_in serverAddr;
    socklen_t addrLen = sizeof(serverAddr);
    string serverIp;
    int serverPort;
    int timeoutSeconds;
    int maxRetries;

    // ----- I/O primitives -----
    bool sendRaw(const char* buf, int len) {
        int n = sendto(sockfd, buf, len, 0, (sockaddr*)&serverAddr, addrLen);
        return n == len;
    }

    bool sendSegment(Segment* seg) {
        int totalSize = seg->header.length;
        vector<char> buf(totalSize);
        memcpy(buf.data(), &seg->header, sizeof(Header));
        int psize = totalSize - HEADER_SIZE;
        if (psize > 0 && seg->payload) {
            memcpy(buf.data() + sizeof(Header), seg->payload, psize);
        }
        return sendRaw(buf.data(), totalSize);
    }

    Segment* receiveSegment() {
        char buf[sizeof(Header) + MAX_PAYLOAD_SIZE];
        int n = recvfrom(sockfd, buf, sizeof(buf), 0, (sockaddr*)&serverAddr, &addrLen);
        if (n <= 0) return nullptr;

        Segment* seg = new Segment;
        memcpy(&seg->header, buf, sizeof(Header));
        int psize = seg->header.length - HEADER_SIZE;
        if (psize > 0) {
            seg->payload = new char[psize];
            memcpy(seg->payload, buf + sizeof(Header), psize);
        } else {
            seg->payload = nullptr;
        }
        return seg;
    }

    // ----- helpers -----
    void sendACK(int seqNum) {
        MetaData m{}; m.type = TYPE_ACK;
        Segment* ack = createSegment(&m, seqNum, sizeof(int));
        ack->header.checkSum = 0;
        ack->header.checkSum = calculateChecksum(ack);
        sendSegment(ack);
        cleanup(ack);
    }

    void sendNAK(int seqNum) {
        MetaData m{}; m.type = TYPE_NAK;
        Segment* nak = createSegment(&m, seqNum, sizeof(int));
        nak->header.checkSum = 0;
        nak->header.checkSum = calculateChecksum(nak);
        sendSegment(nak);
        cleanup(nak);
    }

    bool waitResponse(MetaData& outMeta, int& outSeq) {
        timeval tv{.tv_sec = timeoutSeconds, .tv_usec = 0}; //set timeout
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        const int expectedRespSeq = 0; //++

        int tries = 0;
        //limit timeout
        while (tries < maxRetries) {
            Segment* seg = receiveSegment();
            //timeout?
            if (!seg) {
                cerr << "Timeout waiting RESPONSE (" << (tries+1) << "/" << maxRetries << ")\n";
                tries++;
                continue;
            }

            //Is Corrupt?
            unsigned short recvCsum = seg->header.checkSum;
            seg->header.checkSum = 0;
            unsigned short calcCsum = calculateChecksum(seg);
            if (recvCsum != calcCsum) {
                cerr << "Corrupt RESPONSE → NAK\n";
                sendNAK(expectedRespSeq); // sendNAK(seg->header.seqNumber); //++
                cleanup(seg);
                tries++;
                continue;
            }

            //isCorrect Seq?
            if (seg->header.seqNumber != expectedRespSeq) {
                cerr << "Unexpected RESPONSE seq=" << seg->header.seqNumber
                    << " (expected " << expectedRespSeq << ") → NAK\n";
                sendNAK(expectedRespSeq);
                cleanup(seg);
                tries++;
                continue;
            }
            

            // isAck ?
            int psize = seg->header.length - HEADER_SIZE;
            if (psize < (int)sizeof(MetaData) || !seg->payload) {
                cerr << "RESPONSE payload too small → NAK\n";
                sendNAK(expectedRespSeq);
                cleanup(seg);
                tries++;
                continue;
            }

            MetaData* meta = (MetaData*)seg->payload;
            if (meta->type != TYPE_RESPONSE) {
                cerr << "Unexpected first packet (not TYPE_RESPONSE) → NAK\n";
                sendNAK(expectedRespSeq);
                cleanup(seg);
                tries++;
                continue;
            }

            // Pass
            outMeta = *meta;
            outSeq  = seg->header.seqNumber; // จะเป็น 0 ตาม expectedRespSeq
            cleanup(seg);
            return true;
        }
        return false;
    }

    void cleanup(Segment* seg) {
        if (!seg) return;
        if (seg->payload) delete[] seg->payload;
        delete seg;
    }
};

int main(int argc, char* argv[]) {
    if (argc < 4) {
        cerr << "Usage: " << argv[0] << " <server_ip> <port> <file1> [file2 ...]\n";
        return 1;
    }
    string ip = argv[1];
    int port  = atoi(argv[2]);

    ReliableUDPClient client(ip, port);
    if (!client.init()) return 1; // ฟังก์ชันเตรียมการ (สร้าง UDP socket, เซ็ตที่อยู่เซิร์ฟเวอร์ ) 1 -> หยุดโปรแกรม, 0 -> สำเร็จ

    // เก็บผลลัพธ์ต่อไฟล์
    map<string, bool> results;

    for (int i = 3; i < argc; ++i) {
        string file = argv[i];
        cout << "==== Start: " << file << " ====\n";
        bool ok = client.requestAndDownload(file);
        results[file] = ok;
        cout << "==== End: " << file << " → " << (ok ? "OK" : "FAIL") << " ====\n\n";

    }

    // สรุปผลรวม
    cout << "========== SUMMARY ==========\n";
    int okCount = 0, failCount = 0;
    for (auto& kv : results) {
        cout << kv.first << " : " << (kv.second ? "OK" : "FAIL") << "\n";
        kv.second ? okCount++ : failCount++;
    }
    cout << "Total: " << results.size()
         << " | OK=" << okCount << " | FAIL=" << failCount << "\n";

    return (failCount == 0) ? 0 : 2;
}
