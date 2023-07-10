#pragma once

#include "Future.h"

#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <thread>

/** @brief Simple class for scheduling timers. Mostly for demo purposes.
 * */
class AlarmClock {
public:
    AlarmClock();
    ~AlarmClock();
    AlarmClock(AlarmClock const&) = delete;
    AlarmClock(AlarmClock &&) = delete;
    AlarmClock& operator=(AlarmClock const&) = delete;
    AlarmClock& operator=(AlarmClock &&) = delete;
    
    /** @brief Sets a timer to be executed at a specified point in time. The timer cannot be cancelled
     * */
    void setTimer(std::chrono::system_clock::time_point when, std::function<void()> func);

    /** @brief Creates a future that will complete at a specified point in time. It cannot be cancelled.
     * */
    Future<void> setTimer(std::chrono::system_clock::time_point when);

private:
    void threadFunc();
    
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::map<std::chrono::system_clock::time_point, std::function<void()> > m_timers;
    bool m_closing = false;
    std::thread m_thread;
};
