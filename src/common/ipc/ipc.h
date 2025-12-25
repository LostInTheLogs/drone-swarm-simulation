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
enum class ShmKey : key_t { PARAMS = g_rand_key, IN_QUEUE, OUT_QUEUE };

// NOLINTNEXTLINE(performance-enum-size)
enum class SemIds : int {
    IN_QUEUE_A0,
    IN_QUEUE_B1,
    IN_QUEUE_C1,
    OUT_QUEUE_A0,
    OUT_QUEUE_B1,
    OUT_QUEUE_C1,
    COUNT,
};

namespace detail {

template <typename E>
constexpr auto InitPairsValid(
    const std::array<std::pair<E, int>, static_cast<size_t>(E::COUNT)>& init)
    -> bool {
    auto len = static_cast<size_t>(E::COUNT);
    for (size_t i = 0; i < len; ++i) {
        for (size_t j = i + 1; j < len; ++j) {
            if (init[i].first == init[j].first) {  // NOLINT
                return false;
            }
        }
    }
    return true;
}

template <typename E>
constexpr auto InitPairsToArray(
    const std::array<std::pair<E, int>, static_cast<size_t>(E::COUNT)>& init)
    -> std::span<const unsigned short> {
    static std::array<unsigned short, static_cast<size_t>(E::COUNT)> data{};
    for (const auto& [id, value] : init) {
        data.at(static_cast<size_t>(id)) = value;
    }
    return {data.begin(), data.end()};
}
}  // namespace detail

template <typename E>
class SemInit {
  public:
    constexpr auto Get() -> std::span<const unsigned short> = delete;
};

template <>
class SemInit<SemIds> {
  public:
    static constexpr auto init_ = std::to_array<std::pair<SemIds, int>>({
        {SemIds::IN_QUEUE_A0, 0},   //
        {SemIds::IN_QUEUE_B1, 1},   //
        {SemIds::IN_QUEUE_C1, 1},   //
        {SemIds::OUT_QUEUE_A0, 0},  //
        {SemIds::OUT_QUEUE_B1, 1},  //
        {SemIds::OUT_QUEUE_C1, 1},  //
    });

    static constexpr auto Get() -> std::span<const unsigned short> {
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
