#pragma once

#include <condition_variable>
#include <exception>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <variant>

enum class FutureCompletionState {
    nonCompleted = 0,
    completedNormally = 1,
    exception = 2
};

class FutureNotCompletedTag {};
class VoidFutureCompletedTag {};

/**
 * @brief Base class for PromiseFuturePair, offering only information about the termination
 */
class PromiseFuturePairBase {
public:
    using CommonCallbackType = std::function<void(FutureCompletionState, std::exception_ptr)>;

    virtual ~PromiseFuturePairBase(){}
    virtual bool isReady() const = 0;
    virtual void wait() const = 0;
    virtual void addCommonCallback(CommonCallbackType callback) = 0;
};

template<typename T>
class PromiseFuturePair : public PromiseFuturePairBase {
public:
    using FutureValueType = std::variant<FutureNotCompletedTag,T,std::exception_ptr>;
    using CallbackType = std::function<void(FutureValueType const&)>;
    void set(T value) {
        setResult(FutureValueType(std::move(value)));
    }
    void setException(std::exception_ptr pEx) {
        setResult(FutureValueType(std::move(pEx)));
    }
    void setResult(FutureValueType v) {
        std::unique_lock<std::mutex> lck(m_mutex);
        m_val = std::move(v);
        m_cv.notify_all();
        if(!m_callbacks.empty()) {
            lck.unlock();
            for(auto& callback : m_callbacks) {
                callback(m_val);
            }
            m_callbacks.clear();
        }
    }

    FutureValueType const& get() const {
        std::unique_lock<std::mutex> lck(m_mutex);
        m_cv.wait(lck, [this](){return !std::holds_alternative<FutureNotCompletedTag>(m_val);});
        return m_val;
    }
    FutureValueType getMove() {
        std::unique_lock<std::mutex> lck(m_mutex);
        m_cv.wait(lck, [this](){return !std::holds_alternative<FutureNotCompletedTag>(m_val);});
        return std::move(m_val);
    }
    void addCallback(CallbackType callback) {
        std::unique_lock<std::mutex> lck(m_mutex);
        if(!std::holds_alternative<FutureNotCompletedTag>(m_val)) {
            lck.unlock();
            callback(m_val);
        } else {
            m_callbacks.push_back(std::move(callback));
        }
    }
    bool isReady() const override {
        std::unique_lock<std::mutex> lck(m_mutex);
        return !std::holds_alternative<FutureNotCompletedTag>(m_val);
    }
    void wait() const override {
        std::unique_lock<std::mutex> lck(m_mutex);
        m_cv.wait(lck, [this](){return !std::holds_alternative<FutureNotCompletedTag>(m_val);});
    }
    void addCommonCallback(CommonCallbackType callback) override {
        this->addCallback([commonCallback=std::move(callback)](FutureValueType const& val){
            if(std::holds_alternative<T>(val)) {
                commonCallback(FutureCompletionState::completedNormally, nullptr);
            } else {
                commonCallback(FutureCompletionState::exception, std::get<std::exception_ptr>(val));
            }
        });
    }
private:
    mutable std::mutex m_mutex;
    mutable std::condition_variable m_cv;
    FutureValueType m_val;
    std::list<CallbackType> m_callbacks;
};

template<>
class PromiseFuturePair<void> : public PromiseFuturePairBase {
public:
    using FutureValueType = std::variant<FutureNotCompletedTag,VoidFutureCompletedTag,std::exception_ptr>;
    using CallbackType = std::function<void(FutureValueType const&)>;
    void set() {
        internalSet(FutureValueType(VoidFutureCompletedTag()));
    }
    void setException(std::exception_ptr pEx) {
        internalSet(FutureValueType(std::move(pEx)));
    }
    void addCallback(CallbackType callback) {
        std::unique_lock<std::mutex> lck(m_mutex);
        if(!std::holds_alternative<FutureNotCompletedTag>(m_val)) {
            lck.unlock();
            callback(m_val);
        } else {
            m_callbacks.push_back(std::move(callback));
        }
    }
    bool isReady() const override {
        std::unique_lock<std::mutex> lck(m_mutex);
        return !std::holds_alternative<FutureNotCompletedTag>(m_val);
    }
    void wait() const override {
        std::unique_lock<std::mutex> lck(m_mutex);
        m_cv.wait(lck, [this](){return !std::holds_alternative<FutureNotCompletedTag>(m_val);});
    }
    void addCommonCallback(CommonCallbackType callback) override {
        this->addCallback([commonCallback=std::move(callback)](FutureValueType const& val){
            if(std::holds_alternative<VoidFutureCompletedTag>(val)) {
                commonCallback(FutureCompletionState::completedNormally, nullptr);
            } else {
                commonCallback(FutureCompletionState::exception, std::get<std::exception_ptr>(val));
            }
        });
    }
private:
    void internalSet(FutureValueType v) {
        std::unique_lock<std::mutex> lck(m_mutex);
        m_val = std::move(v);
        m_cv.notify_all();
        if(!m_callbacks.empty()) {
            lck.unlock();
            for(auto& callback : m_callbacks) {
                callback(m_val);
            }
            m_callbacks.clear();
        }
    }

    mutable std::mutex m_mutex;
    mutable std::condition_variable m_cv;
    FutureValueType m_val;
    std::list<CallbackType> m_callbacks;
};

/** Future with base type T.
 * 
 * Essentially, it is a wrapper over a shared pointer to a PromiseFuturePair<T>
 * */
template<typename T>
class Future {
public:
    using FutureValueType = typename PromiseFuturePair<T>::FutureValueType;
    using CallbackType = typename PromiseFuturePair<T>::CallbackType;
    using CommonCallbackType = typename PromiseFuturePairBase::CommonCallbackType;
    
    explicit Future(std::shared_ptr<PromiseFuturePair<T> > pFuture)
        :m_pFuture(pFuture)
        {}

    /** @brief Waits until the future completes, then returns the value, or throws the exception if the future completes with an exception.
     * */
    T const& get() const {
        FutureValueType const& val(m_pFuture->get());
        if(std::holds_alternative<T>(val)) {
            return std::get<T>(val);
        }
        std::rethrow_exception(std::get<std::exception_ptr>(val));
    }
    T getMove() const {
        FutureValueType val = m_pFuture->getMove();
        if(std::holds_alternative<T>(val)) {
            return std::move(std::get<T>(val));
        }
        std::rethrow_exception(std::get<std::exception_ptr>(val));
    }
    
    /** @brief Adds a callback that will execute when the future completes. If the future is already completed, the callback
     * executes on the current thread; otherwise, the callback will execute on the thread that completes the future.
     * */
    void addCallback(CallbackType callback) const {
        m_pFuture->addCallback(callback);
    }
    /** @brief Adds a callback that will execute when the future completes. If the future is already completed, the callback
     * executes on the current thread; otherwise, the callback will execute on the thread that completes the future.
     * */
    void addCommonCallback(CommonCallbackType callback) const {
        m_pFuture->addCommonCallback(callback);
    }
    /** @brief Waits until the future completes.
     * */
    void wait() const {
        m_pFuture->wait();
    }
    std::shared_ptr<PromiseFuturePair<T> > futureObject() const {
        return m_pFuture;
    }
private:
    std::shared_ptr<PromiseFuturePair<T> > m_pFuture;
};

template<>
class Future<void> {
public:
    using CommonCallbackType = typename PromiseFuturePairBase::CommonCallbackType;
    explicit Future(std::shared_ptr<PromiseFuturePairBase> pFuture)
        :m_pFuture(pFuture)
        {}

    template<typename T>
    Future(Future<T> const& f)
        :m_pFuture(f.futureObject())
        {}

    void addCommonCallback(CommonCallbackType callback) {
        m_pFuture->addCommonCallback(callback);
    }
    void wait() {
        m_pFuture->wait();
    }
    std::shared_ptr<PromiseFuturePairBase> futureObject() {
        return m_pFuture;
    }
private:
    std::shared_ptr<PromiseFuturePairBase> m_pFuture;
};

template<typename T>
Future<T> completedFuture(T val) {
    std::shared_ptr<PromiseFuturePair<T> > ret = std::make_shared<PromiseFuturePair<T> >();
    ret->set(std::move(val));
    return Future<T>(ret);
}

inline
Future<void> completedFuture() {
    std::shared_ptr<PromiseFuturePair<void> > ret = std::make_shared<PromiseFuturePair<void> >();
    ret->set();
    return Future<void>(ret);
}
