#include "../packetize.hpp"
#include "../timeout.hpp"
#include "../logger.hpp"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <cerrno>

#include <cstring>
#include <string>
#include <vector>
#include <iostream>

using namespace std;


namespace wire {

// --- Serializer: Segment -> bytes (network byte order) ---
static inline void serializeSegment(const Segment* seg, vector<char>& out) {
    unsigned short n_src  = htons(seg->header.srcPort);
    unsigned short n_des  = htons(seg->header.desPort);
    unsigned short n_len  = htons(seg->header.length);
    unsigned short n_chk  = htons(seg->header.checkSum);
    unsigned int   n_seq  = htonl(seg->header.seqNumber);

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

// --- Header parser: bytes -> host-order fields ---
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

    out.srcPort   = ntohs(h_src);
    out.desPort   = ntohs(h_des);
    out.length    = ntohs(h_len);
    out.checkSum  = ntohs(h_chk);
    out.seqNumber = ntohl(h_seq);

    return (out.length >= HEADER_SIZE && out.length <= (unsigned)nBytes);
}

} // namespace wire

// -------------------------------- UDP File Client ------------------------------
class UdpFileClient {
public:
    UdpFileClient(const string& serverIp, int port, const string& filename,
              int advertisedWindow, int io_timeout_ms)
    : serverIp_(serverIp), port_(port), filename_(filename),
      windowSize_(advertisedWindow), io_timeout_ms_(io_timeout_ms) {}

    int run() {
        Logger logger;

        if (!openSocket()) return 1;
        if (!sendRequest(filename_)) { cleanClose(); return 1; }

        vector<Segment*> segs;
        if (!receiveAllSegments(segs)) {
            freeSegments(segs);
            cleanClose();
            logger.logError("Client terminated (recv timeout or error)");
            return 1;
        }

        auto joined = resemble(segs);
        bool ok = writeFile(filename_, joined.first, joined.second);

        if (!ok) {
            logger.logError("Cannot write file");
        } else {
            logger.log("File received successfully! (" + to_string(joined.second) + " bytes)");
        }

        delete[] joined.first;
        freeSegments(segs);
        cleanClose();
        return ok ? 0 : 1;
    }

private:
    int sock_{-1};
    sockaddr_in serverAddr_{};
    socklen_t addrLen_{sizeof(serverAddr_)};
    string serverIp_;
    int port_{};
    string filename_;
    int windowSize_{}; // ยังไม่ใช้ เผื่อ Sliding Window
    int io_timeout_ms_{3000}; 

private:
    // --- lifecycle ---
    bool openSocket() {
        sock_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock_ < 0) { perror("socket"); return false; }

        // เปิดใช้ timeout  
        set_socket_timeout_ms(sock_, io_timeout_ms_, io_timeout_ms_);

        memset(&serverAddr_, 0, sizeof(serverAddr_));
        serverAddr_.sin_family = AF_INET;
        serverAddr_.sin_port   = htons(port_);
        if (inet_pton(AF_INET, serverIp_.c_str(), &serverAddr_.sin_addr) != 1) {
            cerr << "Invalid server IP\n";
            close(sock_); sock_ = -1;
            return false;
        }

        return true;
    }

    void cleanClose() {
        cout << "[Client] Closing socket fd=" << sock_ << endl;
        if (sock_ >= 0) close(sock_);
        sock_ = -1;
    }

    // --- protocol steps ---
    bool sendRequest(const string& filename) {
        Segment req{};
        req.header.srcPort = (unsigned short)port_;      // ยังไม่ใช้
        req.header.desPort = (unsigned short)port_;
        req.header.seqNumber = 0;
        req.header.length = HEADER_SIZE + (unsigned short)filename.size();
        req.payload = (void*)filename.data(); 

        vector<char> wire;
        wire::serializeSegment(&req, wire);

        int sent = sendto(sock_, wire.data(), (int)wire.size(), 0,
                          (sockaddr*)&serverAddr_, addrLen_);
        if (sent < 0) {
            perror("sendto(request)");
            return false;
        }
        return true;
    }

    bool receiveAllSegments(vector<Segment*>& outSegs) {
        while (true) {
            vector<char> buf(HEADER_SIZE + MAX_PAYLOAD_SIZE);
            int n = recvfrom(sock_, buf.data(), (int)buf.size(), 0,
                             (sockaddr*)&serverAddr_, &addrLen_);
        
            //timeout
            if (n < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    cerr << "[Client] recv timeout\n";
                } else {
                    perror("recvfrom");
                }
                return false;
            }

            if (n < HEADER_SIZE) { cerr << "Short packet\n"; return false; }

            wire::ParsedHeader hdr;
            if (!wire::parseHeader(buf.data(), n, hdr)) {
                cerr << "Length mismatch\n";
                return false;
            }

            int payloadLen = hdr.length - HEADER_SIZE;

            // FIN packet: payloadLen == 0 → จบไฟล์
            if (payloadLen == 0) {
                // ถ้าต้องการความเข้ม อาจตรวจว่า hdr.seqNumber == outSegs.size()
                break;
            }

            char* copyBuf = new char[payloadLen];
            memcpy(copyBuf, buf.data() + HEADER_SIZE, payloadLen);

            Segment* seg = new Segment;
            seg->header.srcPort    = hdr.srcPort;
            seg->header.desPort    = hdr.desPort;
            seg->header.length     = hdr.length;
            seg->header.checkSum   = hdr.checkSum;
            seg->header.seqNumber  = hdr.seqNumber;
            seg->payload           = copyBuf;

            outSegs.push_back(seg);
        }
        return true;
    }

    // --- utils ---
    static void freeSegments(vector<Segment*>& segs) {
        for (auto* s : segs) {
            delete[] (char*)s->payload;
            delete s;
        }
        segs.clear();
    }
};

// ----------------------------------- main -----------------------------------
int main(int argc, char* argv[]) {
    if (argc < 5) {
        cerr << "Usage: ./Client <server_ip> <port> <filename> <advertised_window> [io_timeout_ms]\n";
        return 1;
    }
    const char* serverIP  = argv[1];
    int port              = atoi(argv[2]);
    const char* filename  = argv[3];
    int windowSize        = atoi(argv[4]);
    int io_timeout_ms     = 3000;
    if (argc >= 6) {
        io_timeout_ms = clamp_timeout_ms(atoll(argv[5]));
    }

    UdpFileClient client(serverIP, port, filename, windowSize, io_timeout_ms);
    return client.run();
}