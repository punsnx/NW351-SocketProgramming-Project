#include "window.hpp"
#include <algorithm>

namespace rudp {

SlidingWindow::SlidingWindow(uint32_t adv_window_bytes)
  : adv_win_(adv_window_bytes) {}

bool SlidingWindow::can_send(std::size_t seg_bytes) const {
  std::size_t inflight_bytes = 0;
  for (const auto& kv : inflight_) inflight_bytes += kv.second;
  return (inflight_bytes + seg_bytes) <= adv_win_;
}

void SlidingWindow::on_send(uint32_t seq, std::size_t seg_bytes) {
  if (inflight_.empty()) base_seq_ = seq;
  next_seq_ = std::max(next_seq_, seq + static_cast<uint32_t>(seg_bytes));
  inflight_[seq] = seg_bytes;
}

void SlidingWindow::on_ack(uint32_t ack_cumulative) {
  // remove all segments fully covered by cumulative ACK
  std::vector<uint32_t> to_erase;
  for (const auto& kv : inflight_) {
    const uint32_t seg_seq = kv.first;
    const uint32_t seg_end = seg_seq + static_cast<uint32_t>(kv.second);
    if (ack_cumulative >= seg_end) {
      to_erase.push_back(seg_seq);
    }
  }
  for (auto s : to_erase) inflight_.erase(s);

  // advance base
  if (inflight_.empty()) {
    base_seq_ = ack_cumulative;
  } else {
    // base = smallest seq in inflight
    uint32_t min_seq = inflight_.begin()->first;
    for (const auto& kv : inflight_) min_seq = std::min(min_seq, kv.first);
    base_seq_ = min_seq;
  }
}

void SlidingWindow::on_timeout() {
  // nothing to update except we keep the inflight; protocol will retransmit
}

uint32_t SlidingWindow::next_seq() const { return next_seq_; }

std::vector<uint32_t> SlidingWindow::lost_candidates(uint32_t ack_cum) const {
  std::vector<uint32_t> lost;
  for (const auto& kv : inflight_) {
    if (kv.first < ack_cum) lost.push_back(kv.first);
  }
  std::sort(lost.begin(), lost.end());
  return lost;
}

} // namespace rudp
