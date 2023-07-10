#pragma once

#include "AlarmClock.h"
#include "Executor.h"

template<typename R, typename Func>
Future<R> launchAsync(Executor& executor, Func func)
{
    std::shared_ptr<PromiseFuturePair<R> > ret = std::make_shared<PromiseFuturePair<R> >();
    executor.enqueue([ret,tmpFunc=std::move(func)]() -> void {
        ret->set(tmpFunc());
    });
    return Future<R>(ret);
}

/**
 * @brief Adds a simple (synchronous) function as a continuation to a future
 * @param executor An executor that will execute the continuation
 * @param func The function to be executed. It is assumed to execute synchronously and return a simple value (not a future)
 * @param fArg The future whose completion should trigger the continuation. It will be given as an argument to the function func
 * @return A future that will be completed when the continuation (func) completes
 */
template<typename R, typename Func, typename Arg>
Future<R> addContinuation(Executor& executor, Func func, Future<Arg> fArg)
{
    std::shared_ptr<PromiseFuturePair<R> > ret = std::make_shared<PromiseFuturePair<R> >();
    auto continuation = [ret,tmpFunc=std::move(func), fArg]() -> void {
        typename PromiseFuturePair<Arg>::FutureValueType const& val(fArg.futureObject()->get());
        if(std::holds_alternative<Arg>(val)) {
            try {
                ret->set(tmpFunc(std::get<Arg>(val)));
            } catch(...) {
                ret->setException(std::current_exception());
            }
        } else {
            ret->setException(std::get<std::exception_ptr>(val));
        }
    };
    fArg.addCallback([&executor, tmpContinuation = std::move(continuation)](typename Future<Arg>::FutureValueType const& ) -> void {
        executor.enqueue(std::move(tmpContinuation));
    });
    return Future<R>(ret);
}

/**
 * @brief Adds an asynchronous function as a continuation to a future
 * @param executor An executor that will execute the continuation
 * @param func The function to be executed. It is assumed to start an asynchronoys operation and immediately return a future for that operation
 * @param fArg The future whose completion should trigger the continuation. It will be given as an argument to the function func
 * @return A future that will be completed when the continuation (func) completes
 */
template<typename R, typename Func, typename Arg>
Future<R> addAsyncContinuation(Executor& executor, Func func, Future<Arg> fArg)
{
    std::shared_ptr<PromiseFuturePair<R> > ret = std::make_shared<PromiseFuturePair<R> >();
    auto continuation = [ret, tmpFunc = std::move(func), fArg]() -> void {
        typename PromiseFuturePair<Arg>::FutureValueType const& val(fArg.futureObject()->get());
        if(std::holds_alternative<Arg>(val)) {
            try {
                auto future = tmpFunc(std::get<Arg>(val));
                future.addCallback([ret](typename Future<R>::FutureValueType const& v) {ret->setResult(v); });
            } catch(...) {
                ret->setException(std::current_exception());
            }
        } else {
            ret->setException(std::get<std::exception_ptr>(val));
        }
    };
    fArg.addCallback([&executor, tmpContinuation = std::move(continuation)](typename Future<Arg>::FutureValueType const&) -> void {
        executor.enqueue(std::move(tmpContinuation));
    });
    return Future<R>(ret);
}

/**
 * @brief Adds an asynchronous function as a continuation to a future in case the future ends in an exception
 * @param executor An executor that will execute the continuation
 * @param func The function to be executed. It shall take a std::exception_ptr as an argument. It is assumed to start an asynchronoys operation and immediately return a future for that operation
 * @param fArg The future whose completion with exception should trigger the continuation. It will be given as an argument to the function func
 * @return A future that will be completed when the continuation (func) completes
 */
template<typename R, typename Func, typename Arg>
Future<R> catchAsync(Executor& executor, Func func, Future<Arg> fArg)
{
    std::shared_ptr<PromiseFuturePair<R> > ret = std::make_shared<PromiseFuturePair<R> >();
    auto continuation = [ret, tmpFunc = std::move(func), fArg]() -> void {
        typename PromiseFuturePair<Arg>::FutureValueType const& val(fArg.futureObject()->get());
        if(std::holds_alternative<std::exception_ptr>(val)) {
            try {
                auto future = tmpFunc(std::get<std::exception_ptr>(val));
                future.addCallback([ret](typename Future<R>::FutureValueType const& v) {ret->setResult(v); });
            } catch(...) {
                ret->setException(std::current_exception());
            }
        } else {
            ret->setResult(val);
        }
    };
    fArg.addCallback([&executor, tmpContinuation = std::move(continuation)](typename Future<Arg>::FutureValueType const&) -> void {
        executor.enqueue(std::move(tmpContinuation));
    });
    return Future<R>(ret);
}

namespace continuations_private {
    /**
     * @brief Given a completed future start, it executes loopingPredicate on it and, as long as it returns true, it enqueues 
     * @param executor
     * @param loopingPredicate
     * @param loopFunc
     * @param start
     * @param ret
     */
    template<typename R, typename LoopFunc, typename PredicateFunc>
    void auxLoop(Executor& executor, PredicateFunc loopingPredicate, LoopFunc loopFunc, R const& start, std::shared_ptr<PromiseFuturePair<R> > ret)
    {
        if(!loopingPredicate(start)) {
            ret->set(start);
            return;
        }

        try {
            Future<R> tmpResFuture = loopFunc(start);
            tmpResFuture.addCallback([&executor,loopingPredicate,loopFunc,tmpResFuture,ret](typename Future<R>::FutureValueType const&){
                executor.enqueue([&executor,loopingPredicate,loopFunc,tmpResFuture,ret](){
                    typename PromiseFuturePair<R>::FutureValueType const& val(tmpResFuture.futureObject()->get());
                    if(std::holds_alternative<R>(val)) {
                        auxLoop(executor, loopingPredicate, loopFunc, std::get<R>(val), ret);
                    } else {
                        ret->setException(std::get<std::exception_ptr>(val));
                    }
                });
            });
        } catch(...) {
            ret->setException(std::current_exception());
        }
    }

}

/**
 * @brief Adds a loop of an asynchronous function as a continuation to a future
 * @param executor An executor that will execute the continuations
 * @param loopingPredicate A function taking a value of type R and returns true if the loop should execute (again) or false if the value is to be returned
 * @param loopFunc A function taking an R and returning a Future<R> that is to be used as the loop body
 * @param start 
 * @return 
 * 
 * When start completes, the loopingPredicate(start) is invoked. If this returns false, the value in start is set in
 * the retuned future. Otherwise, loopFunc(start) is invoked and its result is treated as if it were start
 */
template<typename R, typename LoopFunc, typename PredicateFunc>
Future<R> executeAsyncLoop(Executor& executor, PredicateFunc loopingPredicate, LoopFunc loopFunc, R const& start)
{
    std::shared_ptr<PromiseFuturePair<R> > ret = std::make_shared<PromiseFuturePair<R> >();
    continuations_private::auxLoop(executor, loopingPredicate, loopFunc, start, ret);
    return Future<R>(ret);
}

// addRepeatedAsyncContinuation
