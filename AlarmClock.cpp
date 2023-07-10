#include "AlarmClock.h"

AlarmClock::AlarmClock()
    :m_thread(&AlarmClock::threadFunc, this)
{
}

AlarmClock::~AlarmClock() {
    m_mutex.lock();
    m_closing = true;
    m_cv.notify_all();
    m_mutex.unlock();
    m_thread.join();
}

void AlarmClock::setTimer(std::chrono::system_clock::time_point when, std::function<void()> func) {
    std::unique_lock<std::mutex> lck(m_mutex);
    auto it = m_timers.emplace(when, func).first;
    if(it == m_timers.begin()) {
        m_cv.notify_one();
    }
}

Future<void> AlarmClock::setTimer(std::chrono::system_clock::time_point when) {
    std::shared_ptr<PromiseFuturePair<void> > ret = std::make_shared<PromiseFuturePair<void> >();
    this->setTimer(when, [ret](){ret->set();});
    return Future<void>(ret);
}

void AlarmClock::threadFunc() {
    std::unique_lock<std::mutex> lck(m_mutex);
    while(true) {
        if(m_timers.empty()) {
            if(m_closing) return;
            m_cv.wait(lck);
        } else {
            auto ret = m_cv.wait_until(lck, m_timers.begin()->first);
            if(ret == std::cv_status::timeout) {
                std::function<void()> action = std::move(m_timers.begin()->second);
                m_timers.erase(m_timers.begin());
                lck.unlock();
                action();
                lck.lock();
            }
        }
    }
}
