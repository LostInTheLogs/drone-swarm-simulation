#include <algorithm>
#include <cstdlib>
#include <format>

#include "clock.h"
#include "logger.h"
#include "thread.h"

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

}  // namespace

auto main(int /*argc*/, char* /*argv*/[]) -> int {
    ThreadMutex battery_mut;
    int bat_level = 50;
    bool bat_charging = false;

    GetLogger().Debug("Hello world");

    auto battery_th = Thread::Create([&]() {
        using namespace std::chrono_literals;

        auto next = MonotonicClock::now();
        auto dur = 200ms;

        while (true) {
            next += dur;
            auto slept = Thread::SleepUntil(next);
            if (!slept) {
                GetLogger().Info("sleep interruped");
                CurrentProcess::Get().Signal(SIGTERM).value();
                return;
            }

            battery_mut.Lock();
            if (bat_charging) {
                bat_level++;
            } else {
                bat_level--;
            }
            bat_level = std::clamp(bat_level, 0, 100);
            if (bat_level % 10 == 0) {
                GetLogger().Info(std::format("Bat: {:>3}%", bat_level));
            }
            battery_mut.Unlock();

            if (bat_level <= 0) {
                GetLogger().Warning("Battery died!");
                CurrentProcess::Get().Signal(SIGTERM).value();
                return;
            }
        }
    });

    if (!HandleExpectedError(battery_th)) {
        return 1;
    }
    if (!HandleExpectedError(battery_th->Join())) {
        return 1;
    }

    return 0;
}
