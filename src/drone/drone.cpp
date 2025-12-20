#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <format>

#include "clock.h"
#include "logger.h"
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

inline auto GetLogger() -> Logger& {
    static auto g_logger = Logger::Create("drone");
    if (!HandleExpectedError(g_logger)) {
        _Exit(1);
    }
    return *g_logger;
}

constexpr auto g_ignore_suicide_bat_thr = 20;
constexpr auto g_low_bat_thr = 20;
constexpr auto g_max_charges = 2;

}  // namespace

auto main(int /*argc*/, char* /*argv*/[]) -> int {
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGUSR1);
    pthread_sigmask(SIG_BLOCK, &sigset, nullptr);

    ThreadMutex state_mut;
    ThreadCond state_changed;
    int bat_level = 50;
    int charges = 0;
    bool docked = false;

    bool suicide_order_received = false;

    GetLogger().Debug("Hello world");

    const auto should_return = [&]() {
        return !docked && bat_level < g_low_bat_thr && !suicide_order_received;
    };
    const auto should_leave = [&]() {
        return docked && (bat_level == 100 || suicide_order_received);
    };

    const auto signal_thread = Thread::Create([&]() {
        while (true) {
            int sig{};
            sigwait(&sigset, &sig);

            state_mut.Lock();
            if (bat_level < g_ignore_suicide_bat_thr) {
                GetLogger().Info("Suicide mission order ignored");
                state_mut.Unlock();
                continue;
            }
            suicide_order_received = true;
            GetLogger().Info("Suicide mission order accepted");
            state_changed.Broadcast();
            state_mut.Unlock();
        }
    });
    if (!signal_thread) {
        return 1;
    }

    const auto battery_thread = Thread::Create([&]() {
        auto next = MonotonicClock::now();
        const auto dur = 50ms;

        while (!CurrentProcess::TerminateReceived()) {
            next += dur;
            auto slept = Thread::SleepUntil(next);
            if (!slept) {
                GetLogger().Info("Sleep interruped");
                CurrentProcess::Get().Signal(SIGTERM).value();
                return;
            }

            state_mut.Lock();
            if (docked) {
                bat_level++;
            } else {
                bat_level--;
            }
            auto clamped = std::clamp(bat_level, 0, 100);
            if (bat_level == clamped && clamped % 10 == 0) {
                GetLogger().Info(std::format("Bat: {:>3}%", clamped));
            }
            bat_level = clamped;

            if (bat_level <= 0) {
                GetLogger().Warning("Battery died!");
                CurrentProcess::Get().Signal(SIGTERM).value();
                state_changed.Broadcast();
                state_mut.Unlock();
                return;
            }

            if (should_return() || should_leave() ||
                CurrentProcess::TerminateReceived()) {
                state_changed.Broadcast();
            }

            state_mut.Unlock();
        }
    });
    if (!HandleExpectedError(battery_thread)) {
        return 1;
    }

    state_mut.Lock();
    while (bat_level > 0 && !CurrentProcess::TerminateReceived()) {
        state_changed.Wait(state_mut);
        if (bat_level <= 0 || CurrentProcess::TerminateReceived()) {
            break;
        }

        if (should_leave()) {
            charges++;
            state_mut.Unlock();

            GetLogger().Info("Leaving the base");
            if (!Thread::SleepFor(500ms)) {  // TODO: leave base
                break;
            }

            state_mut.Lock();
            docked = false;
            GetLogger().Info("Left the base");
            continue;
        }
        if (should_return()) {
            state_mut.Unlock();

            GetLogger().Info("Returning to the base");
            if (!Thread::SleepFor(500ms)) {  // TODO: go to base
                break;
            }

            state_mut.Lock();
            GetLogger().Info("Back at the base");
            if (charges == g_max_charges) {
                GetLogger().Info("Max charging cycles, decomissioning");
                CurrentProcess::Get().Signal(SIGTERM).value();
                break;
            }
            docked = true;
            continue;
        }
    }
    state_mut.Unlock();

    if (!HandleExpectedError(battery_thread->Join())) {
        return 1;
    }
    GetLogger().Info("Goodbye");

    return 0;
}
