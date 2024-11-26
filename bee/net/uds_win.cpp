//  clang-format off
#include <winsock2.h>
//  clang-format on
#include <bee/net/endpoint.h>
#include <bee/net/socket.h>
#include <bee/net/uds_win.h>
#include <bee/nonstd/charconv.h>
#include <bee/nonstd/unreachable.h>
#include <bee/utility/dynarray.h>
#include <bee/win/wtf8.h>
#include <mstcpip.h>
#include <mswsock.h>

#include <array>
#include <limits>

#if defined(__MINGW32__)
#    define WSA_FLAG_NO_HANDLE_INHERIT 0x80
#endif

namespace bee::net::socket {

    namespace fileutil {
        static FILE* open(zstring_view filename, const wchar_t* mode) noexcept {
#if defined(_MSC_VER)
#    pragma warning(push)
#    pragma warning(disable : 4996)
#endif
            return _wfopen(wtf8::u2w(filename).c_str(), mode);
#if defined(_MSC_VER)
#    pragma warning(pop)
#endif
        }
        static size_t size(FILE* f) noexcept {
            _fseeki64(f, 0, SEEK_END);
            long long size = _ftelli64(f);
            _fseeki64(f, 0, SEEK_SET);
            return (size_t)size;
        }
        static size_t read(FILE* f, void* buf, size_t sz) noexcept {
            return fread(buf, sizeof(char), sz, f);
        }
        static size_t write(FILE* f, const void* buf, size_t sz) noexcept {
            return fwrite(buf, sizeof(char), sz, f);
        }
        static void close(FILE* f) noexcept {
            fclose(f);
        }
    }

    static std::string file_read(zstring_view filename) noexcept {
        FILE* f = fileutil::open(filename, L"rb");
        if (!f) {
            return std::string();
        }
        std::string result(fileutil::size(f), '\0');
        fileutil::read(f, result.data(), result.size());
        fileutil::close(f);
        return result;
    }

    static bool file_write(zstring_view filename, const std::string& value) noexcept {
        FILE* f = fileutil::open(filename, L"wb");
        if (!f) {
            return false;
        }
        fileutil::write(f, value.data(), value.size());
        fileutil::close(f);
        return true;
    }

    static bool read_tcp_port(const endpoint& ep, uint16_t& tcpport) noexcept {
        auto [type, path] = ep.get_unix();
        if (type != un_format::pathname) {
            return false;
        }
        auto unixpath = file_read(path);
        if (unixpath.empty()) {
            return false;
        }
        if (auto [p, ec] = std::from_chars(unixpath.data(), unixpath.data() + unixpath.size(), tcpport); ec != std::errc()) {
            return false;
        }
        if (tcpport <= 0 || tcpport > (std::numeric_limits<uint16_t>::max)()) {
            return false;
        }
        return true;
    }

    static bool write_tcp_port(zstring_view path, fd_t s) noexcept {
        endpoint ep;
        if (socket::getsockname(s, ep)) {
            auto tcpport = ep.get_port();
            std::array<char, 10> portstr;
            if (auto [p, ec] = std::to_chars(portstr.data(), portstr.data() + portstr.size() - 1, tcpport); ec != std::errc()) {
                return false;
            } else {
                p[0] = '\0';
            }
            return file_write(path, portstr.data());
        }
        return false;
    }

    status u_connect(fd_t s, const endpoint& ep) noexcept {
        uint16_t tcpport = 0;
        if (!read_tcp_port(ep, tcpport)) {
            ::WSASetLastError(WSAECONNREFUSED);
            return status::failed;
        }
        return socket::connect(s, endpoint::from_localhost(tcpport));
    }

    bool u_bind(fd_t s, const endpoint& ep) {
        const bool ok = socket::bind(s, endpoint::from_localhost(0));
        if (!ok) {
            return ok;
        }
        auto [type, path] = ep.get_unix();
        if (type != un_format::pathname) {
            ::WSASetLastError(WSAENETDOWN);
            return false;
        }
        if (!write_tcp_port(path, s)) {
            ::WSASetLastError(WSAENETDOWN);
            return false;
        }
        return true;
    }

    static WSAPROTOCOL_INFOW UnixProtocol;
    bool u_support() noexcept {
        static GUID AF_UNIX_PROVIDER_ID = { 0xA00943D9, 0x9C2E, 0x4633, { 0x9B, 0x59, 0x00, 0x57, 0xA3, 0x16, 0x09, 0x94 } };
        DWORD len                       = 0;
        ::WSAEnumProtocolsW(0, NULL, &len);
        dynarray<std::byte> buf(len);
        LPWSAPROTOCOL_INFOW protocols = (LPWSAPROTOCOL_INFOW)buf.data();
        const int n                   = ::WSAEnumProtocolsW(0, protocols, &len);
        if (n == SOCKET_ERROR) {
            return false;
        }
        for (int i = 0; i < n; ++i) {
            if (protocols[i].iAddressFamily == AF_UNIX && IsEqualGUID(protocols[i].ProviderId, AF_UNIX_PROVIDER_ID)) {
                const fd_t fd = ::WSASocketW(PF_UNIX, SOCK_STREAM, 0, &protocols[i], 0, WSA_FLAG_NO_HANDLE_INHERIT);
                if (fd == retired_fd) {
                    return false;
                }
                ::closesocket(fd);
                UnixProtocol = protocols[i];
                return true;
            }
        }
        return false;
    }

    fd_t u_createSocket(int af, int type, int protocol, fd_flags fd_flags) noexcept {
        return ::WSASocketW(af, type, protocol, af == PF_UNIX ? &UnixProtocol : NULL, 0, WSA_FLAG_NO_HANDLE_INHERIT);
    }
}
