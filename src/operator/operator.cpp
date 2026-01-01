#include <sys/wait.h>

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

        while (true) {
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
                if (!state.free_spots_base.Wait(Retry::NEVER, IPC_NOWAIT)) {
                    break;
                }

                logger.Debug("Spawning new drone");
                auto drone = Process::Create({"./drone"});
                state.drones->Insert(drone.GetPid());
                drone.Disown();
            }

            state.mut.UnlockWrite();

            if (!Thread::SleepFor(1s)) {
                break;
            }
        }

        Err(state.mut.LockWrite(Retry::ALWAYS));
        for (size_t i = 0; i < state.drones->Size(); i++) {
            auto drone = Process((*state.drones)[i]);
            drone.Wait();
        }
        state.drones->Clear();
        state.mut.UnlockWrite();

    } catch (std::exception& e) {
        LogPrinter::PrintError("operator", e.what());
        return 1;
    }

    return 0;
}
