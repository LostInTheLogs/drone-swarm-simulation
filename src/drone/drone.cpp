#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <format>

#include "clock.h"
#include "globals.h"
#include "ipc/ipc.h"
#include "ipc/semaphore_set.h"
#include "logger.h"
#include "mutex.h"
#include "thread.h"
#include "thread_utils.h"

using namespace std::chrono_literals;

namespace {

auto HandleExpectedError(const auto& expected) {
    if (!expected) {
        LogPrinter::PrintError("drone", expected.error().what());
    }
    return static_cast<bool>(expected);
}

constexpr auto GetLogger() -> Logger& {
    static auto g_logger = Logger::Create("drone");
    return g_logger;
}

constexpr auto g_ignore_suicide_bat_thr = 20;
constexpr auto g_low_bat_thr = 20;
constexpr auto g_max_charges = 2;

struct DroneState {
    ThreadMutex mutex;
    ThreadCond changed;
    int bat_level = 50;
    int charges = 0;
    bool docked = false;
    bool suicide_order_received = false;
};

constexpr auto ShouldReturn(const DroneState& state) -> bool {
    return !state.docked && state.bat_level < g_low_bat_thr &&
           !state.suicide_order_received;
};
constexpr auto ShouldLeave(const DroneState& state) -> bool {
    return state.docked &&
           (state.bat_level == 100 || state.suicide_order_received);
};
constexpr auto StateChanged(const DroneState& state) -> bool {
    return (ShouldLeave(state) || ShouldReturn(state) || state.bat_level <= 0 ||
            CurrentProcess::TerminateReceived());
};

constexpr void SetupSignals() {
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGUSR1);
    pthread_sigmask(SIG_BLOCK, &sigset, nullptr);
}

constexpr void SignalThread(DroneState& state) {
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGUSR1);

    while (true) {
        int sig{};
        sigwait(&sigset, &sig);

        state.mutex.Lock();
        if (state.bat_level < g_ignore_suicide_bat_thr) {
            GetLogger().Info("Suicide mission order ignored");
            state.mutex.Unlock();
            continue;
        }
        state.suicide_order_received = true;
        GetLogger().Info("Suicide mission order accepted");
        state.changed.Broadcast();
        state.mutex.Unlock();
    }
}

constexpr void BatteryThread(DroneState& state) {
    auto next = MonotonicClock::now();
    const auto dur = 50ms;

    while (!CurrentProcess::TerminateReceived()) {
        next += dur;
        auto slept = Thread::SleepUntil(next);
        if (!slept) {
            GetLogger().Info("Sleep interruped");
            CurrentProcess::Get().Signal(SIGTERM);
            return;
        }

        state.mutex.Lock();
        if (state.docked) {
            state.bat_level++;
        } else {
            state.bat_level--;
        }
        auto clamped = std::clamp(state.bat_level, 0, 100);
        if (state.bat_level == clamped && clamped % 10 == 0) {
            GetLogger().Debug(std::format("Bat: {:>3}%", clamped));
        }
        state.bat_level = clamped;

        if (StateChanged(state)) {
            state.changed.Broadcast();
        }

        if (state.bat_level <= 0) {
            GetLogger().Warning("Battery died!");
            CurrentProcess::Get().Signal(SIGTERM);
            state.mutex.Unlock();
            return;
        }

        state.mutex.Unlock();
    }
}

constexpr void MainThread(DroneState& state) {
    state.mutex.Lock();
    while (true) {
        while (!StateChanged(state)) {
            state.changed.Wait(state.mutex);
        }
        if (state.bat_level <= 0 || CurrentProcess::TerminateReceived()) {
            break;
        }

        if (ShouldLeave(state)) {
            state.charges++;
            state.mutex.Unlock();

            GetLogger().Info("Leaving the base");
            if (!Thread::SleepFor(500ms)) {  // TODO: leave base
                break;
            }

            state.mutex.Lock();
            state.docked = false;
            GetLogger().Info("Left the base");
            continue;
        }
        if (ShouldReturn(state)) {
            state.mutex.Unlock();

            GetLogger().Info("Returning to the base");
            if (!Thread::SleepFor(500ms)) {  // TODO: go to base
                break;
            }

            state.mutex.Lock();
            GetLogger().Info("Back at the base");
            if (state.charges == g_max_charges) {
                GetLogger().Info("Max charging cycles, decomissioning");
                CurrentProcess::Get().Signal(SIGTERM);
                break;
            }
            state.docked = true;
            continue;
        }
    }
    state.mutex.Unlock();
}
}  // namespace

auto main(int /*argc*/, char* /*argv*/[]) -> int {
    try {
        GetLogger().Info("Hello world");

        SetupSignals();

        DroneState state;

        // auto shm_params = ShmParameters::Create(ShmKey::PARAMS, 0666);
        // shm_params->max_drones = 10;
        //
        // const auto queue_size =
        //     SmhQueue<pid_t>::CalcSize(shm_params->max_drones);
        // auto shm_in_queue = ShmProcQueue::Get(ShmKey::QUEUE1, queue_size);
        // auto sems = SemaphoreSet<SemIds>::Get(SemSetKey::MAIN);
        // auto in_queue_mut = RWMutex::Get<SemIds::QUEUE1_A0,
        // SemIds::QUEUE1_B1,
        //                                  SemIds::QUEUE1_C1>(sems);
        // auto smh_out_queue = ShmProcQueue::Get(ShmKey::QUEUE2, queue_size);
        // auto out_queue_mut = RWMutex::Get<SemIds::QUEUE2_A0,
        // SemIds::QUEUE2_B1,
        //                                   SemIds::QUEUE2_C1>(sems);
        // shm_params.Detach();

        [[maybe_unused]] const auto signal_thread =
            Thread::Create([&state]() { SignalThread(state); });

        [[maybe_unused]] const auto battery_thread =
            Thread::Create([&state]() { BatteryThread(state); });

        MainThread(state);

        GetLogger().Info("Goodbye");
    } catch (std::exception& e) {
        LogPrinter::PrintError("drone", e.what());
        return 1;
    }

    return 0;
}
