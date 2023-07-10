#include "FutureWaiter.h"

void FutureWaiter::waitForAll() {
    std::unique_lock<std::mutex> lck(m_mutex);
    m_cv.wait(lck, [this](){return m_nrActive == 0;});
}

void FutureWaiter::callback(int index) {
    std::unique_lock<std::mutex> lck(m_mutex);
    if(m_waitList[index].second) {
        m_waitList[index].second = false;
        m_waitList[index].first = nullptr;
        --m_nrActive;
        if(0 == m_nrActive) {
            m_cv.notify_all();
        }
    }
}
