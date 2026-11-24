#ifndef CAPSID_CGROUP_IO_H
#define CAPSID_CGROUP_IO_H

#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <string>
#include <unistd.h>

namespace capsid {

inline bool cgroup_read_at(int directory, const char* name, std::string* value) {
    const int descriptor = openat(directory, name, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (descriptor < 0) {
        return false;
    }
    char buffer[256];
    size_t size = 0;
    while (size < sizeof(buffer)) {
        const ssize_t count = read(descriptor, buffer + size, sizeof(buffer) - size);
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count < 0) {
            close(descriptor);
            return false;
        }
        if (count == 0) {
            break;
        }
        size += static_cast<size_t>(count);
    }
    close(descriptor);
    if (size == sizeof(buffer)) {
        errno = EOVERFLOW;
        return false;
    }
    size_t begin = 0;
    while (begin < size && (buffer[begin] == ' ' || buffer[begin] == '\t' ||
                            buffer[begin] == '\r' || buffer[begin] == '\n')) {
        ++begin;
    }
    while (size > begin && (buffer[size - 1] == ' ' || buffer[size - 1] == '\t' ||
                            buffer[size - 1] == '\r' || buffer[size - 1] == '\n')) {
        --size;
    }
    value->assign(buffer + begin, buffer + size);
    return true;
}

inline bool cgroup_write_at(int directory, const char* name, const std::string& value) {
    const int descriptor = openat(directory, name, O_WRONLY | O_CLOEXEC | O_NOFOLLOW);
    if (descriptor < 0) {
        return false;
    }
    ssize_t count;
    do {
        count = write(descriptor, value.data(), value.size());
    } while (count < 0 && errno == EINTR);
    const int write_error = errno;
    close(descriptor);
    if (count != static_cast<ssize_t>(value.size())) {
        errno = count < 0 ? write_error : EIO;
        return false;
    }
    return true;
}

inline bool cgroup_value_matches(const std::string& name, const std::string& desired,
                                 const std::string& actual) {
    if (actual == desired) {
        return true;
    }
    if (name != "memory.max" && name != "memory.high" && name != "memory.swap.max") {
        return false;
    }
    if (desired == "max" || actual == "max" || desired.empty() || actual.empty()) {
        return false;
    }
    char* desired_end = nullptr;
    char* actual_end = nullptr;
    errno = 0;
    const unsigned long long desired_val = std::strtoull(desired.c_str(), &desired_end, 10);
    if (errno != 0 || desired_end == desired.c_str() || *desired_end != '\0') {
        return false;
    }
    const unsigned long long actual_val = std::strtoull(actual.c_str(), &actual_end, 10);
    if (errno != 0 || actual_end == actual.c_str() || *actual_end != '\0') {
        return false;
    }
    if (actual_val == 0 || actual_val > desired_val) {
        return false;
    }
    const long page_size = sysconf(_SC_PAGESIZE);
    const unsigned long long page_bytes =
        page_size > 0 ? static_cast<unsigned long long>(page_size) : 4096ULL;
    return (desired_val - actual_val) < page_bytes;
}

} // namespace capsid

#endif
