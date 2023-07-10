#pragma once

#include <functional>

/** @brief Simple executor interface.
 * */
class Executor {
public:
    virtual ~Executor() {}

    /** @brief adds an action to be executed at a later time.
     * */
    virtual void enqueue(std::function<void()> func) = 0;
};
