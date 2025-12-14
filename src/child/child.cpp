#include <experimental/scope>
#include <iostream>

namespace {
auto HandleExpected(const auto& expected) {
    if (!expected) {
        std::cout << expected.error().what() << '\n';
    }
    return static_cast<bool>(expected);
}
}  // namespace

auto main(int /*argc*/, char* /*argv*/[]) -> int {
    // auto msg_queue = IpcMessageQueue::Get(1234);
    // if (!HandleExpected(msg_queue)) {
    //     return 1;
    // }
    //
    // auto sent = msg_queue->Send(51, 1);
    // if (!HandleExpected(sent)) {
    //     return 1;
    // }

    return 0;
}
