#pragma once
#include <chrono>
#include <iostream>
#include <string>

class Logger {
public:
    Logger() {
        start_ = std::chrono::steady_clock::now();
    }

    void log(const std::string& msg) const {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_).count();
        std::cout << "[" << elapsed << " ms] " << msg << std::endl;
    }

    void logError(const std::string& msg) const {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_).count();
        std::cerr << "[" << elapsed << " ms] ERROR: " << msg << std::endl;
    }

private:
    std::chrono::steady_clock::time_point start_;
};
