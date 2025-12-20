#pragma once

#include <pthread.h>

class ThreadMutex {
  public:
    ThreadMutex() = default;
    ThreadMutex(ThreadMutex &&) = delete;
    ThreadMutex(const ThreadMutex &) = delete;
    auto operator=(ThreadMutex &&) = delete;
    auto operator=(const ThreadMutex &) -> ThreadMutex & = delete;
    ~ThreadMutex() = default;

    void Lock();
    void Unlock();

  private:
    pthread_mutex_t mutex_ = PTHREAD_MUTEX_INITIALIZER;

    friend class ThreadCond;
};

class ThreadCond {
  public:
    ThreadCond() = default;
    ThreadCond(ThreadCond &&) = delete;
    ThreadCond(const ThreadCond &) = delete;
    auto operator=(ThreadCond &&) = delete;
    auto operator=(const ThreadCond &) -> ThreadCond & = delete;
    ~ThreadCond() = default;

    void Broadcast();
    void Wait(ThreadMutex &mutex);

  private:
    pthread_cond_t cond_ = PTHREAD_COND_INITIALIZER;
};
