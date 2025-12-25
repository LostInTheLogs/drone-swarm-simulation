#include "mutex.h"

RWMutex::RWMutex(Semaphore reader_count, Semaphore reader_count_mut,
                 Semaphore writer_mut)
    : reader_count_(reader_count),
      reader_count_mut_(reader_count_mut),
      writer_mut_(writer_mut) {}

void RWMutex::LockRead() {
    reader_count_mut_.Wait();

    reader_count_.Signal();
    if (reader_count_.GetVal() == 1) {
        writer_mut_.Wait();
    }

    reader_count_mut_.Signal();
}
void RWMutex::UnlockRead() {
    reader_count_mut_.Wait();

    reader_count_.Wait();
    if (reader_count_.GetVal() == 0) {
        writer_mut_.Signal();
    }

    reader_count_mut_.Signal();
}

void RWMutex::LockWrite() {
    writer_mut_.Wait();
}
void RWMutex::UnlockWrite() {
    writer_mut_.Signal();
}
