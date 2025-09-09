#pragma once
#include <string>
#include <cstdint>
#include <vector>
#include <chrono>

namespace rudp {
class UdpSocket {
 public:
  UdpSocket();
  ~UdpSocket();
  void bind(uint16_t port);
  void connect_to(const std::string& host, uint16_t port);
  void set_recv_timeout(std::chrono::milliseconds t);
  int  send(const uint8_t* data, std::size_t n);
  int  recv(uint8_t* buf, std::size_t cap);

 private:
  int fd_{-1};
  // sockaddr_storage peer_ …
};
}
