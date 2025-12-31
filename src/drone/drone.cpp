#include <iostream>

#include "globals.h"
#include "logger.h"
#include "mutex.h"
#include "process.h"
#include "thread.h"
#include "thread_utils.h"

using namespace std::chrono_literals;

namespace {

constexpr auto GetLogger() -> Logger& {
    static auto g_logger = Logger::Create("drone");
    return g_logger;
}

struct Tunnel {
    TunnelData* data;
    Mutex mutex;
};

struct DroneState {
    ThreadMutex mutex;
    ThreadCond changed;
    int bat_level = 100;
    int charges = 0;
    bool docked = true;
    bool at_base = true;
    bool suicide_order_received = false;
};

struct BaseState {
    Semaphore free_spots;
};

unsigned int g_tun_cap = 1;

constexpr auto ShouldReturn(const GlobalParameters& params,
                            const DroneState& state) -> bool {
    return !state.docked && state.bat_level < params.low_bat_thr &&
           !state.suicide_order_received;
};
constexpr auto ShouldLeave(const DroneState& state) -> bool {
    return state.docked &&
           (state.bat_level == 100 || state.suicide_order_received);
};
constexpr auto StateChanged(const GlobalParameters& params,
                            const DroneState& state) -> bool {
    return (ShouldLeave(state) || ShouldReturn(params, state) ||
            state.bat_level <= 0 || CurrentProcess::TerminateReceived());
};

constexpr void SetupSignals() {
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGUSR1);
    pthread_sigmask(SIG_BLOCK, &sigset, nullptr);
}

constexpr void SignalThread(const GlobalParameters& params, DroneState& state) {
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGUSR1);

    while (true) {
        int sig{};
        sigwait(&sigset, &sig);

        state.mutex.Lock();
        if (state.bat_level < params.ignore_suicide_bat_thr) {
            GetLogger().Info("Suicide mission order ignored");
            state.mutex.Unlock();
            continue;
        }
        state.suicide_order_received = true;
        GetLogger().Info("Suicide mission order accepted");
        state.changed.Broadcast();
        state.mutex.Unlock();
        return;
    }
}

constexpr void BatteryThread(const GlobalParameters& params,
                             DroneState& state) {
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

        if (StateChanged(params, state)) {
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

void Err(auto&& val) {
    if (!val) {
        throw std::forward<decltype(val)>(val).error();
    }
}

constexpr auto WaitInQueue(DroneState& state, ProcQueue& queue, RWMutex& mut)
    -> std::expected<void, std::monostate> {
    const auto pid = g_curr_process.GetPid();
    int not_pid = pid - 1;

    if (!mut.LockWrite()) {
        return std::unexpected(std::monostate());
    }
    state.mutex.Lock();
    queue.Push(pid, 100 - state.bat_level);
    state.mutex.Unlock();
    mut.UnlockWrite();

    Err(mut.LockRead(Retry::ALWAYS));
    while (queue.Peek().value_or(not_pid) != pid) {
        mut.UnlockRead();
        if (!Thread::SleepFor(50ms) || CurrentProcess::TerminateReceived()) {
            Err(mut.LockWrite(Retry::ALWAYS));
            queue.Remove(pid);
            mut.UnlockWrite();
            return std::unexpected(std::monostate());
        }
        Err(mut.LockRead(Retry::ALWAYS));
    }
    mut.UnlockRead();

    return {};
};

constexpr auto EnterOneTunnel(TunnelDir dir, Tunnel& tun)
    -> std::expected<bool, std::monostate> {
    bool entered = false;
    if (!tun.mutex.Lock()) {
        return std::unexpected(std::monostate());
    }
    entered = tun.data->drones == 0 ||
              (tun.data->dir == dir && tun.data->drones < g_tun_cap);
    if (entered) {
        tun.data->dir = dir;
        tun.data->drones++;
    }
    tun.mutex.Unlock();
    return entered;
}

constexpr auto EnterTunnels(TunnelDir dir, Tunnel& tun1, Tunnel& tun2)
    -> std::expected<std::reference_wrapper<Tunnel>, std::monostate> {
    while (true) {
        auto entered = EnterOneTunnel(dir, tun1);
        if (!entered) {
            return std::unexpected(std::monostate());
        }
        if (*entered) {
            return tun1;
        }

        entered = EnterOneTunnel(dir, tun2);
        if (!entered) {
            return std::unexpected(std::monostate());
        }
        if (*entered) {
            return tun2;
        }

        if (!Thread::SleepFor(50ms)) {
            return std::unexpected(std::monostate());
        }
    }
};

constexpr auto EnterExitSequence(DroneState& state, BaseState& base,
                                 ProcQueue& queue, RWMutex& queue_mut,
                                 TunnelDir dir, Tunnel& tun1, Tunnel& tun2)
    -> std::expected<bool, std::monostate> {
    if (!WaitInQueue(state, queue, queue_mut)) {
        return std::unexpected(std::monostate());
    }

    if (dir == TunnelDir::IN) {
        if (!base.free_spots.Wait()) {
            Err(queue_mut.LockWrite(Retry::ALWAYS));
            queue.Remove(g_curr_process.GetPid());
            queue_mut.UnlockWrite();
            return std::unexpected(std::monostate());
        }
        state.mutex.Lock();
        state.at_base = true;
        state.mutex.Unlock();
    }

    auto entered_tun = EnterTunnels(dir, tun1, tun2);

    // leave the queue even if entering tunnel interruped
    Err(queue_mut.LockWrite(Retry::ALWAYS));
    queue.Remove(g_curr_process.GetPid());
    queue_mut.UnlockWrite();

    if (!entered_tun) {
        base.free_spots.Signal(Retry::ALWAYS);
        state.mutex.Lock();
        state.at_base = false;
        state.mutex.Unlock();
        return std::unexpected(std::monostate());
    }

    if (dir == TunnelDir::OUT) {
        state.mutex.Lock();
        state.docked = false;
        state.mutex.Unlock();
        GetLogger().Info("Left the charging pad");
    }

    auto slept = Thread::SleepFor(500ms);

    // leave the tunnel even if sleep interruped
    auto& tun = (*entered_tun).get();
    Err(tun.mutex.Lock(Retry::ALWAYS));
    tun.data->drones--;
    if (tun.data->drones == 0) {
        tun.data->dir = TunnelDir::EMPTY;
    }
    tun.mutex.Unlock();

    if (!slept) {
        base.free_spots.Signal(Retry::ALWAYS);
        state.mutex.Lock();
        state.at_base = false;
        state.mutex.Unlock();
        return std::unexpected(std::monostate());
    }

    if (dir == TunnelDir::IN) {
        state.mutex.Lock();
        state.docked = true;
        state.mutex.Unlock();
        GetLogger().Info("Back on the charging pad");
    }

    if (dir == TunnelDir::OUT) {
        base.free_spots.Signal(Retry::ALWAYS);
        state.mutex.Lock();
        state.at_base = false;
        state.mutex.Unlock();
    }

    return {};
}

constexpr void MainThread(const GlobalParameters& params, DroneState& state) {
    const auto queue_size = Queue<pid_t>::CalcSize(params.init_drone_count);

    auto sems = SemaphoreSet<SemIds>::Get(SemSetKey::MAIN);

    auto in_queue = ShmProcQueue::Get(ShmKey::IN_QUEUE, queue_size);
    auto in_mut = RWMutex::Get<RWMUT_SEMS(IN_QUEUE)>(sems);
    auto out_queue = ShmProcQueue::Get(ShmKey::OUT_QUEUE, queue_size);
    auto out_mut = RWMutex::Get<RWMUT_SEMS(OUT_QUEUE)>(sems);

    auto data = ShmBaseData::Get(ShmKey::BASE_DATA);

    Tunnel tunnel1{.data = &data->tunnel1,
                   .mutex = Mutex(Semaphore::Get(sems, SemIds::TUNNEL1_1))};
    Tunnel tunnel2{.data = &data->tunnel2,
                   .mutex = Mutex(Semaphore::Get(sems, SemIds::TUNNEL2_1))};

    BaseState base{.free_spots = Semaphore::Get(sems, SemIds::FREE_SPOTS_BASE)};

    state.mutex.Lock();
    while (true) {
        while (!StateChanged(params, state)) {
            state.changed.Wait(state.mutex);
        }
        if (state.bat_level <= 0 || CurrentProcess::TerminateReceived()) {
            break;
        }

        if (ShouldLeave(state)) {
            state.charges++;
            state.mutex.Unlock();

            GetLogger().Info("Leaving the base");
            if (!EnterExitSequence(state, base, *out_queue, out_mut,
                                   TunnelDir::OUT, tunnel1, tunnel2)) {
                return;
            }
            GetLogger().Info("Left the base");

            state.mutex.Lock();
            continue;
        }
        if (ShouldReturn(params, state)) {
            state.mutex.Unlock();

            GetLogger().Info("Returning to the base");
            if (!EnterExitSequence(state, base, *in_queue, in_mut,
                                   TunnelDir::IN, tunnel2, tunnel1)) {
                return;
            }
            GetLogger().Info("Back at the base");

            state.mutex.Lock();
            if (state.charges >= params.max_charges) {
                GetLogger().Info("Max charging cycles, decomissioning");
                base.free_spots.Signal(Retry::ALWAYS);
                state.at_base = false;
                CurrentProcess::Get().Signal(SIGTERM);
                break;
            }
            continue;
        }
    }
    if (state.at_base) {
        base.free_spots.Signal(Retry::ALWAYS);
        state.at_base = false;
    }
    state.mutex.Unlock();
}
}  // namespace

auto main(int /*argc*/, char* /*argv*/[]) -> int {
    try {
        GetLogger().Info("Hello world");

        SetupSignals();

        auto params = ShmParameters::Get(ShmKey::PARAMS);
        DroneState state;

        [[maybe_unused]] const auto signal_thread = Thread::Create(
            [&state, &params]() { SignalThread(*params, state); });

        [[maybe_unused]] const auto battery_thread = Thread::Create(
            [&state, &params]() { BatteryThread(*params, state); });

        MainThread(*params, state);

        GetLogger().Info("Goodbye");
    } catch (std::exception& e) {
        LogPrinter::PrintError("drone", e.what());
        return 1;
    }

    return 0;
}
