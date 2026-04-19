#pragma once
#ifndef ESVO2_CORE_SYSTEM_STATUS_H
#define ESVO2_CORE_SYSTEM_STATUS_H

#include <atomic>
#include <memory>
#include <string>

namespace esvo2_core
{

/// @brief Atomic int mapped to enum that keeps track of the system status.
enum class SystemStatus : int
{
    INITIALIZATION = 0,
    WORKING = 1,
    RESET = 2,
    TERMINATE = 3
};

/// @brief meyers singleton -- one global instance of system status atomic int
inline std::atomic<int> &getStatusInstance()
{
    static std::atomic<int> instance{static_cast<int>(SystemStatus::INITIALIZATION)};
    return instance;
}

inline SystemStatus getSystemStatus()
{
    return static_cast<SystemStatus>(getStatusInstance().load());
}

inline void setSystemStatus(SystemStatus status)
{
    getStatusInstance().store(static_cast<int>(status));
}

inline std::string systemStatusToString()
{
    switch (getSystemStatus())
    {
    case SystemStatus::INITIALIZATION:
        return "INITIALIZATION";
    case SystemStatus::WORKING:
        return "WORKING";
    case SystemStatus::RESET:
        return "RESET";
    case SystemStatus::TERMINATE:
        return "TERMINATE";
    default:
        return "INVALID STATUS";
    }
}

} // namespace esvo2_core

#endif // ESVO2_CORE_SYSTEM_STATUS_H