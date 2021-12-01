/** ==========================================================================
* 2010 by KjellKod.cc. This is PUBLIC DOMAIN to use at your own risk and comes
* with no warranties. This code is yours to share, use and modify with no
* strings attached and no restrictions or obligations.
 *
 * For more information see g3log/LICENSE or refer refer to http://unlicense.org
* ============================================================================
*
* Example of a normal std::queue protected by a mutex for operations,
* making it safe for thread communication, using std::mutex from C++0x with
* the help from the std::thread library from JustSoftwareSolutions
* ref: http://www.stdthread.co.uk/doc/headers/mutex.html
*
* This example  was totally inspired by Anthony Williams lock-based data structures in
* Ref: "C++ Concurrency In Action" http://www.manning.com/williams */

#pragma once

#include <queue>
#include <mutex>
#include <exception>
#include <condition_variable>

/** Multiple producer, multiple consumer thread safe queue
* Since 'return by reference' is used this queue won't throw */
template<typename T>
class shared_queue
{
   std::queue<T> queue_; // TODO, lock free queue
   mutable std::mutex m_;
   std::condition_variable data_cond_;

   shared_queue &operator=(const shared_queue &) = delete;
   shared_queue(const shared_queue &other) = delete;

public:
   shared_queue() {}

   void push(T item) {
      {
         std::lock_guard<std::mutex> lock(m_);
         queue_.push(std::move(item));
      }
      data_cond_.notify_one();
   }

   /// return immediately, with true if successful retrieval
   bool try_and_pop(T &popped_item) {
      std::lock_guard<std::mutex> lock(m_);
      if (queue_.empty()) {
         return false;
      }
      popped_item = std::move(queue_.front());
      queue_.pop();
      return true;
   }

   /// Try to retrieve, if no items, wait till an item is available and try again
   void wait_and_pop(T& popped_item) {
      // Unique lock
      // A unique lock is an object that manages a mutex object with unique
      // ownership in both states: locked and unlocked.
      // On construction (or by move-assigning to it), the object acquires a 
      // mutex object, for whose locking and unlocking operations becomes 
      // responsible.
      // This class guarantees an unlocked status on destruction (even if not
      // called explicitly).
      std::unique_lock<std::mutex> lock(m_);
      while (queue_.empty()) {
         // Wait until notified
         // The execution of the current thread (which have locked lock's mutex)
         // is blocked until notified.
         // At the moment of blocking the thread, the function automatically calls 
         // lock.unlock(), allowing other locked threads to continue.
         // To return from wait() two things must happen: the thread gets notified 
         // from the condition variable, and the thread relocks the mutex
         // successfully. wait() unlocks the mutex and waits on the condition 
         // variable atomically, and trys to relock the mutex when it is notified.
         data_cond_.wait(lock);
         // This 'while' loop is equal to
         // data_cond_.wait(lock, [](bool result){return !queue_.empty();});
      }
      popped_item = std::move(queue_.front());
      queue_.pop();
   }

   bool empty() const {
      std::lock_guard<std::mutex> lock(m_);
      return queue_.empty();
   }

   unsigned size() const {
      std::lock_guard<std::mutex> lock(m_);
      return queue_.size();
   }
};
