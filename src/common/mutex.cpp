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

RWMutex::RWMutex(Mutex reader_count, Mutex reader_count_mut, Mutex writer_mut)
    : reader_count_(reader_count),
      reader_count_mut_(reader_count_mut),
      writer_mut_(writer_mut) {}

auto RWMutex::LockRead(Retry retry) -> std::expected<void, IpcError> {
    if (auto success = reader_count_mut_.Lock(retry); !success) {
        return success;
    }

    reader_count_.Unlock(retry);
    if (reader_count_.GetVal() == 1) {
        if (auto success = writer_mut_.Lock(retry); !success) {
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

    if (auto success = reader_count_.Lock(retry); !success) {
        throw IpcError(success.error());
    }
    if (reader_count_.GetVal() == 0) {
        writer_mut_.Unlock(retry);
    }

    reader_count_mut_.Unlock(retry);
}

auto RWMutex::LockWrite(Retry retry) -> std::expected<void, IpcError> {
    return writer_mut_.Lock(retry);
}
void RWMutex::UnlockWrite(Retry retry) {
    writer_mut_.Unlock(retry);
}
