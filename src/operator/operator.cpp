#include <sys/wait.h>

#include <cstdio>

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
    int curr_max_drones;
    ShmDrones drones;
    Semaphore free_spots_base;
    RWMutex mut;
};
}  // namespace

auto main(int /*argc*/, char* /*argv*/[]) -> int {
    using namespace std::chrono_literals;

    try {
        auto shm_params = ShmParameters::Get(ShmKey::PARAMS);

        const auto drones_arr_size =
            ShmDrones::value_type::CalcSize(shm_params->max_drones);
        auto sems = SemaphoreSet<SemIds>::Get(SemSetKey::MAIN);

        State state{
            .curr_max_drones = std::max(1, shm_params->max_drones / 2),
            .drones = ShmDrones::Get(ShmKey::DRONES, drones_arr_size),
            .free_spots_base = Semaphore::Get(sems, SemIds::FREE_SPOTS_BASE),
            .mut = RWMutex::Get<RWMUT_SEMS(DRONES)>(sems)};

        state.free_spots_base.Set(shm_params->max_drones_at_base);

        shm_params.Detach();

        auto logger = Logger::Create("operator");

        const auto spawn = [&state]() {
            auto drone = Process::Create({"./drone"});
            state.drones->Insert(drone.GetPid());
            drone.Disown();
        };

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
                   static_cast<size_t>(state.curr_max_drones)) {
                if (!state.free_spots_base.Wait(Retry::NEVER, IPC_NOWAIT)) {
                    logger.Debug("base full");
                    break;
                }
                logger.Debug("Spawning new drone");
                spawn();
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
