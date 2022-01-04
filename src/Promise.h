#pragma once
#include <QMutex>
#include <QSemaphore>

// Simple cancelable(!) promise because the machinery to do this
// using std::future is still experimental. Also, QFuture would be
// an option, except that it requires you to call finish() after
// cancelling (and explicitly forbids finishing multiple times)
// which makes using it in an not-necessarily-called callback
// hard.
template<typename T>
struct Promise {
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

    // Call wait() before accessing cancelled or value. If wait returns true,
    // the future was fulfilled. wait() blocks till the future was either
    // fulfilled or cancelled. (If fulfill() and cancel() were called at the
    // same time the output is undetermined.)
    bool wait() {
        semaphore_.acquire();
        return !cancelled;
    }
};