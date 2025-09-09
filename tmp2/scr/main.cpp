#include <iostream>
#include "protocol.hpp"

int main(int argc, char** argv) {
  using namespace rudp;
  if (argc >= 2 && std::string(argv[1]) == "server") {
    if (argc != 4) {
      std::cerr << "usage: rudp server <port> <adv_window_bytes>\n";
      return 2;
    }
    ServerOptions opt{static_cast<uint16_t>(std::stoi(argv[2])),
                      static_cast<std::size_t>(std::stoul(argv[3]))};
    return run_server(opt);
  } else if (argc >= 2 && std::string(argv[1]) == "client") {
    if (argc != 6) {
      std::cerr << "usage: rudp client <host> <port> <file> <adv_window_bytes>\n";
      return 2;
    }
    ClientOptions opt{argv[2],
                      static_cast<uint16_t>(std::stoi(argv[3])),
                      argv[4],
                      static_cast<std::size_t>(std::stoul(argv[5]))};
    return run_client(opt);
  }
  std::cerr << "usage: rudp (server|client) ...\n";
  return 2;
}
