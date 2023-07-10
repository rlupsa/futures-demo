#pragma once

#include "Executor.h"

#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

/** @brief Simple thread pool, with a fixed number of threads.
 * */
class ThreadPool : public Executor
{
public:
    explicit ThreadPool(size_t nrThreads);
    ~ThreadPool() override;
    void enqueue(std::function<void()> func) override;

private:
    void workerFunction();

    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_closing = false;
    std::queue<std::function<void()> > m_workItems;
    std::vector<std::thread> m_workers;
};
