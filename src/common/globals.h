#include <unistd.h>

#include "array.h"
#include "ipc/shared_memory.h"
#include "queue.h"

struct GlobalParameters {
    int init_drone_count = 5;
    int max_drones_at_base = 2;
    int ignore_suicide_bat_thr = 20;
    int low_bat_thr = 10;
    int max_charges = 0;
};

enum class TunnelDir : uint8_t { IN, OUT, EMPTY };

struct TunnelData {
    TunnelDir dir = TunnelDir::EMPTY;
    unsigned int drones = 0;
};

struct BaseData {
    TunnelData tunnel1;
    TunnelData tunnel2;
};

using ShmParameters = SharedMemory<GlobalParameters>;
using ShmBaseData = SharedMemory<BaseData>;
using ShmDrones = SharedMemory<Array<pid_t>>;
using ProcQueue = Queue<pid_t>;
using ShmProcQueue = SharedMemory<Queue<pid_t>>;
