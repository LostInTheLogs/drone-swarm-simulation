#include "msg_queue.h"

#include <sys/ipc.h>
#include <sys/msg.h>

#include <cassert>
#include <cstring>

using std::expected, std::unexpected;

IpcMessageQueue::IpcMessageQueue(int queue_id, bool owner)
    : id_(queue_id), owner_(owner) {};

IpcMessageQueue::IpcMessageQueue(IpcMessageQueue&& other) noexcept
    : id_(other.id_), owner_(other.owner_) {
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
    return IpcMessageQueue(id_, false);
}

auto IpcMessageQueue::Create(MsgQueueKey queue_key, unsigned int permissions)
    -> expected<IpcMessageQueue, IpcError> {
    auto key = static_cast<key_t>(queue_key);
    auto queue_id = GetQueueId(queue_key, permissions | IPC_CREAT | IPC_EXCL);
    if (!queue_id) {
        return unexpected(IpcError(IpcType::MESSAGE_QUEUE, key, -1, errno));
    }
    return IpcMessageQueue(*queue_id, true);
}

auto IpcMessageQueue::GetOrCreate(MsgQueueKey queue_key,
                                  unsigned int permissions, bool owner)
    -> expected<IpcMessageQueue, IpcError> {
    auto key = static_cast<key_t>(queue_key);
    auto queue_id = GetQueueId(queue_key, permissions | IPC_CREAT);
    if (!queue_id) {
        return unexpected(IpcError(IpcType::MESSAGE_QUEUE, key, -1, errno));
    }
    return IpcMessageQueue(*queue_id, owner);
}

auto IpcMessageQueue::Get(MsgQueueKey queue_key)
    -> expected<IpcMessageQueue, IpcError> {
    auto key = static_cast<key_t>(queue_key);
    auto queue_id = GetQueueId(queue_key, 0);
    if (!queue_id) {
        return unexpected(IpcError(IpcType::MESSAGE_QUEUE, key, -1, errno));
    }
    return IpcMessageQueue(*queue_id);
}

auto IpcMessageQueue::GetQueueId(MsgQueueKey queue_key, unsigned int flags)
    -> expected<int, IpcError> {
    auto key = static_cast<key_t>(queue_key);
    auto queue_id = msgget(key, static_cast<int>(flags));
    if (queue_id < 0) {
        return unexpected(IpcError(IpcType::MESSAGE_QUEUE, key, -1, errno));
    }
    return queue_id;
}

auto IpcMessageQueue::Remove() -> expected<void, IpcError> {
    if (owner_) {
        auto success = msgctl(id_, IPC_RMID, nullptr);
        if (success == -1) {
            return unexpected(IpcError(IpcType::MESSAGE_QUEUE, -1, id_, errno));
        }
    }
    owner_ = false;

    return {};
}
