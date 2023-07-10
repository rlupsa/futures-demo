#include "ThreadPool.h"

ThreadPool::ThreadPool(size_t nrThreads)
{
    m_workers.reserve(nrThreads);
    for (size_t i = 0; i < nrThreads; ++i) {
        m_workers.emplace_back(&ThreadPool::workerFunction, this);
    }
}

ThreadPool::~ThreadPool() {
    std::unique_lock<std::mutex> lck(m_mutex);
    m_closing = true;
    m_cv.notify_all();
    lck.unlock();
    for (auto& worker : m_workers) {
        worker.join();
    }
}

void ThreadPool::enqueue(std::function<void()> func) {
    std::unique_lock<std::mutex> lck(m_mutex);
    m_workItems.push(std::move(func));
    m_cv.notify_one();
}

void ThreadPool::workerFunction() {
    std::unique_lock<std::mutex> lck(m_mutex);
    while (true) {
        if (!m_workItems.empty()) {
            std::function<void()> func = std::move(m_workItems.front());
            m_workItems.pop();
            lck.unlock();
            func();
            lck.lock();
        } else if (m_closing) {
            return;
        } else {
            m_cv.wait(lck);
        }
    }
}
