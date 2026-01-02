#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <format>

#include "globals.h"
#include "ipc/semaphore_set.h"
#include "logger.h"
#include "mutex.h"
#include "process.h"
#include "thread.h"

namespace {
void Err(auto&& val) {
    if (!val) {
        throw std::forward<decltype(val)>(val).error();
    }
}

struct State {
    int curr_drone_cap;
    ShmDrones drones;
    Semaphore free_spots_base;
    RWMutex mut;
};

constexpr void SetupSignals() {
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGUSR1);
    sigaddset(&sigset, SIGUSR2);
    pthread_sigmask(SIG_BLOCK, &sigset, nullptr);
}
}  // namespace

auto main(int /*argc*/, char* /*argv*/[]) -> int {
    using namespace std::chrono_literals;

    try {
        SetupSignals();
        auto shm_params = ShmParameters::Get(ShmKey::PARAMS);
        const auto drone_hi_cap = shm_params->init_drone_count * 2;
        const auto drone_lo_cap = 1;
        const auto scenario = shm_params->scenario;

        const auto drones_arr_size =
            ShmDrones::value_type::CalcSize(drone_hi_cap);
        auto sems = SemaphoreSet<SemIds>::Get(SemSetKey::MAIN);

        State state{
            .curr_drone_cap = std::max(1, shm_params->init_drone_count),
            .drones = ShmDrones::Get(ShmKey::DRONES, drones_arr_size),
            .free_spots_base = Semaphore::Get(sems, SemIds::FREE_SPOTS_BASE),
            .mut = RWMutex::Get<RWMUT_SEMS(DRONES)>(sems)};

        state.free_spots_base.Set(shm_params->max_drones_at_base);

        shm_params.Detach();

        auto logger = Logger::Create("operator");

        const auto spawn_drone = [&]() {
            if (!state.free_spots_base.Wait(Retry::NEVER, IPC_NOWAIT)) {
                return false;
            }
            logger.Debug("Spawning new drone");
            auto drone = Process::Create({"./drone"});
            state.drones->Insert(drone.GetPid());
            drone.Disown();
            return true;
        };

        [[maybe_unused]]
        auto signal_thread =
            Thread::Create([&state, drone_lo_cap, drone_hi_cap, &logger]() {
                sigset_t sigset;
                sigemptyset(&sigset);
                sigaddset(&sigset, SIGUSR1);
                sigaddset(&sigset, SIGUSR2);

                while (true) {
                    int sig{};
                    sigwait(&sigset, &sig);
                    if (!state.mut.LockWrite()) {
                        break;
                    }
                    if (sig == SIGUSR1) {
                        state.curr_drone_cap *= 2;
                    } else if (sig == SIGUSR2) {
                        state.curr_drone_cap /= 2;
                    }
                    state.curr_drone_cap = std::clamp(
                        state.curr_drone_cap, drone_lo_cap, drone_hi_cap);
                    logger.Info(std::format("Max drone cap updated to {}",
                                            state.curr_drone_cap));
                    state.mut.UnlockWrite();
                }
            });

        const auto scenario_inc_dec_drone_count = [&]() {
            logger.Warning("Waiting for the drone count to stablilize...");
            Err(Thread::SleepFor(100ms));
            Err(state.mut.LockRead());
            logger.Warning(
                std::format("Drone count: {}", state.drones->Size()));
            state.mut.UnlockRead();

            for (int i = 0; i < 2; i++) {
                logger.Warning("Increasing drone count");
                g_curr_process.Signal(SIGUSR1);

                logger.Warning("Waiting for the drone count to stablilize...");
                Err(Thread::SleepFor(1000ms));
                Err(state.mut.LockRead());
                logger.Warning(
                    std::format("Drone count: {}", state.drones->Size()));
                state.mut.UnlockRead();
                Err(Thread::SleepFor(200ms));
                Err(state.mut.LockRead());
                logger.Warning(
                    std::format("Drone count: {}", state.drones->Size()));
                state.mut.UnlockRead();
            }

            for (int i = 0; i < 4; i++) {
                logger.Warning("Decreasing drone count");
                g_curr_process.Signal(SIGUSR2);

                logger.Warning("Waiting for the drone count to stablilize...");
                Err(Thread::SleepFor(1000ms));
                Err(state.mut.LockRead());
                logger.Warning(
                    std::format("Drone count: {}", state.drones->Size()));
                state.mut.UnlockRead();
                Err(Thread::SleepFor(200ms));
                Err(state.mut.LockRead());
                logger.Warning(
                    std::format("Drone count: {}", state.drones->Size()));
                state.mut.UnlockRead();
            }

            g_curr_process.Signal(SIGTERM);
        };
        const auto scenario_priority_queue = [&]() {
            Err(Thread::SleepFor(500ms));
            Err(state.mut.LockWrite());
            state.curr_drone_cap = 0;
            state.mut.UnlockWrite();
            Err(Thread::SleepFor(2000ms));

            g_curr_process.Signal(SIGTERM);
        };
        const auto scenario_suicide_order = [&]() {
            Err(state.mut.LockWrite());
            state.curr_drone_cap = 2;
            state.mut.UnlockWrite();
            Err(Thread::SleepFor(100ms));
            Err(state.mut.LockRead());
            logger.Warning("Sending suicide order");
            Process((*state.drones)[0]).Signal(SIGUSR1);
            state.mut.UnlockRead();

            Err(Thread::SleepFor(200ms));

            Err(state.mut.LockRead());
            logger.Warning("Sending suicide order");
            Process((*state.drones)[1]).Signal(SIGUSR1);
            state.mut.UnlockRead();

            Err(Thread::SleepFor(800ms));

            Err(state.mut.LockRead());
            logger.Warning("Sending suicide order");
            Process((*state.drones)[0]).Signal(SIGUSR1);
            state.mut.UnlockRead();

            Err(Thread::SleepFor(350ms));

            Err(state.mut.LockRead());
            logger.Warning("Sending suicide order");
            Process((*state.drones)[0]).Signal(SIGUSR1);
            state.mut.UnlockRead();

            Err(Thread::SleepFor(200ms));

            g_curr_process.Signal(SIGTERM);
        };
        const auto scenario_dead_bat_in_tunnel = []() {
            Err(Thread::SleepFor(1000ms));
            g_curr_process.Signal(SIGTERM);
        };
        const auto scenario_tunnel_dir_change = []() {
            Err(Thread::SleepFor(1000ms));
            g_curr_process.Signal(SIGTERM);
        };

        switch (scenario) {
            case INC_DEC_DRONE_COUNT:
                (void)Thread::Create(scenario_inc_dec_drone_count);
                break;
            case PRIORITY_QUEUE:
                (void)Thread::Create(scenario_priority_queue);
                break;
            case SUICIDE_ORDER:
                Err(state.mut.LockWrite());
                state.curr_drone_cap = 0;
                state.mut.UnlockWrite();
                (void)Thread::Create(scenario_suicide_order);
                break;
            case DEAD_BAT_IN_TUNNEL:
                (void)Thread::Create(scenario_dead_bat_in_tunnel);
                break;
            case TUNNEL_DIR_CHANGE:
                (void)Thread::Create(scenario_tunnel_dir_change);
                break;
            case NO_TEST:
            case COUNT:
                break;
        }

        while (!CurrentProcess::TerminateReceived()) {
            if (!state.mut.LockWrite()) {
                break;
            }

            pid_t pid = -1;
            while ((pid = waitpid(-1, nullptr, WNOHANG)) > 0) {
                logger.Debug("Zombie drone collected");
                state.drones->Remove(pid);
            }

            while (state.drones->Size() <
                   static_cast<size_t>(state.curr_drone_cap)) {
                if (!spawn_drone()) {
                    break;
                }
            }

            state.mut.UnlockWrite();

            if (!Thread::SleepFor(50ms)) {
                break;
            }
        }

        Err(state.mut.LockWrite(Retry::ALWAYS));
        for (size_t i = 0; i < state.drones->Size(); i++) {
            auto drone = Process((*state.drones)[i]);
            drone.TermWait();
        }
        state.drones->Clear();
        state.mut.UnlockWrite();

    } catch (std::exception& e) {
        LogPrinter::PrintError("operator", e.what());
        return 1;
    }

    return 0;
}
