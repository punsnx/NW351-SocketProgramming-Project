#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>
#include "packet.hpp"

namespace rudp {
class SlidingWindow {
 public:
  explicit SlidingWindow(uint32_t adv_window_bytes);
  bool can_send(std::size_t seg_bytes) const;
  void on_send(uint32_t seq, std::size_t seg_bytes);
  void on_ack(uint32_t ack_cumulative); // cum-ACK
  void on_timeout();
  uint32_t next_seq() const;

  // เก็บ out-of-order / dupACK สำหรับ fast retransmit
  std::vector<uint32_t> lost_candidates(uint32_t ack_cum) const;

 private:
  uint32_t base_seq_{0};
  uint32_t next_seq_{0};
  uint32_t adv_win_{0};
  std::unordered_map<uint32_t, std::size_t> inflight_; // seq->len
};
}
