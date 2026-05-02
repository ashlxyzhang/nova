#pragma once
#include <fstream>
#include <mutex>
#include <string>
#include <chrono>

// Simple file logger. Include this header and call DLOG("msg") anywhere.
// Output goes to src/SLAM/refactor_files/unused/slam_debug.log
// Thread-safe.
const std::string logging_file = "/Users/ashley/Documents/repos/nova/src/SLAM/refactor_files/unused/slam_debug.log";

inline std::ofstream& _dlog_stream()
{
    static std::ofstream f(logging_file, std::ios::out | std::ios::trunc);
    return f;
}

inline std::mutex& _dlog_mutex()
{
    static std::mutex m;
    return m;
}

inline double _dlog_now_sec()
{
    return std::chrono::duration<double>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

#ifndef DLOG
#define DLOG(msg)                                                        \
    do {                                                                 \
        std::lock_guard<std::mutex> _lg(_dlog_mutex());                 \
        _dlog_stream() << std::fixed << "[" << _dlog_now_sec() << "] " \
                       << __FUNCTION__ << ":" << __LINE__ << "  "      \
                       << msg << "\n";                                   \
        _dlog_stream().flush();                                          \
    } while (0)
#endif
