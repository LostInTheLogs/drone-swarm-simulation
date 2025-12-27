#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <system_error>

constexpr key_t g_rand_key = 33889;

// NOLINTNEXTLINE(performance-enum-size)
enum class MsgQueueKey : key_t { MAIN = g_rand_key };

// NOLINTNEXTLINE(performance-enum-size)
enum class MessageTypeId : long { LOGGER = 1 };

// NOLINTNEXTLINE(performance-enum-size)
enum class SemSetKey : key_t { MAIN = g_rand_key };

// NOLINTNEXTLINE(performance-enum-size)
enum class ShmKey : key_t { PARAMS = g_rand_key, QUEUE1, QUEUE2 };

// NOLINTNEXTLINE(performance-enum-size)
enum class SemIds : int {
    QUEUE1_A0,
    QUEUE1_B1,
    QUEUE1_C1,
    QUEUE2_A0,
    QUEUE2_B1,
    QUEUE2_C1,
    COUNT,
};

namespace detail {

template <typename E>
constexpr auto InitPairsValid(
    const std::array<std::pair<E, int>, static_cast<size_t>(E::COUNT)>& init)
    -> bool {
    auto len = static_cast<size_t>(E::COUNT);
    for (size_t i = 0; i < len; ++i) {
        if (i != static_cast<size_t>(init[i].first)) {  // NOLINT
            return false;
        }
    }
    return true;
}

template <typename E>
constexpr auto InitPairsToArray(
    const std::array<std::pair<E, int>, static_cast<size_t>(E::COUNT)>& init)
    -> std::array<unsigned short, static_cast<size_t>(E::COUNT)> {
    std::array<unsigned short, static_cast<size_t>(E::COUNT)> data{};
    for (const auto& [id, value] : init) {
        data.at(static_cast<size_t>(id)) = value;
    }
    return data;
}
}  // namespace detail

template <typename E>
class SemInit {
  public:
    constexpr auto Get() -> std::array<const unsigned short, E::COUNT> = delete;
};

template <>
class SemInit<SemIds> {
  public:
    static constexpr auto init_ = std::to_array<std::pair<SemIds, int>>({
        {SemIds::QUEUE1_A0, 0},  //
        {SemIds::QUEUE1_B1, 1},  //
        {SemIds::QUEUE1_C1, 1},  //
        {SemIds::QUEUE2_A0, 0},  //
        {SemIds::QUEUE2_B1, 1},  //
        {SemIds::QUEUE2_C1, 1},  //
    });

    static constexpr auto Get() -> auto {
        static_assert(detail::InitPairsValid(init_));
        return detail::InitPairsToArray<SemIds>(init_);
    }
};

enum class IpcType : uint8_t { MESSAGE_QUEUE, SEMAPHORE_SET, SHARED_MEMORY };

class IpcError : public std::system_error {
  public:
    explicit IpcError(IpcType ipc_type, key_t key, int ipc_id, int error);

  private:
    static auto IpcTypeToStr(IpcType ipc_type) -> const char*;
};
