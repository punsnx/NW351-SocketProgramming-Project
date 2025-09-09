#pragma once
#include <cstdint>
#include <array>
#include <vector>
#include <cstddef>

namespace rudp {

// จาก README โครงการ: data ต่อ packet = 1450 ไบต์, header มี seq, ack, ack-flag. :contentReference[oaicite:3]{index=3}
constexpr std::size_t kMaxData = 1450;

struct PacketHeader {
  uint32_t seq;
  uint32_t ack;
  uint8_t  flags; // bit0 = ACK, bit1 = FIN (เพิ่มได้ภายหลัง)
} __attribute__((packed));

struct Segment {
  PacketHeader hdr{};
  std::array<uint8_t, kMaxData> data{};
  std::size_t data_len{0};
};

std::vector<uint8_t> serialize(const Segment& s);   // ไม่ยุ่ง socket
bool deserialize(const uint8_t* buf, std::size_t n, Segment* out);
} // namespace rudp
