#pragma once
#include <sys/time.h>
#include <sys/socket.h>

/* prototype
struct my_timeval {
    long tv_sec;   // วินาที
    long tv_usec;  // ไมโครวินาที
}


/* prototype
int setsockopt(
    int sockfd,    // file descriptor ของ socket ที่จะตั้งค่า(ได้จาก socket())
    socket() return ค่าเป็น int ที่เรียกว่า socket file descriptor (fd)
    int level,     // ระดับของ option เช่น SOL_SOCKET(ตัวเลือกของ socket เอง), IPPROTO_TCP (ตัวเลือก TCP),..
    int optname,   // ตัวเลือก เช่น SO_RCVTIMEO
    const void *optval, // pointer ไปยังค่าที่จะตั้งค่า
    socklen_t optlen    // ขนาดของข้อมูลที่ส่งไป
);
*/

static inline void set_socket_timeout_ms(int sock, int recv_ms, int send_ms) {
    timeval tv{};
    tv.tv_sec  = recv_ms / 1000;
    tv.tv_usec = (recv_ms % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)); 

    tv.tv_sec  = send_ms / 1000;
    tv.tv_usec = (send_ms % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    //return 0 = success, -1 = error
}

static inline int clamp_timeout_ms(long long ms, int def=3000, int minTimeout=1, int maxTimeout=600000) {
    if (ms <= 0) return def;
    if (ms > maxTimeout) return maxTimeout;
    if (ms < minTimeout) return minTimeout;
    return (int)ms;
}

