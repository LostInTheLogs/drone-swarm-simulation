#include "mutex.h"

RWMutex::RWMutex(Semaphore reader_count, Semaphore reader_count_mut,
                 Semaphore writer_mut)
    : reader_count_(reader_count),
      reader_count_mut_(reader_count_mut),
      writer_mut_(writer_mut) {}

auto RWMutex::LockRead(Retry retry) -> std::expected<void, IpcError> {
    if (auto success = reader_count_mut_.Wait(retry); !success) {
        return success;
    }

    reader_count_.Signal(retry);
    if (reader_count_.GetVal() == 1) {
        if (auto success = writer_mut_.Wait(retry); !success) {
            return success;
        }
    }

    reader_count_mut_.Signal(retry);
    return {};
}

void RWMutex::UnlockRead(Retry retry) {
    if (auto success = reader_count_mut_.Wait(retry); !success) {
        throw IpcError(success.error());
    }

    if (auto success = reader_count_.Wait(retry); !success) {
        throw IpcError(success.error());
    }
    if (reader_count_.GetVal() == 0) {
        writer_mut_.Signal(retry);
    }

    reader_count_mut_.Signal(retry);
}

auto RWMutex::LockWrite(Retry retry) -> std::expected<void, IpcError> {
    return writer_mut_.Wait(retry);
}
void RWMutex::UnlockWrite(Retry retry) {
    writer_mut_.Signal(retry);
}
