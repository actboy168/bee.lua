#include <bee/sys/file_handle.h>

namespace bee {
    file_handle::file_handle() noexcept
        : h((value_type)-1) {}
    file_handle::operator bool() const noexcept {
        return valid();
    }
    bool file_handle::valid() const noexcept {
        return *this != file_handle {};
    }
    file_handle::value_type file_handle::value() const noexcept {
        return h;
    }
    file_handle::value_type* file_handle::operator&() noexcept {
        return &h;
    }
    bool file_handle::operator==(const file_handle& other) const noexcept {
        return h == other.h;
    }
    bool file_handle::operator!=(const file_handle& other) const noexcept {
        return h != other.h;
    }
}
