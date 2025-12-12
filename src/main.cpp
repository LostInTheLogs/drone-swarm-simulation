#include <experimental/scope>
#include <iostream>

#include "systemv_ipc.h"
#include "thread.h"

namespace {
auto HandleExpected(const auto& expected) {
    if (!expected) {
        std::cout << expected.error().what() << '\n';
    }
    return static_cast<bool>(expected);
}
}  // namespace

auto main(int /*argc*/, char* /*argv*/[]) -> int {
    auto msg_queue = IpcMessageQueue::Create(1234, 0666);
    if (!HandleExpected(msg_queue)) {
        return 1;
    }
    std::experimental::scope_exit guard{[&msg_queue] {
        auto removed = msg_queue->Remove();
        HandleExpected(removed);
    }};

    auto sender = Thread::Create([&msg_queue]() {
        auto sent = msg_queue->Send(15, 1);
        HandleExpected(sent);
    });
    if (!HandleExpected(sender)) {
        return 1;
    }

    auto received = msg_queue->Receive<int>(1);
    if (!HandleExpected(received)) {
        return 1;
    }

    std::cout << received.value();

    auto joined = sender->Join();
    if (!HandleExpected(joined)) {
        return 1;
    }
    return 0;
}
