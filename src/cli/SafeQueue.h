#include <mutex>
#include <condition_variable>

#include <queue>

#pragma once
// Thread safe queue. We can go lock free with fancier implementations like a circular buffer queue
// which only relies on atomic variables. Neat. https://github.com/bittnkr/uniq
// This implementation is taken from https://stackoverflow.com/questions/15278343/c11-thread-safe-queue/16075550#16075550
template <class T>
class SafeQueue
{
public:
  SafeQueue(void)
    : q()
    , m()
    , c()
  {}

  ~SafeQueue(void)
  {}

  // Add an element to the queue.
  void enqueue(T t)
  {
    std::lock_guard<std::mutex> lock(m);
    q.push(t);
    c.notify_one();
  }

  // Get the "front"-element.
  // If the queue is empty, wait till a element is avaiable.
  T dequeue(void)
  {
    std::unique_lock<std::mutex> lock(m);

    c.wait( lock, [&]{ return !q.empty(); } ); // release lock as long as the wait and reaquire it afterwards.

    T val = q.front();
    q.pop();
    return val;
  }

private:
  std::queue<T> q;
  mutable std::mutex m;
  std::condition_variable c;
};
