#pragma once

#include "Future.h"
#include <condition_variable>
#include <mutex>

/** @brief An object holding futures that correspond to "fire and forget" operations.
 * 
 * A FutureWaiter will keep track of the "fire and forget" futures. As they complete, they are discarded (so that any associated
 * resources get freed).
 * */
class FutureWaiter {
public:
    /** @brief Adds a future to the "fire and forget" list. It will be kept by this object until it completes.
     * */
    template<typename T>
    void addToWaitList(Future<T> f) {
        std::unique_lock<std::mutex> lck(m_mutex);
        size_t index;
        for(index=0 ; index < m_waitList.size() ; ++index) {
            if(!m_waitList[index].second) break;
        }
        if(index >= m_waitList.size()) {
            m_waitList.emplace_back(f.futureObject(), true);
        } else {
            m_waitList[index].first = f.futureObject();
            m_waitList[index].second = true;
        }
        ++m_nrActive;
        //std::cout << "Added client " << index << "\n";
        lck.unlock();
        m_waitList[index].first->addCommonCallback([this,index](FutureCompletionState state, std::exception_ptr){
            switch(state) {
            case FutureCompletionState::completedNormally:
                //std::cout << "Client " << index << " terminated normally\n";
                break;
            case FutureCompletionState::exception:
                //std::cout << "Client " << index << " terminated with exception\n";
                break;
            default:
                //std::cout << "Client " << index << ": we should not get here\n";
                break;
            }
            callback(index);
        });
    }

    /** @brief Waits until all added futures complete. addToWaitList() should not be called after this function is called.
     * */
    void waitForAll();

private:
    void callback(int index);

    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::vector<std::pair<std::shared_ptr<PromiseFuturePairBase> ,bool> > m_waitList;
    size_t m_nrActive = 0;
};
