#include "mutex.h"

Mutex::Mutex(Semaphore sem) : sem_(sem) {}

auto Mutex::Lock(Retry retry) -> std::expected<void, IpcError> {
    return sem_.Wait(retry, SEM_UNDO);
}

void Mutex::Unlock(Retry retry) {
    sem_.Signal(retry, SEM_UNDO);
}

auto Mutex::GetVal() const -> int {
    return sem_.GetVal();
}

RWMutex::RWMutex(Semaphore reader_count, Mutex reader_count_mut,
                 Semaphore writer_mut)
    : reader_count_(reader_count),
      reader_count_mut_(reader_count_mut),
      writer_sem_(writer_mut) {}

auto RWMutex::LockRead(Retry retry) -> std::expected<void, IpcError> {
    if (auto success = reader_count_mut_.Lock(retry); !success) {
        return success;
    }

    reader_count_.Signal(retry, SEM_UNDO);
    if (reader_count_.GetVal() == 1) {
        if (auto success = writer_sem_.Wait(retry); !success) {
            return success;
        }
    }

    reader_count_mut_.Unlock(retry);
    return {};
}

void RWMutex::UnlockRead(Retry retry) {
    if (auto success = reader_count_mut_.Lock(retry); !success) {
        throw IpcError(success.error());
    }

    if (auto success = reader_count_.Wait(retry, SEM_UNDO); !success) {
        throw IpcError(success.error());
    }
    if (reader_count_.GetVal() == 0) {
        writer_sem_.Signal(retry);
    }

    reader_count_mut_.Unlock(retry);
}

auto RWMutex::LockWrite(Retry retry) -> std::expected<void, IpcError> {
    return writer_sem_.Wait(retry, SEM_UNDO);
}
void RWMutex::UnlockWrite(Retry retry) {
    writer_sem_.Signal(retry, SEM_UNDO);
}
