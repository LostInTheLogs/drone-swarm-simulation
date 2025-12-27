#include <unistd.h>

#include "ipc/shared_memory.h"
#include "queue.h"

struct GlobalParameters {
    int max_drones = 10;
    int ignore_suicide_bat_thr = 20;
    int low_bat_thr = 20;
    int max_charges = 2;
};

enum class TunnelDir : uint8_t { IN, OUT, EMPTY };

struct TunnelData {
    TunnelDir dir = TunnelDir::EMPTY;
    unsigned int drones = 0;
};

using ShmParameters = SharedMemory<GlobalParameters>;
using ShmTunnelData = SharedMemory<TunnelData>;
using ProcQueue = Queue<pid_t>;
using ShmProcQueue = SharedMemory<Queue<pid_t>>;
