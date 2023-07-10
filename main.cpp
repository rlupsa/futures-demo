#include "AlarmClock.h"
#include "Continuations.h"
#include "ThreadPool.h"

#include <iostream>

namespace {
    /**
     * @brief Starts an asynchronous operation that will return a specified result after a given amount of time
     * @param alarmClock An alarm clock object used for creating the delayed response
     * @param durationMilliseconds The delay in milliseconds
     * @param retVal The value to return
     * @return A future that will get completed after the specified delay
     */
    template<typename T>
    Future<T> delayedResult(AlarmClock& alarmClock, int durationMilliseconds, T retVal)
    {
        std::shared_ptr<PromiseFuturePair<T> > ret = std::make_shared<PromiseFuturePair<T> >();
        std::cout << "Setting alarm for delayed result " << retVal << " in " << durationMilliseconds << "ms.\n";
        alarmClock.setTimer(std::chrono::system_clock::now() + std::chrono::milliseconds(durationMilliseconds), [ret, tmpRetVal=std::move(retVal)]() {
            std::cout << "Returning delayed result " << tmpRetVal << "\n";
            ret->set(tmpRetVal);
        });
        return Future<T>(ret);
    }

    void testDirect()
    {
        AlarmClock alarmClock;
        ThreadPool threadPool(32);
        auto f1 = delayedResult(alarmClock, 2000, 40);
        auto f2 = addContinuation<int>(threadPool, [](int a)->int {return a + 2; }, f1);
        auto ret = f2.get();
        std::cout << "The answer = " << ret << "\n";
    }

    void testUnpack()
    {
        AlarmClock alarmClock;
        ThreadPool threadPool(32);
        auto f1 = delayedResult(alarmClock, 2000, 40);
        auto f2 = addAsyncContinuation<int>(threadPool, [&alarmClock](int a)->Future<int> {return delayedResult(alarmClock, 2000, a + 2); }, f1);
        auto ret = f2.get();
        std::cout << "The answer = " << ret << "\n";
    }

    void testAsyncLoop()
    {
        AlarmClock alarmClock;
        ThreadPool threadPool(32);
        auto f = executeAsyncLoop<int>(threadPool,
            [](int v)->bool {return v < 42;},
            [&alarmClock](int const& v)->Future<int> {return delayedResult(alarmClock, 1000, v + 7); },
            0);
        auto ret = f.get();
        std::cout << "The answer = " << ret << "\n";
    }
}

void demo_server();

int main()
{
    std::cout << "Hello World!\n";
    demo_server();
}
