#include "ipc.h"

#include <sys/ipc.h>
#include <sys/msg.h>

#include <format>

using std::format;

IpcError::IpcError(IpcType ipc_type, key_t key, int ipc_id, int error)
    : std::system_error(error, std::generic_category(),
                        format("IPC Error in {} key '{}' id '{}'",
                               IpcTypeToStr(ipc_type), key, ipc_id)) {}

auto IpcError::IpcTypeToStr(IpcType ipc_type) -> const char* {
    switch (ipc_type) {
        case IpcType::MESSAGE_QUEUE:
            return "message queue";
        case IpcType::SEMAPHORE_SET:
            return "semaphore set";
        case IpcType::SHARED_MEMORY:
            return "shared memory";
    }

    return "";
}
