#pragma once

#include "ipc/semaphore_set.h"
class RWMutex {
  public:
    // mut initalized with 1, count with 0
    RWMutex(Semaphore reader_count, Semaphore reader_count_mut,
            Semaphore writer_mut);
    RWMutex(RWMutex &&) = default;
    RWMutex(const RWMutex &) = default;
    auto operator=(RWMutex &&) -> RWMutex & = default;
    auto operator=(const RWMutex &) -> RWMutex & = default;
    ~RWMutex() = default;

    void LockRead();
    void UnlockRead();
    void LockWrite();
    void UnlockWrite();

  private:
    Semaphore reader_count_;
    Semaphore reader_count_mut_;
    Semaphore writer_mut_;
};
