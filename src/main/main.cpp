#include <unistd.h>

#include <csignal>
#include <span>
#include <stdexcept>
#include <string>

#include "globals.h"
#include "ipc/ipc.h"
#include "ipc/semaphore_set.h"
#include "ipc/shared_memory.h"
#include "logger.h"
#include "process.h"
#include "queue.h"
#include "thread.h"

namespace {
auto Err(auto&& val) -> decltype(auto) {
    if (!val) {
        throw std::forward<decltype(val)>(val).error();
    }
    return std::forward<decltype(val)>(val).value();
}
}  // namespace

auto main(int argc, char* argv[]) -> int {
    using namespace std::chrono_literals;
    try {
        std::span<char*> args(argv, argc);
        if (args.size() > 2) {
            throw std::runtime_error("Too many arguments!");
        }

        auto shm_params = ShmParameters::Create(ShmKey::PARAMS, 0666);

        if (args.size() == 2) {
            auto scenario = std::stoi(argv[1]);
            if (scenario < 0 || scenario >= TestScenario::COUNT) {
                throw std::runtime_error("This test scenario does not exist!");
            }
            shm_params->scenario = static_cast<TestScenario>(scenario);
        }

        switch (shm_params->scenario) {
            case INC_DEC_DRONE_COUNT:
                shm_params->init_drone_count = 4;
                shm_params->max_drones_at_base = 2;
                shm_params->max_charges = 0;
                shm_params->battery_lifetime = 500ms;
                shm_params->tunnel_length = 1ms;
                shm_params->tun_cap = 10;
                break;
            case PRIORITY_QUEUE:
                shm_params->init_drone_count = 10;
                shm_params->max_drones_at_base = 100;
                shm_params->tun_cap = 1;
                shm_params->tunnel_length = 200ms;
                shm_params->battery_lifetime = 10s;
                break;
            case SUICIDE_ORDER:
                shm_params->init_drone_count = 2;
                shm_params->battery_lifetime = 800ms;
                shm_params->tunnel_length = 200ms;
                break;
            case DEAD_BAT_IN_TUNNEL:
                shm_params->tun_cap = 1;
                shm_params->battery_lifetime = 2000ms;
                shm_params->tunnel_length = 200ms;
                shm_params->init_drone_count = 10;
                shm_params->max_drones_at_base = 100;
                break;
            case TUNNEL_DIR_CHANGE:
                shm_params->init_drone_count = 4;
                shm_params->tun_cap = 1;
                shm_params->tunnel_length = 200ms;
                shm_params->max_drones_at_base = 100;
                break;
            case NO_TEST:
            case COUNT:
                break;
        }

        const auto queue_size =
            Queue<pid_t>::CalcSize(shm_params->init_drone_count);
        auto in_queue =
            ShmProcQueue::Create(ShmKey::IN_QUEUE, 0666, queue_size);
        auto out_queue =
            ShmProcQueue::Create(ShmKey::OUT_QUEUE, 0666, queue_size);

        const auto drones_arr_size =
            ShmDrones::value_type::CalcSize(shm_params->init_drone_count * 2UL);
        auto drones_arr =
            ShmDrones::Create(ShmKey::DRONES, 0666, drones_arr_size);

        auto base = ShmBaseData::Create(ShmKey::BASE_DATA, 0666);

        auto sems = SemaphoreSet<SemIds>::Create(SemSetKey::MAIN, 0666);

        auto logger_process = Process::CreateReady({"./logger"});

        auto logger = Logger::Create("operator");
        shm_params->Print(logger);

        auto operator_proc = Process::Create({"./operator"});
        shm_params->operator_pid = operator_proc.GetPid();
        shm_params.Detach();
        operator_proc.Wait();

        auto slept = Thread::SleepFor(200ms);
        logger_process.TermWait();
    } catch (std::exception& e) {
        LogPrinter::PrintError("main", e.what());
        return 1;
    }
    return 0;
}
