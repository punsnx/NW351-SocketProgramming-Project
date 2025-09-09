#include "protocol.hpp"

#include "packet.hpp"
#include "checksum.hpp"
#include "timer.hpp"
#include "window.hpp"
#include "socket.hpp"

#include <unordered_map>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <chrono>
#include <algorithm>

namespace rudp {

namespace {
constexpr uint8_t FLAG_ACK = 0x01;
constexpr uint8_t FLAG_FIN = 0x02;

struct SentMeta {
  Segment seg;
  std::chrono::steady_clock::time_point t_send;
  std::vector<uint8_t> wire; // cached serialized bytes for retransmit
};

std::chrono::milliseconds safe_timeout(const RttEstimator& rtt) {
  auto t = rtt.timeout();
  if (t.count() <= 0) return std::chrono::milliseconds(500);
  return t;
}
} // namespace

// ------------------- SERVER -------------------
int run_server(const ServerOptions& opt) {
  try {
    UdpSocket sock;
    sock.bind(opt.port);
    sock.set_recv_timeout(std::chrono::milliseconds(500));
    std::cerr << "[server] listening on UDP :" << opt.port << "\n";

    // 1) wait for filename request
    std::vector<uint8_t> rbuf(2048);
    int n = sock.recv(rbuf.data(), rbuf.size());
    if (n <= 0) {
      std::cerr << "[server] no request\n";
      return 1;
    }
    const std::string filename(reinterpret_cast<char*>(rbuf.data()),
                               reinterpret_cast<char*>(rbuf.data() + n));
    std::cerr << "[server] request file: " << filename << "\n";
    std::ifstream fin(filename, std::ios::binary);
    if (!fin) {
      std::cerr << "[server] open file failed\n";
      // send FIN immediately (empty)
      Segment finseg{};
      finseg.hdr.flags = FLAG_FIN;
      auto w = serialize(finseg);
      sock.send(w.data(), w.size());
      return 2;
    }

    // components (ไม่มี congestion control)
    RttEstimator rtt;
    SlidingWindow win(static_cast<uint32_t>(opt.adv_window));

    uint32_t next_seq = 0;
    uint32_t ack_cum  = 0;
    bool eof = false;
    std::unordered_map<uint32_t, SentMeta> inflight; // seq -> meta

    // main loop
    while (true) {
      // 2) send while window allows — ใช้ budget = adv_window เท่านั้น
      std::size_t inflight_bytes = 0;
      for (auto& kv : inflight) inflight_bytes += kv.second.seg.data_len;
      std::size_t budget = opt.adv_window;

      while (!eof && inflight_bytes + kMaxData <= budget && win.can_send(kMaxData)) {
        // read chunk
        Segment seg{};
        fin.read(reinterpret_cast<char*>(seg.data.data()), kMaxData);
        std::streamsize got = fin.gcount();
        if (got <= 0) {
          eof = true;
          break;
        }
        seg.data_len = static_cast<std::size_t>(got);
        seg.hdr.seq  = next_seq;
        seg.hdr.ack  = 0;
        seg.hdr.flags = 0;

        auto wire = serialize(seg);
        if (sock.send(wire.data(), wire.size()) < 0) {
          std::cerr << "[server] send error\n";
        }
        SentMeta meta{seg, std::chrono::steady_clock::now(), wire};
        inflight[seg.hdr.seq] = std::move(meta);
        win.on_send(seg.hdr.seq, seg.data_len);
        next_seq += seg.data_len;
        inflight_bytes += seg.data_len;
      }

      // 3) if file ended and nothing inflight -> send FIN, wait for ACK then break
      if (eof && inflight.empty()) {
        Segment finseg{};
        finseg.hdr.flags = FLAG_FIN;
        finseg.hdr.seq   = next_seq;
        auto w = serialize(finseg);
        sock.send(w.data(), w.size());
        // wait a bit for client ACK (optional)
        int r = sock.recv(rbuf.data(), rbuf.size());
        if (r > 0) {
          Segment ackseg{};
          if (deserialize(rbuf.data(), r, &ackseg) && (ackseg.hdr.flags & FLAG_ACK)) {
            // ok
          }
        }
        std::cerr << "[server] done\n";
        break;
      }

      // 4) wait for ACK with timeout; on timeout -> retransmit oldest
      sock.set_recv_timeout(safe_timeout(rtt));
      int r = sock.recv(rbuf.data(), rbuf.size());
      if (r <= 0) {
        // timeout: retransmit oldest inflight
        if (!inflight.empty()) {
          auto oldest = std::min_element(
            inflight.begin(), inflight.end(),
            [](const auto& a, const auto& b){ return a.second.t_send < b.second.t_send; });
          std::cerr << "[server] timeout -> retransmit seq " << oldest->first << "\n";
          sock.send(oldest->second.wire.data(), oldest->second.wire.size());
          oldest->second.t_send = std::chrono::steady_clock::now();
          win.on_timeout();
        }
        continue;
      }

      Segment in{};
      if (!deserialize(rbuf.data(), r, &in)) {
        std::cerr << "[server] bad packet\n";
        continue;
      }
      if (in.hdr.flags & FLAG_ACK) {
        uint32_t new_ack = in.hdr.ack;
        if (new_ack > ack_cum) {
          // measure RTT using oldest covered seq
          uint32_t oldest_seq = 0;
          bool found = false;
          for (const auto& kv : inflight) {
            uint32_t seg_end = kv.first + static_cast<uint32_t>(kv.second.seg.data_len);
            if (new_ack >= seg_end) {
              if (!found || kv.first < oldest_seq) { oldest_seq = kv.first; found = true; }
            }
          }
          if (found) {
            auto now = std::chrono::steady_clock::now();
            auto dt  = now - inflight[oldest_seq].t_send;
            rtt.sample(std::chrono::duration_cast<std::chrono::milliseconds>(dt));
          }

          // drop acked
          std::vector<uint32_t> to_erase;
          for (auto& kv : inflight) {
            const uint32_t seg_end = kv.first + static_cast<uint32_t>(kv.second.seg.data_len);
            if (new_ack >= seg_end) to_erase.push_back(kv.first);
          }
          for (auto s : to_erase) inflight.erase(s);

          win.on_ack(new_ack);
          ack_cum = new_ack;
        } else {
          // dup ack (optional)
        }
      } else {
        // server shouldn't receive data; ignore
      }
    }

    return 0;
  } catch (const std::exception& e) {
    std::cerr << "[server] exception: " << e.what() << "\n";
    return 99;
  }
}

// ------------------- CLIENT -------------------
int run_client(const ClientOptions& opt) {
  try {
    UdpSocket sock;
    sock.connect_to(opt.host, opt.port);
    sock.set_recv_timeout(std::chrono::milliseconds(200));

    // 1) send filename request
    std::vector<uint8_t> req(opt.file.begin(), opt.file.end());
    sock.send(req.data(), req.size());
    std::cerr << "[client] requested: " << opt.file << "\n";

    // 2) prepare output file: write to "<name>.out"
    std::string outname = opt.file + ".out";
    std::ofstream fout(outname, std::ios::binary);
    if (!fout) {
      std::cerr << "[client] cannot open output file\n";
      return 2;
    }

    uint32_t expected = 0;
    std::vector<uint8_t> buf(2048 + kMaxData);

    for (;;) {
      int n = sock.recv(buf.data(), buf.size());
      if (n <= 0) {
        // ask again with dup ACK to probe
        Segment ack{};
        ack.hdr.flags = FLAG_ACK;
        ack.hdr.ack   = expected;
        auto w = serialize(ack);
        sock.send(w.data(), w.size());
        continue;
      }

      Segment seg{};
      if (!deserialize(buf.data(), n, &seg)) {
        std::cerr << "[client] bad packet\n";
        continue;
      }

      if (seg.hdr.flags & FLAG_FIN) {
        // finalize
        Segment ack{};
        ack.hdr.flags = FLAG_ACK;
        ack.hdr.ack   = expected;
        auto w = serialize(ack);
        sock.send(w.data(), w.size());
        std::cerr << "[client] FIN received, wrote: " << outname << "\n";
        break;
      }

      if (seg.hdr.seq == expected) {
        // in-order
        if (seg.data_len > 0) {
          fout.write(reinterpret_cast<const char*>(seg.data.data()),
                     static_cast<std::streamsize>(seg.data_len));
          expected += static_cast<uint32_t>(seg.data_len);
        }
      } else {
        // out-of-order; ignore payload but ACK current expected (cumulative)
      }

      // send ACK (cumulative)
      Segment ack{};
      ack.hdr.flags = FLAG_ACK;
      ack.hdr.ack   = expected;
      auto w = serialize(ack);
      sock.send(w.data(), w.size());
    }

    return 0;
  } catch (const std::exception& e) {
    std::cerr << "[client] exception: " << e.what() << "\n";
    return 99;
  }
}

} // namespace rudp
