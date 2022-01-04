#pragma once
#include <QMutex>
#include <QSemaphore>

// Simple cancelable(!) promise because the machinery to do this
// using std::future is still experimental.
template<typename T>
struct Promise {
    QMutex lock_;
    QSemaphore semaphore_;

    T value;
    bool cancelled{false};

    void fulfill(T &&val) {
        value = std::move(val);
        semaphore_.release();
    }

    void cancel() {
        cancelled = true;
        semaphore_.release();
    }

    bool wait() {
        semaphore_.acquire();
        return !cancelled;
    }
};