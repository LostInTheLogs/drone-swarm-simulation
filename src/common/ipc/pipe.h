#pragma once

#include <expected>
#include <system_error>

class PipeReader {
  public:
    explicit PipeReader(int file_descriptor, bool blocking = true);
    PipeReader(PipeReader &&) noexcept;
    PipeReader(const PipeReader &) = delete;
    auto operator=(PipeReader &&) noexcept -> PipeReader &;
    auto operator=(const PipeReader &) -> PipeReader & = delete;
    ~PipeReader();

    template <typename T>
    auto Read() -> std::expected<T, std::system_error> {
        T data;
        auto success = read(fd_, &data, sizeof(data));
        if (success == -1) {
            return std::unexpected(
                std::system_error(errno, std::generic_category()));
        }
        return data;
    }

  private:
    int fd_;
};

class PipeWriter {
  public:
    explicit PipeWriter(int file_descriptor, bool blocking = true);
    PipeWriter(PipeWriter &&) noexcept;
    PipeWriter(const PipeWriter &) = delete;
    auto operator=(PipeWriter &&) noexcept -> PipeWriter &;
    auto operator=(const PipeWriter &) -> PipeWriter & = delete;
    ~PipeWriter();

    template <typename T>
    auto Write(T data) -> std::expected<void, std::system_error> {
        auto success = write(fd_, &data, sizeof(data));
        if (success == -1) {
            return std::unexpected(
                std::system_error(errno, std::generic_category()));
        }
        return {};
    }

  private:
    int fd_;
};
