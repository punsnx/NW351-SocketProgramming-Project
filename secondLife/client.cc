#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>

#include "header/project-header.h"      // expects Header, Segment, MAX_PAYLOAD_SIZE, HEADER_SIZE
#include "header/packetize.h"           // for createSegment()
#include "header/checksum.h"            // for calculateChecksum()

using namespace std;

// Message types (aligned with server.cc)
#define TYPE_REQUEST  1
#define TYPE_RESPONSE 2
#define TYPE_DATA     3   // not used by server for data payloads, but kept for completeness
#define TYPE_ACK      4
#define TYPE_NAK      5
#define TYPE_COMPLETE 6

struct MetaData {
    int type;                 // TYPE_*
    char filename[256];       // requested/serving file name
    bool fileExists;          // server response: file present?
    int fileSize;             // bytes
    int totalSegments;        // how many segments server created
    int windowSize;           // optional (not used in stop-and-wait server)
    int maxPayloadSize;       // MAX_PAYLOAD_SIZE announced by server
};

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
        delete[] buffer;
    }

    Segment* receiveSegment() {
        char buffer[sizeof(Header) + MAX_PAYLOAD_SIZE];
        sockaddr_in from{}; socklen_t fromLen = sizeof(from);
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
        return seg;
    }

    void sendACK(int seq) {
        MetaData meta{}; meta.type = TYPE_ACK;
        Segment* ack = createSegment(&meta, seq, (int)sizeof(int));
        ack->header.checkSum = calculateChecksum(ack);
        sendSegment(ack);
        delete ack; // payload points to stack memory (meta), do not delete
    }

    void sendNAK(int seq) {
        MetaData meta{}; meta.type = TYPE_NAK;
        Segment* nak = createSegment(&meta, seq, (int)sizeof(int));
        nak->header.checkSum = calculateChecksum(nak);
        sendSegment(nak);
        delete nak; // payload points to stack memory (meta), do not delete
    }

    // Send a segment and wait for ACK; used for the initial request (mirrors server's reliable send)
    bool sendReliable(Segment* segToSend) {
        for (int tries = 0; tries < maxRetries; ++tries) {
            sendSegment(segToSend);
            // wait for ACK/NAK
            Segment* resp = receiveSegment();
            if (!resp) {
                cerr << "[request] timeout, retry " << (tries+1) << "/" << maxRetries << "\n";
                continue;
            }

            unsigned short rx = resp->header.checkSum;
            resp->header.checkSum = 0;
            unsigned short calc = calculateChecksum(resp);
            bool ok = (rx == calc);

            if (ok && resp->payload) {
                MetaData* m = (MetaData*)resp->payload;
                if (m->type == TYPE_ACK) {
                    // got ACK for our request
                    delete[] (char*)resp->payload; delete resp;
                    return true;
                }
            }
            // corrupt or NAK -> retry
            if (resp->payload) delete[] (char*)resp->payload; delete resp;
        }
        return false;
    }

    // ---- protocol ----
    bool requestFile(const string& filename, MetaData& outRespMeta) {
        cout << "\n=== Requesting file: " << filename << " ===\n";

        MetaData req{};
        req.type = TYPE_REQUEST;
        memset(req.filename, 0, sizeof(req.filename));
        strncpy(req.filename, filename.c_str(), sizeof(req.filename) - 1);

        Segment* request = createSegment(&req, /*seq*/0, (int)sizeof(MetaData));
        request->header.checkSum = calculateChecksum(request);

        // Send the request (server expects us to ACK its RESPONSE; we expect nothing here)
        sendSegment(request);
        delete request; // payload is stack req

        // Wait for RESPONSE from server and ACK it
        for (;;) {
            Segment* seg = receiveSegment();
            if (!seg) {
                cerr << "[response] timeout waiting for server RESPONSE\n";
                return false;
            }
            unsigned short rx = seg->header.checkSum;
            seg->header.checkSum = 0;
            unsigned short calc = calculateChecksum(seg);
            if (rx != calc) {
                cerr << "[response] checksum error, sending NAK\n";
                sendNAK(seg->header.seqNumber);
                cleanup(seg);
                continue; // wait again
            }

            int psize = seg->header.length - HEADER_SIZE;
            if (psize == (int)sizeof(MetaData) && seg->payload) {
                MetaData* meta = (MetaData*)seg->payload;
                if (meta->type == TYPE_RESPONSE) {
                    // ACK the RESPONSE so server can proceed
                    sendACK(seg->header.seqNumber);
                    outRespMeta = *meta; // copy out for caller
                    cout << "Server says fileExists=" << boolalpha << meta->fileExists
                         << ", fileSize=" << meta->fileSize
                         << ", totalSegments=" << meta->totalSegments
                         << ", maxPayload=" << meta->maxPayloadSize << "\n";
                    cleanup(seg);
                    return true;
                }
            }
            // Unexpected packet: NAK and continue
            cerr << "[response] unexpected packet, NAK and wait\n";
            sendNAK(seg->header.seqNumber);
            cleanup(seg);
        }
    }

    bool receiveFile(const string& filename, int totalSegments) {
        ofstream ofs(filename, ios::binary);
        if (!ofs) {
            cerr << "Cannot open output file: " << filename << "\n";
            return false;
        }

        int expectedSeq = 0;
        int received = 0;

        cout << "Receiving data segments... expecting " << totalSegments << " segments\n";

        while (true) {
            Segment* seg = receiveSegment();
            if (!seg) {
                // timeout: ask for retransmission of the expected segment
                cerr << "[data] timeout, NAK seq " << expectedSeq << "\n";
                sendNAK(expectedSeq);
                continue;
            }

            unsigned short rx = seg->header.checkSum;
            seg->header.checkSum = 0;
            unsigned short calc = calculateChecksum(seg);
            if (rx != calc) {
                cerr << "[data] checksum error on seq " << seg->header.seqNumber << ", NAK expected " << expectedSeq << "\n";
                sendNAK(expectedSeq);
                cleanup(seg);
                continue;
            }

            int psize = seg->header.length - HEADER_SIZE;
            bool isMetaSized = (psize == (int)sizeof(MetaData));
            if (isMetaSized && seg->payload) {
                MetaData* meta = (MetaData*)seg->payload;
                if (meta->type == TYPE_COMPLETE) {
                    cout << "Got TYPE_COMPLETE. Received segments: " << received << "\n";
                    // ACK completion (optional)
                    sendACK(seg->header.seqNumber);
                    cleanup(seg);
                    break;
                }
            }

            // Treat as data segment
            int seq = seg->header.seqNumber;
            if (seq == expectedSeq) {
                if (psize > 0 && seg->payload) {
                    ofs.write((char*)seg->payload, psize);
                }
                sendACK(seq);
                expectedSeq++;
                received++;
            } else if (seq < expectedSeq) {
                // duplicate old segment -> ACK it again so server proceeds
                sendACK(seq);
            } else {
                // future segment -> ask for the missing one
                sendNAK(expectedSeq);
            }
            cleanup(seg);
        }

        ofs.close();
        cout << "File saved to: " << filename << "\n";
        return true;
    }

private:
    static void cleanup(Segment* seg) {
        if (!seg) return;
        if (seg->payload) delete[] (char*)seg->payload;
        delete seg;
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
            if (!resp.fileExists || resp.fileSize <= 0 || resp.totalSegments <= 0) {
                cout << "Server reports file not found: " << fname << "\n";
                continue;
            }
            client.receiveFile(resp.filename[0] ? string(resp.filename) : fname, resp.totalSegments);
        }
    } catch (const exception& ex) {
        cerr << "Fatal: " << ex.what() << "\n";
        return 2;
    }

    return 0;
}
