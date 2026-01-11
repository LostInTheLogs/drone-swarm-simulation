#include <unistd.h>

#include <cstdio>
#include <format>
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

constexpr void SignalThread(const GlobalParameters& params, DroneState& state,
                            QueueData& in_queue) {
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

        in_queue.can_leave_changed_mut.Lock();
        in_queue.can_leave_changed.Broadcast();
        in_queue.can_leave_changed_mut.Unlock();
        in_queue.mut.Lock();
        in_queue.changed.Broadcast();
        in_queue.mut.Unlock();

        return;
    }
}

constexpr void BroadcastAll(QueueData& queue) {
    queue.mut.Lock();
    queue.changed.Broadcast();
    queue.mut.Unlock();
    queue.can_leave_changed_mut.Lock();
    queue.can_leave_changed.Broadcast();
    queue.can_leave_changed_mut.Unlock();
}

constexpr void BatteryThread(const GlobalParameters& params, DroneState& state,
                             QueueData& in_queue, QueueData& out_queue) {
    auto next = MonotonicClock::now();
    auto dur = 0ms;

    while (true) {
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
            const auto bat_died = state.bat_level <= 0;
            if (bat_died) {
                GetLogger().Warning("Battery died!");
                CurrentProcess::Get().Signal(SIGTERM);
            }
            state.changed.Broadcast();
            state.mutex.Unlock();
            BroadcastAll(in_queue);
            BroadcastAll(out_queue);
            if (bat_died || CurrentProcess::TerminateReceived()) {
                return;
            }
        } else {
            state.mutex.Unlock();
        }
    }
}

void Err(auto&& val) {
    if (!val) {
        throw std::forward<decltype(val)>(val).error();
    }
}

constexpr auto EnterOneTunnel(const GlobalParameters& params, TunnelDir dir,
                              Tunnel& tun) -> bool {
    bool entered = false;
    if (!tun.mutex.Lock()) {
        return false;
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

auto WaitForTunInQueue(const GlobalParameters& params, DroneState& state,
                       BaseState& base, TunnelDir dir, QueueData& queue,
                       Tunnel& tun1, Tunnel& tun2)
    -> std::expected<std::reference_wrapper<Tunnel>, std::monostate> {
    const auto pid = g_curr_process.GetPid();
    int not_pid = 0;

    // add pid to the queue
    queue.mut.Lock();
    const auto prio = (dir == TunnelDir::OUT ? -1 : 100 - state.bat_level);
    GetLogger().Trace(std::format("Added to queue, priority {}", prio));
    queue.queue.Push(pid, prio);
    queue.changed.Broadcast();
    queue.mut.Unlock();

    const auto should_abort = [&]() {
        if (CurrentProcess::TerminateReceived()) {
            return true;
        }
        if (dir == TunnelDir::OUT) {
            return false;
        }

        state.mutex.Lock();
        auto should = state.suicide_order_received;
        state.mutex.Unlock();
        return should;
    };
    const auto leave_queue = [&]() {
        queue.mut.Lock();
        queue.queue.Remove(pid);
        queue.changed.Broadcast();
        queue.mut.Unlock();
        queue.can_leave_changed_mut.Lock();
        queue.can_leave_changed.Broadcast();
        queue.can_leave_changed_mut.Unlock();
    };
    const auto leave_base = [&]() {
        state.mutex.Lock();
        if (state.at_base) {
            base.free_spots.Signal(Retry::ALWAYS);
            state.at_base = false;
        }
        state.mutex.Unlock();
        queue.can_leave_changed_mut.Lock();
        queue.can_leave_changed.Broadcast();
        queue.can_leave_changed_mut.Unlock();
    };

    while (true) {
        // wait until we're first in queue
        queue.mut.Lock();
        while (queue.queue.Peek().value_or(not_pid) != pid) {
            queue.changed.Wait(queue.mut);
            if (should_abort()) {
                queue.mut.Unlock();
                leave_queue();
                return std::unexpected(std::monostate());
            }
        }
        queue.mut.Unlock();
        bool first = true;

        // wait until we can enter the base
        if (dir == TunnelDir::IN) {
            while (true) {
                if (base.free_spots.Wait(Retry::NEVER)) {
                    state.mutex.Lock();
                    state.at_base = true;
                    state.mutex.Unlock();
                    break;
                }
                if (should_abort()) {
                    leave_queue();
                    return std::unexpected(std::monostate());
                }
            }
        }

        // wait until we can enter the tunnel
        queue.can_leave_changed_mut.Lock();
        while (first) {
            if (should_abort()) {
                queue.can_leave_changed_mut.Unlock();
                leave_queue();
                leave_base();
                return std::unexpected(std::monostate());
            }

            // check if we're no longer first
            queue.mut.Lock();
            if (queue.queue.Peek().value_or(not_pid) != pid) {
                queue.mut.Unlock();
                queue.can_leave_changed_mut.Unlock();
                leave_base();
                first = false;
                continue;
            }

            // check tunnel 1
            if (EnterOneTunnel(params, dir, tun1)) {
                queue.queue.Remove(pid);
                queue.changed.Broadcast();
                queue.mut.Unlock();
                queue.can_leave_changed_mut.Unlock();
                return tun1;
            }

            // check tunnel 2
            if (EnterOneTunnel(params, dir, tun2)) {
                queue.queue.Remove(pid);
                queue.changed.Broadcast();
                queue.mut.Unlock();
                queue.can_leave_changed_mut.Unlock();
                return tun2;
            }

            queue.mut.Unlock();
            queue.can_leave_changed.Wait(queue.can_leave_changed_mut);
        }
        if (!first) {
            continue;
        }
    }
};

constexpr auto EnterExitSequence(const GlobalParameters& params,
                                 DroneState& state, BaseState& base,
                                 QueueData& dir_queue, QueueData& in_queue,
                                 TunnelDir dir, Tunnel& tun1, Tunnel& tun2)
    -> std::expected<bool, std::monostate> {
    auto entered_tun =
        WaitForTunInQueue(params, state, base, dir, dir_queue, tun1, tun2);
    if (!entered_tun) {
        state.mutex.Lock();
        if (state.at_base) {
            base.free_spots.Signal(Retry::ALWAYS);
        }
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
    dir_queue.can_leave_changed_mut.Lock();
    dir_queue.can_leave_changed.Broadcast();
    dir_queue.can_leave_changed_mut.Unlock();

    if (!slept) {
        base.free_spots.Signal(Retry::ALWAYS);
        state.mutex.Lock();
        state.at_base = false;
        state.mutex.Unlock();
        if (dir == TunnelDir::OUT) {
            in_queue.can_leave_changed_mut.Lock();
            in_queue.can_leave_changed.Broadcast();
            in_queue.can_leave_changed_mut.Unlock();
        }
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
        in_queue.can_leave_changed_mut.Lock();
        in_queue.can_leave_changed.Broadcast();
        in_queue.can_leave_changed_mut.Unlock();
    }

    return {};
}

constexpr void MainThread(const GlobalParameters& params, DroneState& state) {
    const auto queue_data_size =
        sizeof(ShmProcQueue::value_type) +
        Queue<pid_t>::CalcExtraSize(params.init_drone_count * 2UL);

    auto sems = SemaphoreSet<SemIds>::Get(SemSetKey::MAIN);

    auto in_queue = ShmProcQueue::Get(ShmKey::IN_QUEUE, queue_data_size);
    auto out_queue = ShmProcQueue::Get(ShmKey::OUT_QUEUE, queue_data_size);

    auto data = ShmBaseData::Get(ShmKey::BASE_DATA);

    Tunnel tunnel1{.id = 1,
                   .data = &data->tunnel1,
                   .mutex = Mutex(Semaphore::Get(sems, SemIds::TUNNEL1_1))};
    Tunnel tunnel2{.id = 2,
                   .data = &data->tunnel2,
                   .mutex = Mutex(Semaphore::Get(sems, SemIds::TUNNEL2_1))};

    BaseState base{.free_spots = Semaphore::Get(sems, SemIds::FREE_SPOTS_BASE)};

    auto signal_thread = Thread::Create([&state, &params, &in_queue]() {
        SignalThread(params, state, *in_queue);
    });
    signal_thread.Detach();

    auto battery_thread =
        Thread::Create([&state, &params, &in_queue, &out_queue]() {
            BatteryThread(params, state, *in_queue, *out_queue);
        });

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
            if (!EnterExitSequence(params, state, base, *out_queue, *in_queue,
                                   TunnelDir::OUT, tunnel1, tunnel2)) {
                battery_thread.Join();
                return;
            }
            GetLogger().Info("Left the base");

            state.mutex.Lock();
            continue;
        }
        if (ShouldReturn(params, state)) {
            state.mutex.Unlock();

            GetLogger().Info("Returning to the base");
            if (!EnterExitSequence(params, state, base, *in_queue, *in_queue,
                                   TunnelDir::IN, tunnel2, tunnel1)) {
                if (!CurrentProcess::TerminateReceived()) {
                    state.mutex.Lock();
                    continue;
                }
                battery_thread.Join();
                return;
            }
            GetLogger().Info("Back at the base");

            state.mutex.Lock();
            if (state.charges >= params.max_charges) {
                GetLogger().Warning("Max charging cycles, decomissioning");
                CurrentProcess::Get().Signal(SIGTERM);
                break;
            }
            continue;
        }
    }
    if (state.at_base) {
        base.free_spots.Signal(Retry::ALWAYS);
        state.at_base = false;
        in_queue->can_leave_changed_mut.Lock();
        in_queue->can_leave_changed.Broadcast();
        in_queue->can_leave_changed_mut.Unlock();
    }
    state.mutex.Unlock();
    battery_thread.Join();
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

        MainThread(*params, state);
        GetLogger().Info("Goodbye");

    } catch (std::exception& e) {
        LogPrinter::PrintError("drone", e.what());
        return 1;
    }

    return 0;
}
