#include <unistd.h>

#include "ipc/shared_memory.h"
#include "shm_queue.h"

struct GlobalParameters {
    int max_drones;
};

using ShmProcQueue = SharedMemory<SmhQueue<pid_t>>;
using ShmParameters = SharedMemory<GlobalParameters>;
