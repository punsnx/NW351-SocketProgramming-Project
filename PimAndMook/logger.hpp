#pragma once
#include <chrono>
#include <iostream>
#include <string>
using namespace chrono;
using namespace std;

class Logger {
public:
    Logger() {
        start = steady_clock::now();
    }

    void log(const string& msg) const {
        auto now = steady_clock::now();
        auto passed = duration_cast<milliseconds>(now - start).count();
        cout << "[" << passed << " ms] " << msg << endl;
    }

    void logError(const string& msg) const {
        auto now = steady_clock::now();
        auto passed = duration_cast<milliseconds>(now - start).count();
        cerr << "[" << passed << " ms] ERROR: " << msg << endl;
    }

private:
    steady_clock::time_point start;
};
