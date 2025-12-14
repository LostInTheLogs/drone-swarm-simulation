#include "msg_queue.h"

#include <sys/ipc.h>
#include <sys/msg.h>

#include <cassert>
#include <cstring>

using std::expected, std::unexpected;

IpcMessageQueue::IpcMessageQueue(key_t key, int queue_id, bool owner)
    : key_(key), id_(queue_id), owner_(owner) {};

IpcMessageQueue::IpcMessageQueue(IpcMessageQueue&& other) noexcept
    : key_(other.key_), id_(other.id_), owner_(other.owner_) {
    other.owner_ = false;
}

auto IpcMessageQueue::operator=(IpcMessageQueue&& other) noexcept
    -> IpcMessageQueue& {
    if (this == &other) {
        return *this;
    }
    if (owner_) {
        auto removed = Remove();
    }
    key_ = other.key_;
    id_ = other.id_;
    owner_ = other.owner_;
    other.owner_ = false;

    return *this;
}

IpcMessageQueue::~IpcMessageQueue() {
    if (owner_) {
        auto removed = Remove();
    }
}

auto IpcMessageQueue::Copy() const -> IpcMessageQueue {
    return IpcMessageQueue(key_, id_, false);
}

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

auto IpcMessageQueue::Remove() -> expected<void, IpcError> {
    if (owner_) {
        auto success = msgctl(id_, IPC_RMID, nullptr);
        if (success == -1) {
            return unexpected(
                IpcError(IpcType::MESSAGE_QUEUE, key_, id_, errno));
        }
    }
    owner_ = false;

    return {};
}
