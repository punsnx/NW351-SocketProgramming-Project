#include "checksum.hpp"

namespace rudp {

static inline uint32_t fold32(uint32_t sum) {
  // fold 32->16: add carries
  while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
  return sum;
}

uint16_t internet_checksum(const uint8_t* data, std::size_t len) {
  uint32_t sum = 0;
  const uint16_t* p = reinterpret_cast<const uint16_t*>(data);

  // sum 16-bit words
  while (len > 1) {
    sum += *p++;
    len -= 2;
  }
  // last odd byte
  if (len == 1) {
    uint16_t last = 0;
    *reinterpret_cast<uint8_t*>(&last) = *reinterpret_cast<const uint8_t*>(p);
    sum += last;
  }

  sum = fold32(sum);
  return static_cast<uint16_t>(~sum);
}

} // namespace rudp
