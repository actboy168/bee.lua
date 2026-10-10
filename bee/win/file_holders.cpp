#include <Windows.h>

#include <bee/win/file_holders.h>

#include <set>
#include <string>
#include <vector>

namespace bee::win {
    typedef LONG NTSTATUS;

    constexpr NTSTATUS NTSTATUS_SUCCESS             = 0x00000000L;
    constexpr NTSTATUS NTSTATUS_INFO_LENGTH_MISMATCH = 0xC0000004L;
    constexpr NTSTATUS STATUS_ACCESS_DENIED        = 0xC0000022L;

    constexpr ULONG SystemExtendedHandleInformation = 64;
    constexpr ULONG ObjectNameInformation           = 1;
    constexpr ULONG ObjectTypeInformation           = 2;
    constexpr ULONG FilePipeLocalInformation        = 24;

    struct UNICODE_STRING {
        USHORT Length;
        USHORT MaximumLength;
        PWSTR  Buffer;
    };

    struct SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX {
        PVOID     Object;
        ULONG_PTR UniqueProcessId;
        ULONG_PTR HandleValue;
        ULONG     GrantedAccess;
        USHORT    CreatorBackTraceIndex;
        USHORT    ObjectTypeIndex;
        ULONG     HandleAttributes;
        ULONG     Reserved;
    };

    struct SYSTEM_HANDLE_INFORMATION_EX {
        ULONG_PTR                            NumberOfHandles;
        ULONG_PTR                            Reserved;
        SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX    Handles[1];
    };

    struct OBJECT_NAME_INFORMATION {
        UNICODE_STRING Name;
    };

    struct OBJ_TYPE_INFO {
        UNICODE_STRING TypeName;
    };

    struct IO_STATUS_BLOCK {
        union {
            NTSTATUS Status;
            PVOID    Pointer;
        };
        ULONG_PTR Information;
    };

    struct FILE_PIPE_LOCAL_INFORMATION {
        ULONG NamedPipeType;
        ULONG NamedPipeConfiguration;
        ULONG MaximumInstances;
        ULONG CurrentInstances;
        ULONG InboundQuota;
        ULONG ReadDataAvailable;
        ULONG OutboundQuota;
        ULONG WriteQuotaAvailable;
        ULONG NamedPipeState;
        ULONG NamedPipeEnd;
    };

    extern "C" {
    NTSTATUS NTAPI NtQuerySystemInformation(ULONG SystemInformationClass, PVOID SystemInformation, ULONG SystemInformationLength, PULONG ReturnLength);
    NTSTATUS NTAPI NtQueryObject(HANDLE Handle, ULONG ObjectInformationClass, PVOID ObjectInformation, ULONG ObjectInformationLength, PULONG ReturnLength);
    NTSTATUS NTAPI NtQueryInformationFile(HANDLE FileHandle, IO_STATUS_BLOCK* IoStatusBlock, PVOID FileInformation, ULONG Length, ULONG FileInformationClass);
    }

    static bool enable_debug_privilege() noexcept {
        HANDLE token = NULL;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token)) {
            return false;
        }
        TOKEN_PRIVILEGES tp = {};
        if (!LookupPrivilegeValueW(NULL, L"SeDebugPrivilege", &tp.Privileges[0].Luid)) {
            CloseHandle(token);
            return false;
        }
        tp.PrivilegeCount             = 1;
        tp.Privileges[0].Attributes   = SE_PRIVILEGE_ENABLED;
        BOOL ok                       = AdjustTokenPrivileges(token, FALSE, &tp, sizeof(tp), NULL, NULL);
        CloseHandle(token);
        return ok && GetLastError() == ERROR_SUCCESS;
    }

    static std::wstring normalize_filepath(std::wstring_view path) {
        DWORD len = GetFullPathNameW(path.data(), 0, NULL, NULL);
        if (len == 0) return {};
        std::wstring result(len - 1, L'\0');
        GetFullPathNameW(path.data(), len, result.data(), NULL);
        return result;
    }

    static bool is_device_handle(HANDLE dup_handle) noexcept {
        HANDLE mapping = CreateFileMappingW(dup_handle, NULL, PAGE_READONLY, 0, 1, NULL);
        if (mapping) {
            CloseHandle(mapping);
            return false;
        }
        return GetLastError() == ERROR_BAD_EXE_FORMAT;
    }

    enum class handle_kind { other, file, section };

    static handle_kind get_handle_kind(HANDLE dup_handle) noexcept {
        std::vector<uint8_t> buffer(256);
        ULONG return_length = 0;
        NTSTATUS status;
        for (int attempt = 0; attempt < 4; ++attempt) {
            status = NtQueryObject(dup_handle, ObjectTypeInformation, buffer.data(), (ULONG)buffer.size(), &return_length);
            if (status == NTSTATUS_SUCCESS) break;
            if (status == NTSTATUS_INFO_LENGTH_MISMATCH) {
                buffer.resize(buffer.size() * 2);
                continue;
            }
            return handle_kind::other;
        }
        if (status != NTSTATUS_SUCCESS) return handle_kind::other;

        auto& info = *reinterpret_cast<OBJ_TYPE_INFO*>(buffer.data());
        if (info.TypeName.Length == 0 || !info.TypeName.Buffer) return handle_kind::other;
        std::wstring_view type(info.TypeName.Buffer, info.TypeName.Length / sizeof(WCHAR));
        if (type == L"File") return handle_kind::file;
        if (type == L"Section") return handle_kind::section;
        return handle_kind::other;
    }

    static std::wstring nt_to_dos_path(std::wstring_view nt_path) {
        WCHAR drives[256];
        if (!GetLogicalDriveStringsW(255, drives)) return {};

        std::wstring best_match;
        size_t best_len = 0;
        for (const WCHAR* drive = drives; *drive; drive += wcslen(drive) + 1) {
            WCHAR drive_letter[3] = { drive[0], L':', L'\0' };
            WCHAR device_name[MAX_PATH];
            if (!QueryDosDeviceW(drive_letter, device_name, MAX_PATH)) continue;

            size_t device_len = wcslen(device_name);
            if (device_len == 0) continue;

            if (nt_path.size() >= device_len && _wcsnicmp(nt_path.data(), device_name, device_len) == 0) {
                std::wstring dos_path(1, drive[0]);
                dos_path += L':';
                dos_path.append(nt_path.data() + device_len);
                if (best_match.empty() || device_len > best_len) {
                    best_match = std::move(dos_path);
                    best_len   = device_len;
                }
            }
        }

        // If no drive letter match, try stripping the \??\ prefix.
        if (best_match.empty() && nt_path.size() > 4 && _wcsnicmp(nt_path.data(), L"\\??\\", 4) == 0) {
            best_match = nt_path.substr(4);
        }

        if (best_match.empty()) {
            best_match = nt_path;
        }
        return best_match;
    }

    static bool path_contains(std::wstring_view dir, std::wstring_view file) {
        if (file.size() < dir.size()) return false;
        if (_wcsnicmp(file.data(), dir.data(), dir.size()) != 0) return false;
        if (file.size() == dir.size()) return true;
        return file[dir.size()] == L'\\';
    }

    std::set<uint32_t> find_file_holders(std::wstring_view filepath) {
        std::set<uint32_t> result;

        enable_debug_privilege();

        std::wstring target_path = normalize_filepath(filepath);
        if (target_path.empty()) return result;

        // Query system handle table with growing buffer
        std::vector<uint8_t> buffer;
        ULONG buffer_size = 0x100000; // 1 MB initial
        NTSTATUS status;
        for (int attempt = 0; attempt < 8; ++attempt) {
            buffer.resize(buffer_size);
            ULONG return_length = 0;
            status = NtQuerySystemInformation(SystemExtendedHandleInformation, buffer.data(), buffer_size, &return_length);
            if (status == NTSTATUS_SUCCESS) break;
            if (status == NTSTATUS_INFO_LENGTH_MISMATCH) {
                buffer_size = return_length + 0x10000;
                continue;
            }
            return result;
        }
        if (status != NTSTATUS_SUCCESS) return result;

        auto& info = *reinterpret_cast<SYSTEM_HANDLE_INFORMATION_EX*>(buffer.data());

        for (ULONG_PTR i = 0; i < info.NumberOfHandles; ++i) {
            auto& entry  = info.Handles[i];
            uint32_t pid = static_cast<uint32_t>(entry.UniqueProcessId);

            if (pid == 0 || pid == 4) continue;
            if (result.count(pid)) continue;

            HANDLE hProcess = OpenProcess(PROCESS_DUP_HANDLE, FALSE, pid);
            if (!hProcess) continue;

            HANDLE dup_handle = NULL;
            BOOL dup_ok       = DuplicateHandle(hProcess, (HANDLE)entry.HandleValue,
                                                GetCurrentProcess(), &dup_handle,
                                                0, FALSE, 0x00000002 /* DUPLICATE_SAME_ACCESS */);
            CloseHandle(hProcess);
            if (!dup_ok || !dup_handle) continue;

            if (is_device_handle(dup_handle)) {
                CloseHandle(dup_handle);
                continue;
            }

            handle_kind kind = get_handle_kind(dup_handle);
            if (kind == handle_kind::other) {
                CloseHandle(dup_handle);
                continue;
            }

            // Only regular files can be named pipes, so only they need the pipe
            // filter below. A Section is a memory-mapped file and never blocks.
            if (kind == handle_kind::file) {
                // Querying a named pipe's name (ObjectNameInformation) blocks
                // indefinitely when the pipe server holds the pipe lock, so we
                // skip pipes up front. FilePipeLocalInformation is non-blocking:
                //   STATUS_SUCCESS       -> named pipe (skip)
                //   STATUS_ACCESS_DENIED -> ambiguous: pipe we can't read, or a
                //                           file with restricted access; skip it
                //                           to dodge the blocking pipes.
                // TODO: ACCESS_DENIED also skips ~14% of File handles that are
                //       real files with restricted access; revisit if needed.
                IO_STATUS_BLOCK iosb = {};
                FILE_PIPE_LOCAL_INFORMATION pipe_info = {};
                NTSTATUS pipe_status = NtQueryInformationFile(dup_handle, &iosb, &pipe_info, sizeof(pipe_info), FilePipeLocalInformation);
                if (pipe_status == NTSTATUS_SUCCESS || pipe_status == STATUS_ACCESS_DENIED) {
                    CloseHandle(dup_handle);
                    continue;
                }
            }

            std::vector<uint8_t> name_buffer(512);
            ULONG name_return_length = 0;
            NTSTATUS name_status;
            for (int attempt = 0; attempt < 4; ++attempt) {
                name_status = NtQueryObject(dup_handle, ObjectNameInformation, name_buffer.data(), (ULONG)name_buffer.size(), &name_return_length);
                if (name_status == NTSTATUS_SUCCESS) break;
                if (name_status == NTSTATUS_INFO_LENGTH_MISMATCH) {
                    name_buffer.resize(name_buffer.size() * 2);
                    continue;
                }
                break;
            }
            CloseHandle(dup_handle);

            if (name_status != NTSTATUS_SUCCESS) continue;

            auto& name_info = *reinterpret_cast<OBJECT_NAME_INFORMATION*>(name_buffer.data());
            if (name_info.Name.Length == 0 || !name_info.Name.Buffer) continue;

            std::wstring_view nt_path(name_info.Name.Buffer, name_info.Name.Length / sizeof(WCHAR));
            std::wstring dos_path = nt_to_dos_path(nt_path);

            if (path_contains(target_path, dos_path) || path_contains(dos_path, target_path)) {
                result.insert(pid);
            }
        }

        return result;
    }
}
