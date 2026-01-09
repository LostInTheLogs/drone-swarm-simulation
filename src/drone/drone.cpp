#include <unistd.h>

#include <format>
#include <iostream>
#include <random>

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
    int id;
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
        if (state.bat_level < params.low_bat_thr) {
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
    auto dur = 0ms;

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
            dur = params.battery_chargetime / 100;
            state.bat_level++;
        } else {
            dur = params.battery_lifetime / 100;
            state.bat_level--;
        }
        auto clamped = std::clamp(state.bat_level, 0, 100);
        if (state.bat_level == clamped && clamped % 10 == 0) {
            GetLogger().Debug(std::format("Bat: {:>3}%", clamped));
            if (clamped <= 20 || clamped >= 80) {
                GetLogger().Info(std::format("Bat: {:>3}%", clamped));
            }
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
        state.changed.Broadcast();
    }
}

void Err(auto&& val) {
    if (!val) {
        throw std::forward<decltype(val)>(val).error();
    }
}

constexpr auto EnterOneTunnel(const GlobalParameters& params, TunnelDir dir,
                              Tunnel& tun)
    -> std::expected<bool, std::monostate> {
    bool entered = false;
    if (!tun.mutex.Lock()) {
        return std::unexpected(std::monostate());
    }
    entered = tun.data->drones == 0 ||
              (tun.data->dir == dir && tun.data->drones < params.tun_cap);
    if (entered) {
        GetLogger().Trace(std::format("Entering tun {} dir: {}", tun.id,
                                      (dir == TunnelDir::IN ? "in" : "out")));
        tun.data->dir = dir;
        tun.data->drones++;
    }
    tun.mutex.Unlock();
    return entered;
}

constexpr auto WaitForTunInQueue(const GlobalParameters& params,
                                 DroneState& state, BaseState& base,
                                 TunnelDir dir, ProcQueue& queue, RWMutex& mut,
                                 Tunnel& tun1, Tunnel& tun2)
    -> std::expected<std::reference_wrapper<Tunnel>, std::monostate> {
    const auto pid = g_curr_process.GetPid();
    int not_pid = pid - 1;

    if (!mut.LockWrite()) {
        return std::unexpected(std::monostate());
    }
    state.mutex.Lock();
    const auto prio = (dir == TunnelDir::OUT ? 0 : 100 - state.bat_level);
    GetLogger().Trace(std::format("Added to queue, priority {}", prio));
    queue.Push(pid, prio);
    state.mutex.Unlock();
    mut.UnlockWrite();

    const auto leave_queue = [&]() {
        Err(mut.LockWrite(Retry::ALWAYS));
        queue.Remove(pid);
        mut.UnlockWrite();
    };
    const auto leave_tun = [&]() {
        state.mutex.Lock();
        if (state.at_base) {
            base.free_spots.Signal(Retry::ALWAYS);
            state.at_base = false;
        }
        state.mutex.Unlock();
    };

    Err(mut.LockRead(Retry::ALWAYS));
    while (true) {
        if (queue.Peek().value_or(not_pid) == pid) {
            bool can_go = true;
            if (dir == TunnelDir::IN) {
                if (base.free_spots.Wait(Retry::NEVER, IPC_NOWAIT)) {
                    state.mutex.Lock();
                    state.at_base = true;
                    state.mutex.Unlock();
                } else {
                    can_go = false;
                }
            }
            if (can_go) {
                auto entered = EnterOneTunnel(params, dir, tun1);
                if (!entered) {
                    mut.UnlockRead();
                    leave_queue();
                    leave_tun();
                    return std::unexpected(std::monostate());
                }
                if (*entered) {
                    mut.UnlockRead();
                    leave_queue();
                    return tun1;
                }

                entered = EnterOneTunnel(params, dir, tun2);
                if (!entered) {
                    mut.UnlockRead();
                    leave_queue();
                    leave_tun();
                    return std::unexpected(std::monostate());
                }
                if (*entered) {
                    mut.UnlockRead();
                    leave_queue();
                    return tun2;
                }

                leave_tun();
            }
        }

        mut.UnlockRead();
        if (!Thread::SleepFor(50ms) || CurrentProcess::TerminateReceived()) {
            leave_queue();
            leave_tun();
            return std::unexpected(std::monostate());
        }
        Err(mut.LockRead(Retry::ALWAYS));
    }
};

constexpr auto EnterExitSequence(const GlobalParameters& params,
                                 DroneState& state, BaseState& base,
                                 ProcQueue& queue, RWMutex& queue_mut,
                                 TunnelDir dir, Tunnel& tun1, Tunnel& tun2)
    -> std::expected<bool, std::monostate> {
    auto entered_tun = WaitForTunInQueue(params, state, base, dir, queue,
                                         queue_mut, tun1, tun2);
    if (!entered_tun) {
        base.free_spots.Signal(Retry::ALWAYS);
        state.mutex.Lock();
        state.at_base = false;
        state.mutex.Unlock();
        return std::unexpected(std::monostate());
    }

    GetLogger().Trace("Left the queue");

    if (dir == TunnelDir::OUT) {
        state.mutex.Lock();
        state.docked = false;
        state.mutex.Unlock();
        GetLogger().Info("Left the charging pad");
    }

    auto slept = Thread::SleepFor(params.tunnel_length);

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

    Tunnel tunnel1{.id = 1,
                   .data = &data->tunnel1,
                   .mutex = Mutex(Semaphore::Get(sems, SemIds::TUNNEL1_1))};
    Tunnel tunnel2{.id = 2,
                   .data = &data->tunnel2,
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
            if (!EnterExitSequence(params, state, base, *out_queue, out_mut,
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
            if (!EnterExitSequence(params, state, base, *in_queue, in_mut,
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

        if (params->scenario == TestScenario::PRIORITY_QUEUE ||
            params->scenario == TestScenario::DEAD_BAT_IN_TUNNEL ||
            (params->scenario == TestScenario::TUNNEL_DIR_CHANGE &&
             getpid() % 2 == 0)) {
            std::random_device rdev;
            std::mt19937 gen(rdev());
            std::uniform_int_distribution<int> dist(10, 20);
            state.at_base = false;
            state.docked = false;
            state.bat_level = dist(gen);
            GetLogger().Debug(std::format("Bat: {:>3}%", state.bat_level));
        } else if (params->scenario == TestScenario::SUICIDE_ORDER) {
            state.bat_level = 2;
        }

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
