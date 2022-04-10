#include <bee/utility/file_handle.h>

namespace bee {
    file_handle::file_handle()
        : h((value_type)-1)
    { }
    file_handle::file_handle(value_type v)
        : h(v)
    { }
    file_handle::operator bool() const {
        return valid();
    }
    bool file_handle::valid() const {
        return *this != file_handle{};
    }
    file_handle::value_type file_handle::value() const {
        return h;
    }
    file_handle::value_type* file_handle::operator &() {
        return &h;
    }
    bool file_handle::operator ==(const file_handle& other) const {
        return h == other.h;
    }
    bool file_handle::operator !=(const file_handle& other) const {
        return h != other.h;
    }
}
