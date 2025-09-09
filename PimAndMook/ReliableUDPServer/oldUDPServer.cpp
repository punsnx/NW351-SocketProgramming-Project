#include "../packetize.hpp"
#include "../timeout.hpp"
#include "../logger.hpp"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cerrno>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

// #include <sys/types.h>      // ประเภทข้อมูลพื้นฐาน เช่น ssize_t
// #include <sys/socket.h>     // ฟังก์ชัน socket, bind, recvfrom, sendto
// #include <netinet/in.h>     // โครงสร้าง sockaddr_in
// #include <arpa/inet.h>      // ฟังก์ชันแปลง IP เช่น inet_pton
// #include <unistd.h>         // close()
// #include <cerrno>  

using namespace std;

namespace wire {

// --- Serializer: แปลง Segment -> contiguous bytes (network byte order) ---
static inline void serializeSegment(const Segment* seg, vector<char>& out) {
    unsigned short n_src = htons(seg->header.srcPort);
    unsigned short n_des = htons(seg->header.desPort);
    unsigned short n_len = htons(seg->header.length);
    unsigned short n_chk = htons(seg->header.checkSum);
    unsigned int   n_seq = htonl(seg->header.seqNumber);

    out.resize(seg->header.length);
    char* w = out.data();

    memcpy(w + 0,  &n_src, 2);
    memcpy(w + 2,  &n_des, 2);
    memcpy(w + 4,  &n_len, 2);
    memcpy(w + 6,  &n_chk, 2);
    memcpy(w + 8,  &n_seq, 4);

    int payloadLen = seg->header.length - HEADER_SIZE;
    if (payloadLen > 0 && seg->payload) {
        memcpy(w + HEADER_SIZE, seg->payload, payloadLen);
    }
}

// --- Parser: ดึง header + payload (เฉพาะกรณีรับ request ชื่อไฟล์) ---
struct ParsedHeader {
    unsigned short srcPort{};
    unsigned short desPort{};
    unsigned short length{};
    unsigned short checkSum{};
    unsigned int   seqNumber{};
};

static inline bool parseHeader(const char* buf, int nBytes, ParsedHeader& out) {
    if (nBytes < HEADER_SIZE) return false;

    unsigned short h_src, h_des, h_len, h_chk;
    unsigned int   h_seq;
    memcpy(&h_src, buf + 0, 2);
    memcpy(&h_des, buf + 2, 2);
    memcpy(&h_len, buf + 4, 2);
    memcpy(&h_chk, buf + 6, 2);
    memcpy(&h_seq, buf + 8, 4);

    out.srcPort  = ntohs(h_src);
    out.desPort  = ntohs(h_des);
    out.length   = ntohs(h_len);
    out.checkSum = ntohs(h_chk);
    out.seqNumber= ntohl(h_seq);

    return (out.length >= HEADER_SIZE && out.length <= nBytes);
}

} // namespace wire

// ------------------------------- UDP File Server -------------------------------
class UdpFileServer {
public:
    UdpFileServer(int port, int advertisedWindow, int io_timeout_ms)
        : port_(port), windowSize_(advertisedWindow), io_timeout_ms_(io_timeout_ms) {}

    int run() { 
        Logger logger;

        if (!openAndBind()) return 0;
        logger.log("Server listening on port " + to_string(port_) +
                   " (io_timeout=" + to_string(io_timeout_ms_) + " ms)");

        for (;;) {
            string filename;
            if (!receiveFilename(filename)) {
                continue;
            }
            logger.log("Client requested file: " + filename);

            if (!sendFile(filename)) {
                logger.logError("sendFile failed for: " + filename);
                continue;
            }
            logger.log("File sent successfully! (" + to_string(lastFileSize_) + " bytes)");
        }
        cleanClose();
        return 0;
    }

private:
    int sock_{-1};
    sockaddr_in serverAddr_{};
    sockaddr_in clientAddr_{};
    socklen_t addrLen_{sizeof(clientAddr_)};
    int port_{};
    int windowSize_{}; // ยังไม่ใช้ เผื่อ Sliding Window
    int lastFileSize_{0};
    int io_timeout_ms_{3000};  

private:
    // เปิด socket + bind
    bool openAndBind() {
        sock_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock_ < 0) {
            perror("socket"); //if error, show "socket: <system error message>"
            return false;
        }

        // เปิดใช้ timeout 
        set_socket_timeout_ms(sock_, /*recv_ms*/ 3000, /*send_ms*/ 3000);

        memset(&serverAddr_, 0, sizeof(serverAddr_));
        serverAddr_.sin_family = AF_INET;
        serverAddr_.sin_addr.s_addr = INADDR_ANY;
        serverAddr_.sin_port = htons(port_);

        if (bind(sock_, (sockaddr*)&serverAddr_, sizeof(serverAddr_)) < 0) {
            perror("bind");
            close(sock_);
            sock_ = -1;
            return false;
        }
        return true;
    }

    void cleanClose() {
        if (sock_ >= 0) close(sock_);
        sock_ = -1;
    }

    bool receiveFilename(string& filename) {
        while (true) {
            char reqBuf[HEADER_SIZE + 1024] = {0};
            int n = recvfrom(sock_, reqBuf, sizeof(reqBuf), 0,
                            (sockaddr*)&clientAddr_, &addrLen_);
            if (n <= 0) {
                if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                    // ไม่มีrequestในรอบนี้ → รอใหม่
                    continue;
                } else {
                    perror("recvfrom");
                    return false; // error
                }
            }

            wire::ParsedHeader hdr;
            if (!wire::parseHeader(reqBuf, n, hdr)) {
                cerr << "Bad request (invalid header/length)\n";
                // จะ continue รอใหม่ หรือ return false ก็ได้
                return false;
            }

            int nameBytes = hdr.length - HEADER_SIZE;
            if (nameBytes <= 0) { cerr << "Empty filename\n"; return false; }

            filename.assign(reqBuf + HEADER_SIZE, reqBuf + HEADER_SIZE + nameBytes);
            return true;
        }
    }

    bool sendFile(const string& filename) {
        auto rf = readFile(filename);
        if (!rf.first || rf.second <= 0) {
            cerr << "Cannot open file: " << filename << "\n";
            return false;
        }

        char* fileData = rf.first;
        int   fileSize = rf.second;
        lastFileSize_ = fileSize;

        vector<Segment*> segs = packetize((void*)fileData, fileSize, MAX_PAYLOAD_SIZE);

        bool ok = true;
        for (auto* seg : segs) {
            fillSegmentAddress(*seg);
            if (!sendSegment(*seg)) {
                ok = false;
                break;
            }
        }

        if (ok) {
            Segment fin{};
            fin.header.srcPort   = (unsigned short)port_;
            fin.header.desPort   = ntohs(clientAddr_.sin_port); 
            fin.header.length    = HEADER_SIZE;
            fin.header.seqNumber = (unsigned int)segs.size();
            if (!sendSegment(fin)) ok = false;
        }

        delete[] fileData;
        for (auto* s : segs) delete s;

        return ok;
    }

    void fillSegmentAddress(Segment& seg) const {
        seg.header.srcPort = (unsigned short)port_;
        seg.header.desPort = ntohs(clientAddr_.sin_port); 
    }

    bool sendSegment(const Segment& seg) {
        vector<char> wireBuf;
        wire::serializeSegment(&seg, wireBuf);
        int sent = sendto(sock_, wireBuf.data(), (int)wireBuf.size(), 0,
                          (sockaddr*)&clientAddr_, addrLen_);
        if (sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                cerr << "[Server] send timeout\n";
            } else {
                perror("sendto");
            }
            return false;
        }
        return true;
    }
};

// ----------------------------------- main -----------------------------------
int main(int argc, char* argv[]) {
    if (argc < 3) {
        cerr << "Usage: ./Server <port> <advertised_window> [io_timeout_ms]\n";
        return 1;
    }
    int port = atoi(argv[1]);
    int windowSize = atoi(argv[2]);
    int io_timeout_ms = 3000;
    if (argc >= 4) {
        io_timeout_ms = clamp_timeout_ms(atoll(argv[3]));
    }

    UdpFileServer server(port, windowSize, io_timeout_ms);
    return server.run();
}

