#include <unistd.h>

#include <csignal>
#include <cstdio>
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
        if (args.size() > 3) {
            throw std::runtime_error("Too many arguments!");
        }

        auto shm_params = ShmParameters::Create(ShmKey::PARAMS, 0600);

        if (args.size() >= 2) {
            auto scenario = std::stoi(argv[1]);
            if (scenario < 0 || scenario >= TestScenario::COUNT) {
                throw std::runtime_error("This test scenario does not exist!");
            }
            shm_params->scenario = static_cast<TestScenario>(scenario);
        }

        if (args.size() >= 3) {
            char level = argv[2][0];
            switch (level) {
                case 'T':
                    shm_params->log_level = Logger::TRACE;
                    break;
                case 'D':
                    shm_params->log_level = Logger::DEBUG;
                    break;
                case 'I':
                    shm_params->log_level = Logger::INFO;
                    break;
                case 'W':
                    shm_params->log_level = Logger::WARNING;
                    break;
                case 'E':
                    shm_params->log_level = Logger::ERROR;
                    break;
                default:
                    throw std::runtime_error(
                        "Log level can only be one of T/D/I/W/E!");
                    break;
            }
        }

        switch (shm_params->scenario) {
            case INC_DEC_DRONE_COUNT:
                shm_params->init_drone_count = 4;
                shm_params->max_drones_at_base = 2;
                shm_params->max_charges = 0;
                shm_params->battery_lifetime = 500ms;
                shm_params->battery_chargetime = 400ms;
                shm_params->tunnel_length = 1ms;
                shm_params->tun_cap = 10;
                break;
            case PRIORITY_QUEUE:
                shm_params->init_drone_count = 10;
                shm_params->max_drones_at_base = 100;
                shm_params->tun_cap = 1;
                shm_params->tunnel_length = 200ms;
                shm_params->battery_lifetime = 10s;
                shm_params->battery_chargetime = 400ms;
                break;
            case SUICIDE_ORDER:
                shm_params->init_drone_count = 2;
                shm_params->battery_lifetime = 1600ms;
                shm_params->battery_chargetime = 800ms;
                shm_params->tunnel_length = 200ms;
                break;
            case DEAD_BAT_IN_TUNNEL:
                shm_params->tun_cap = 1;
                shm_params->battery_lifetime = 1500ms;
                shm_params->battery_chargetime = 400ms;
                shm_params->tunnel_length = 600ms;
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

        const auto queue_data_size =
            sizeof(ShmProcQueue::value_type) +
            Queue<pid_t>::CalcExtraSize(shm_params->init_drone_count * 2UL);
        auto in_queue =
            ShmProcQueue::Create(ShmKey::IN_QUEUE, 0600, queue_data_size);
        in_queue.Detach();
        auto out_queue =
            ShmProcQueue::Create(ShmKey::OUT_QUEUE, 0600, queue_data_size);
        out_queue.Detach();

        const auto drones_arr_size =
            ShmDrones::value_type::CalcSize(shm_params->init_drone_count * 2UL);
        auto drones_arr =
            ShmDrones::Create(ShmKey::DRONES, 0600, drones_arr_size);

        auto base = ShmBaseData::Create(ShmKey::BASE_DATA, 0600);

        auto sems = SemaphoreSet<SemIds>::Create(SemSetKey::MAIN, 0600);

        auto logger_process = Process::CreateReady({"./logger"});

        auto logger = Logger::Create("operator");
        shm_params->Print(logger);

        auto operator_proc = Process::Create({"./operator"});
        shm_params->operator_pid = operator_proc.GetPid();
        shm_params.Detach();
        operator_proc.Wait();

        logger_process.TermWait();
    } catch (std::exception& e) {
        LogPrinter::PrintError("main", e.what());
        return 1;
    }
    return 0;
}
