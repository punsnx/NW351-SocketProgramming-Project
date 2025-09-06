//timeout.hpp
#pragma once
#include <sys/time.h>
#include <sys/socket.h>
#include <algorithm>

static inline void set_socket_timeout_ms(int sock, int recv_ms, int send_ms) {
    timeval tv{};
    tv.tv_sec  = recv_ms / 1000;
    tv.tv_usec = (recv_ms % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    tv.tv_sec  = send_ms / 1000;
    tv.tv_usec = (send_ms % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}

static inline int clamp_timeout_ms(long long ms, int def=3000, int lo=1, int hi=600000) {
    if (ms <= 0) return def;
    if (ms > hi) return hi;
    if (ms < lo) return lo;
    return (int)ms;
}

