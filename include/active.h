/** ==========================================================================
 * 2010 by KjellKod.cc. This is PUBLIC DOMAIN to use at your own risk and comes
 * with no warranties. This code is yours to share, use and modify with no
 * strings attached and no restrictions or obligations.
 *
 * For more information see g3log/LICENSE or refer refer to http://unlicense.org
 * ============================================================================
 *
 * Example of a Active Object, using C++11 std::thread mechanisms to make it
 * safe for thread communication.
 *
 * This was originally published at http://sites.google.com/site/kjellhedstrom2/active-object-with-cpp0x
 * and inspired from Herb Sutter's C++11 Active Object
 * http://herbsutter.com/2010/07/12/effective-concurrency-prefer-using-active-objects-instead-of-naked-threads
 *
 * Last update 2013-12-19 by Kjell Hedstrom,
 * e-mail: hedstrom at kjellkod dot cc
 * linkedin: http://linkedin.com/se/kjellkod */

#pragma once

#include <thread>
#include <functional>
#include <memory>
#include "shared_queue.h"

typedef std::function< void() > Callback;

class Active {
private:
    Active() : done_(false) {} // Construction ONLY through factory createActive();
    Active(const Active&) = delete;
    Active& operator=(const Active&) = delete;

    void run() {
        while (!done_) {
            Callback func;
            mq_.wait_and_pop(func);
            func();
        }
    }

    shared_queue<Callback> mq_;
    std::thread thd_;
    bool done_;

public:
    virtual ~Active() {
        // this: simple by-reference capture of the current object 
        // we can directly use its data member within the lambda body
        send([this] { done_ = true; });
        // std::thread join()
        // Blocks the current thread until the thread identified by *this 
        // finishes its execution. The completion of the thread identified 
        // by *this synchronizes with the corresponding successful return 
        // from join().
        // No synchronization is performed on *this itself. Concurrently 
        // calling join() on the same thread object from multiple threads 
        // constitutes a data race that results in undefined behavior.
        thd_.join();
    }

    void send(Callback msg_) {
        mq_.push(msg_);
    }

    // tzl added improvement
    bool isActive() {
        return !mq_.empty();
    }

    /// Factory: safe construction of object before thread start
    static std::unique_ptr<Active> createActive() {
        std::unique_ptr<Active> aPtr(new Active());
        // template <class Fn, class... Args>
        //    explicit thread(Fn&& fn, Args&&... args);
        aPtr->thd_ = std::thread(&Active::run, aPtr.get());
        return aPtr;
        // unique_ptr<T> does not allow copy construction, instead it supports
        // move semantics.
        // A value that is returned from a function is treated as an rvalue,
        // so the move constructor is called automatically.
    }
};