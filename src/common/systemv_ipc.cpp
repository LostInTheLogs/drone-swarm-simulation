#include "systemv_ipc.h"

#include <sys/ipc.h>
#include <sys/msg.h>

#include <cassert>
#include <cstring>
#include <format>
#include <system_error>

using std::expected, std::unexpected, std::format;

IpcError::IpcError(IpcType ipc_type, key_t key, int ipc_id, int error)
    : std::system_error(error, std::generic_category(),
                        format("IPC Error in {} key '{}' id '{}'",
                               IpcTypeToStr(ipc_type), key, ipc_id)) {}

auto IpcError::IpcTypeToStr(IpcType ipc_type) -> const char* {
    switch (ipc_type) {
        case IpcType::MESSAGE_QUEUE:
            return "message queue";
        case IpcType::SEMAPHORE:
            return "semaphore";
        case IpcType::SHARED_MEMORY:
            return "shared memory";
    }

    return "";
}

IpcMessageQueue::IpcMessageQueue(key_t key, int queue_id, bool owner)
    : key_(key), queue_id_(queue_id), owner_(owner) {};

auto IpcMessageQueue::Create(MsgQueueKey queue_key, unsigned int permissions)
    -> expected<IpcMessageQueue, IpcError> {
    auto key = static_cast<key_t>(queue_key);
    auto flags = permissions | IPC_CREAT | IPC_EXCL;
    auto queue_id = msgget(key, static_cast<int>(flags));
    if (queue_id < 0) {
        return unexpected(IpcError(IpcType::MESSAGE_QUEUE, key, -1, errno));
    }
    return IpcMessageQueue(key, queue_id, true);
}

auto IpcMessageQueue::Get(MsgQueueKey queue_key)
    -> expected<IpcMessageQueue, IpcError> {
    auto key = static_cast<key_t>(queue_key);
    auto queue_id = msgget(key, 0);
    if (queue_id < 0) {
        return unexpected(IpcError(IpcType::MESSAGE_QUEUE, key, -1, errno));
    }
    return IpcMessageQueue(key, queue_id);
}

auto IpcMessageQueue::Remove() const -> expected<void, IpcError> {
    if (owner_) {
        auto success = msgctl(queue_id_, IPC_RMID, nullptr);
        if (success == -1) {
            return std::unexpected(
                IpcError(IpcType::MESSAGE_QUEUE, key_, queue_id_, errno));
        }
    }

    return {};
}
