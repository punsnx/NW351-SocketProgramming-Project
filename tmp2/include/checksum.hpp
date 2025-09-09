#pragma once
#include <cstddef>
#include <cstdint>

namespace rudp {
uint16_t internet_checksum(const uint8_t* data, std::size_t len);
}
