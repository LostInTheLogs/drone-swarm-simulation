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
    sigaddset(&sigset, SIGCHLD);
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
            drone.Detach();
            return true;
        };

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
            Err(Thread::SleepFor(200ms));
            Err(state.mut.LockRead());
            logger.Warning("Sending suicide order");
            Process((*state.drones)[0]).Signal(SIGUSR1);
            state.mut.UnlockRead();

            Err(Thread::SleepFor(200ms));

            Err(state.mut.LockRead());
            logger.Warning("Sending suicide order");
            Process((*state.drones)[1]).Signal(SIGUSR1);
            state.mut.UnlockRead();

            Err(Thread::SleepFor(1800ms));

            Err(state.mut.LockRead());
            logger.Warning("Sending suicide order");
            Process((*state.drones)[0]).Signal(SIGUSR1);
            state.mut.UnlockRead();

            Err(Thread::SleepFor(700ms));

            Err(state.mut.LockRead());
            logger.Warning("Sending suicide order");
            Process((*state.drones)[0]).Signal(SIGUSR1);
            state.mut.UnlockRead();

            Err(Thread::SleepFor(400ms));

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
                Thread::Create(scenario_inc_dec_drone_count).Detach();
                break;
            case PRIORITY_QUEUE:
                Thread::Create(scenario_priority_queue).Detach();
                break;
            case SUICIDE_ORDER:
                Err(state.mut.LockWrite());
                state.curr_drone_cap = 0;
                state.mut.UnlockWrite();
                Thread::Create(scenario_suicide_order).Detach();
                break;
            case DEAD_BAT_IN_TUNNEL:
                Thread::Create(scenario_dead_bat_in_tunnel).Detach();
                break;
            case TUNNEL_DIR_CHANGE:
                Thread::Create(scenario_tunnel_dir_change).Detach();
                break;
            case NO_TEST:
            case COUNT:
                break;
        }

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
        signal_thread.Detach();

        auto reaper_thread = Thread::Create([&]() {
            sigset_t sigset;
            sigemptyset(&sigset);
            sigaddset(&sigset, SIGCHLD);

            while (true) {
                int sig{};
                sigwait(&sigset, &sig);

                if (CurrentProcess::TerminateReceived()) {
                    return;
                }

                if (!state.mut.LockWrite()) {
                    break;
                }

                pid_t pid = -1;
                while (true) {
                    int status = 0;
                    pid = waitpid(-1, &status, WNOHANG);
                    if (pid == -1 && errno == EINTR) {
                        continue;
                    }
                    if (pid <= 0) {
                        break;
                    }
                    if (WIFEXITED(status)) {
                        int exit_code = WEXITSTATUS(status);
                        if (exit_code != 0) {
                            logger.Error(std::format("Drone {} exited with {}",
                                                     pid, exit_code));
                        }
                    } else if (WIFSIGNALED(status)) {
                        logger.Error(std::format("Drone {} killed by signal {}",
                                                 pid, WTERMSIG(status)));
                    }
                    logger.Debug("Zombie drone collected");
                    state.drones->Remove(pid);
                }

                state.mut.UnlockWrite();
            }
        });

        while (!CurrentProcess::TerminateReceived()) {
            if (!state.mut.LockWrite()) {
                break;
            }

            while (state.drones->Size() <
                   static_cast<size_t>(state.curr_drone_cap)) {
                if (!spawn_drone()) {
                    break;
                }
            }

            state.mut.UnlockWrite();

            if (!Thread::SleepFor(90ms)) {
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

        g_curr_process.Signal(SIGCHLD);  // wakes up the reaper
        reaper_thread.Join();
    } catch (std::exception& e) {
        LogPrinter::PrintError("operator", e.what());
        return 1;
    }

    return 0;
}
