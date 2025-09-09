#include "socket.hpp"

#include <stdexcept>
#include <cstring>
#include <cerrno>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>

namespace rudp {

UdpSocket::UdpSocket() {
  fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
  if (fd_ < 0) throw std::runtime_error("socket() failed");
  // peer_ unset
}

UdpSocket::~UdpSocket() {
  if (fd_ >= 0) ::close(fd_);
}

void UdpSocket::bind(uint16_t port) {
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);
  if (::bind(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    throw std::runtime_error("bind() failed");
  }
}

void UdpSocket::connect_to(const std::string& host, uint16_t port) {
  struct addrinfo hints{};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;

  struct addrinfo* res = nullptr;
  int rc = ::getaddrinfo(host.c_str(), nullptr, &hints, &res);
  if (rc != 0 || !res) throw std::runtime_error("getaddrinfo failed");

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr = reinterpret_cast<sockaddr_in*>(res->ai_addr)->sin_addr;

  std::memcpy(&peer_, &addr, sizeof(addr));
  // store length
  // (use sizeof(sockaddr_in), not sockaddr_storage)
  // no hard connect() needed, but fine to do:
  ::connect(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
  freeaddrinfo(res);
}

void UdpSocket::set_recv_timeout(std::chrono::milliseconds t) {
  timeval tv{};
  tv.tv_sec  = static_cast<int>(t.count() / 1000);
  tv.tv_usec = static_cast<int>((t.count() % 1000) * 1000);
  ::setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
}

int UdpSocket::send(const uint8_t* data, std::size_t n) {
  if (fd_ < 0) return -1;

  // if peer_ set (via connect_to() or first recvfrom), use sendto with it
  if (reinterpret_cast<const sockaddr_in*>(&peer_)->sin_family == AF_INET) {
    return ::sendto(fd_, data, n, 0,
                    reinterpret_cast<const sockaddr*>(&peer_),
                    sizeof(sockaddr_in));
  }
  // else: best effort (assumes ::connect used)
  return ::send(fd_, data, n, 0);
}

int UdpSocket::recv(uint8_t* buf, std::size_t cap) {
  if (fd_ < 0) return -1;

  sockaddr_in from{};
  socklen_t flen = sizeof(from);
  int r = ::recvfrom(fd_, buf, cap, 0, reinterpret_cast<sockaddr*>(&from), &flen);
  if (r >= 0) {
    // latch peer if not set
    if (reinterpret_cast<const sockaddr_in*>(&peer_)->sin_family != AF_INET) {
      std::memcpy(&peer_, &from, sizeof(from));
    }
  }
  return r;
}

} // namespace rudp
