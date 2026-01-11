#include <unistd.h>

#include <chrono>

#include "array.h"
#include "ipc/shared_memory.h"
#include "logger.h"
#include "queue.h"
#include "thread_utils.h"

enum TestScenario : uint8_t {
    NO_TEST,
    INC_DEC_DRONE_COUNT,
    PRIORITY_QUEUE,
    SUICIDE_ORDER,
    DEAD_BAT_IN_TUNNEL,
    TUNNEL_DIR_CHANGE,
    COUNT
};

struct GlobalParameters {
    TestScenario scenario = TestScenario::NO_TEST;
    Logger::LogLevel log_level = Logger::TRACE;

    pid_t operator_pid = 0;

    int init_drone_count = 50;    // N
    int max_drones_at_base = 30;  // < N/2

    int max_charges = 0;
    std::chrono::milliseconds battery_lifetime{500};
    std::chrono::milliseconds battery_chargetime{20};

    std::chrono::milliseconds tunnel_length{20};
    unsigned int tun_cap = 2;

    int low_bat_thr = 20;

    void Print(Logger& logger) {
        logger.Debug(std::format("GlobalParameters:"));
        logger.Debug(
            std::format("  scenario = {}", static_cast<int>(scenario)));
        logger.Debug(
            std::format("  N=init_drone_count = {}", init_drone_count));
        logger.Debug(
            std::format("  P=max_drones_at_base = {}", max_drones_at_base));
        logger.Debug(std::format("  Xi=max_charges = {}", max_charges));
        logger.Debug(std::format("  T2=battery_lifetime = {} ms",
                                 battery_lifetime.count()));
        logger.Debug(std::format("  T1=battery_chargetime = {} ms",
                                 battery_chargetime.count()));
        logger.Debug(
            std::format("  tunnel_length = {} ms", tunnel_length.count()));
        logger.Debug(std::format("  tun_cap = {}", tun_cap));
    }
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
struct QueueData {
    PubThreadMutex mut;
    PubThreadCond changed;
    PubThreadMutex can_leave_changed_mut;
    PubThreadCond can_leave_changed;
    ProcQueue queue;  // WARN: MUST BE LAST
};

using ShmProcQueue = SharedMemory<QueueData>;
