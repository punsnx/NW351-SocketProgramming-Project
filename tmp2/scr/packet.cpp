#include "packet.hpp"
#include "checksum.hpp"

#include <arpa/inet.h>
#include <cstring>

namespace rudp {

namespace {
constexpr std::size_t kHdrWireLen = 4 + 4 + 1; // seq(4) + ack(4) + flags(1)
constexpr std::size_t kCksumLen   = 2;         // uint16_t
}

std::vector<uint8_t> serialize(const Segment& s) {
  std::vector<uint8_t> buf;
  buf.reserve(kHdrWireLen + s.data_len + kCksumLen);

  // header (network byte order)
  uint32_t nseq = htonl(s.hdr.seq);
  uint32_t nack = htonl(s.hdr.ack);

  const uint8_t* pseq = reinterpret_cast<const uint8_t*>(&nseq);
  const uint8_t* pack = reinterpret_cast<const uint8_t*>(&nack);

  buf.insert(buf.end(), pseq, pseq + 4);
  buf.insert(buf.end(), pack, pack + 4);
  buf.push_back(s.hdr.flags);

  // payload
  buf.insert(buf.end(), s.data.begin(), s.data.begin() + s.data_len);

  // checksum over header + data
  uint16_t sum = internet_checksum(buf.data(), buf.size());
  uint16_t nsum = htons(sum);
  const uint8_t* psum = reinterpret_cast<const uint8_t*>(&nsum);
  buf.insert(buf.end(), psum, psum + 2);

  return buf;
}

bool deserialize(const uint8_t* buf, std::size_t n, Segment* out) {
  if (!out) return false;
  if (n < kHdrWireLen + kCksumLen) return false;

  // verify checksum
  const uint16_t* pck = reinterpret_cast<const uint16_t*>(buf + n - 2);
  uint16_t got_cksum = ntohs(*pck);
  uint16_t calc_cksum = internet_checksum(buf, n - 2);
  if (got_cksum != calc_cksum) return false;

  // parse header
  uint32_t nseq = 0, nack = 0;
  std::memcpy(&nseq, buf + 0, 4);
  std::memcpy(&nack, buf + 4, 4);
  out->hdr.seq   = ntohl(nseq);
  out->hdr.ack   = ntohl(nack);
  out->hdr.flags = *(buf + 8);

  // payload
  const std::size_t payload_len = n - kHdrWireLen - kCksumLen;
  if (payload_len > kMaxData) return false;

  std::memset(out->data.data(), 0, out->data.size());
  if (payload_len > 0) {
    std::memcpy(out->data.data(), buf + kHdrWireLen, payload_len);
  }
  out->data_len = payload_len;
  return true;
}

} // namespace rudp
