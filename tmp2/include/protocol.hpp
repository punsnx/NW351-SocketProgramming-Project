#pragma once
#include <string>
#include <cstddef>

namespace rudp {
struct ServerOptions { uint16_t port; std::size_t adv_window; };
struct ClientOptions { std::string host; uint16_t port; std::string file; std::size_t adv_window; };

int run_server(const ServerOptions& opt); // รับ request ชื่อไฟล์ -> ส่งไฟล์ด้วย RUDP
int run_client(const ClientOptions& opt); // ขอไฟล์ -> รับ/ประกอบ/ตรวจครบถ้วน
}
