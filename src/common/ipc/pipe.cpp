#include "pipe.h"

#include <fcntl.h>
#include <unistd.h>

#include "logger.h"

PipeReader::PipeReader(int file_descriptor, bool blocking)
    : fd_(file_descriptor) {
    if (!blocking) {
        fcntl(fd_, F_SETFL, O_NONBLOCK);
    }
}

PipeReader::PipeReader(PipeReader &&other) noexcept : fd_(other.fd_) {
    other.fd_ = -1;
}
auto PipeReader::operator=(PipeReader &&other) noexcept -> PipeReader & {
    if (&other == this) {
        return *this;
    }
    fd_ = other.fd_;
    other.fd_ = -1;
    return *this;
}

PipeReader::~PipeReader() {
    if (fd_ == -1) {
        return;
    }
    if (close(fd_) == -1) {
        LogPrinter::PrintError("PipeReader", "Couldn't close pipe");
    }
}

PipeWriter::PipeWriter(int file_descriptor, bool blocking)
    : fd_(file_descriptor) {
    if (!blocking) {
        fcntl(fd_, F_SETFL, O_NONBLOCK);
    }
}
PipeWriter::PipeWriter(PipeWriter &&other) noexcept : fd_(other.fd_) {
    other.fd_ = -1;
}
auto PipeWriter::operator=(PipeWriter &&other) noexcept -> PipeWriter & {
    if (&other == this) {
        return *this;
    }
    fd_ = other.fd_;
    other.fd_ = -1;
    return *this;
}

PipeWriter::~PipeWriter() {
    if (fd_ == -1) {
        return;
    }
    if (close(fd_) == -1) {
        LogPrinter::PrintError("PipeWriter", "Couldn't close pipe");
    }
}
