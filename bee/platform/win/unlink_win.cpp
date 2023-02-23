#include <bee/platform/win/unlink.h>
#include <windows.h>

enum {
    FILE_DISPOSITION_DO_NOT_DELETE = 0x00,
    FILE_DISPOSITION_DELETE = 0x01,
    FILE_DISPOSITION_POSIX_SEMANTICS = 0x02,
    FILE_DISPOSITION_FORCE_IMAGE_SECTION_CHECK = 0x04,
    FILE_DISPOSITION_ON_CLOSE = 0x08,
    FILE_DISPOSITION_IGNORE_READONLY_ATTRIBUTE = 0x10,
};

enum FILE_INFORMATION_CLASS {
    FileDirectoryInformation = 1,                 //  1
    FileFullDirectoryInformation,                 //  2
    FileBothDirectoryInformation,                 //  3
    FileBasicInformation,                         //  4
    FileStandardInformation,                      //  5
    FileInternalInformation,                      //  6
    FileEaInformation,                            //  7
    FileAccessInformation,                        //  8
    FileNameInformation,                          //  9
    FileRenameInformation,                        // 10
    FileLinkInformation,                          // 11
    FileNamesInformation,                         // 12
    FileDispositionInformation,                   // 13
    FilePositionInformation,                      // 14
    FileFullEaInformation,                        // 15
    FileModeInformation,                          // 16
    FileAlignmentInformation,                     // 17
    FileAllInformation,                           // 18
    FileAllocationInformation,                    // 19
    FileEndOfFileInformation,                     // 20
    FileAlternateNameInformation,                 // 21
    FileStreamInformation,                        // 22
    FilePipeInformation,                          // 23
    FilePipeLocalInformation,                     // 24
    FilePipeRemoteInformation,                    // 25
    FileMailslotQueryInformation,                 // 26
    FileMailslotSetInformation,                   // 27
    FileCompressionInformation,                   // 28
    FileObjectIdInformation,                      // 29
    FileCompletionInformation,                    // 30
    FileMoveClusterInformation,                   // 31
    FileQuotaInformation,                         // 32
    FileReparsePointInformation,                  // 33
    FileNetworkOpenInformation,                   // 34
    FileAttributeTagInformation,                  // 35
    FileTrackingInformation,                      // 36
    FileIdBothDirectoryInformation,               // 37
    FileIdFullDirectoryInformation,               // 38
    FileValidDataLengthInformation,               // 39
    FileShortNameInformation,                     // 40
    FileIoCompletionNotificationInformation,      // 41
    FileIoStatusBlockRangeInformation,            // 42
    FileIoPriorityHintInformation,                // 43
    FileSfioReserveInformation,                   // 44
    FileSfioVolumeInformation,                    // 45
    FileHardLinkInformation,                      // 46
    FileProcessIdsUsingFileInformation,           // 47
    FileNormalizedNameInformation,                // 48
    FileNetworkPhysicalNameInformation,           // 49
    FileIdGlobalTxDirectoryInformation,           // 50
    FileIsRemoteDeviceInformation,                // 51
    FileUnusedInformation,                        // 52
    FileNumaNodeInformation,                      // 53
    FileStandardLinkInformation,                  // 54
    FileRemoteProtocolInformation,                // 55
    FileRenameInformationBypassAccessCheck,       // 56
    FileLinkInformationBypassAccessCheck,         // 57
    FileVolumeNameInformation,                    // 58
    FileIdInformation,                            // 59
    FileIdExtdDirectoryInformation,               // 60
    FileReplaceCompletionInformation,             // 61
    FileHardLinkFullIdInformation,                // 62
    FileIdExtdBothDirectoryInformation,           // 63
    FileDispositionInformationEx,                 // 64
    FileRenameInformationEx,                      // 65
    FileRenameInformationExBypassAccessCheck,     // 66
    FileDesiredStorageClassInformation,           // 67
    FileStatInformation,                          // 68
    FileMemoryPartitionInformation,               // 69
    FileStatLxInformation,                        // 70
    FileCaseSensitiveInformation,                 // 71
    FileLinkInformationEx,                        // 72
    FileLinkInformationExBypassAccessCheck,       // 73
    FileStorageReserveIdInformation,              // 74
    FileCaseSensitiveInformationForceAccessCheck, // 75
    FileMaximumInformation
};

struct FILE_DISPOSITION_INFORMATION_EX {
    ULONG Flags;
};

struct IO_STATUS_BLOCK {
    union {
        NTSTATUS Status;
        PVOID    Pointer;
    };
    ULONG_PTR Information;
};

struct UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR  Buffer;
};

struct OBJECT_ATTRIBUTES {
    ULONG           Length;
    HANDLE          RootDirectory;
    UNICODE_STRING* ObjectName;
    ULONG           Attributes;
    PVOID           SecurityDescriptor;
    PVOID           SecurityQualityOfService;
};

extern "C" {
NTSTATUS __stdcall NtOpenFile(PHANDLE, ACCESS_MASK, OBJECT_ATTRIBUTES*, IO_STATUS_BLOCK*, ULONG, ULONG);
NTSTATUS __stdcall NtSetInformationFile(HANDLE, IO_STATUS_BLOCK*, PVOID, ULONG, FILE_INFORMATION_CLASS);
NTSTATUS __stdcall NtClose(HANDLE);
ULONG    __stdcall RtlNtStatusToDosError(NTSTATUS);
BOOLEAN  __stdcall RtlDosPathNameToNtPathName_U(PCWSTR, UNICODE_STRING*, PWSTR*, PVOID);
VOID     __stdcall RtlFreeUnicodeString(UNICODE_STRING*);
}

#define FILE_OPEN_FOR_BACKUP_INTENT 0x00004000
#define FILE_OPEN_REPARSE_POINT     0x00200000
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#define InitializeObjectAttributes(p, n, a, r, s) \
    {                                             \
        (p)->Length = sizeof(OBJECT_ATTRIBUTES);  \
        (p)->RootDirectory = r;                   \
        (p)->Attributes = a;                      \
        (p)->ObjectName = n;                      \
        (p)->SecurityDescriptor = s;              \
        (p)->SecurityQualityOfService = NULL;     \
    }

namespace bee::win {
    bool unlink(const wchar_t* path) {
        UNICODE_STRING uni_path;
        if (!RtlDosPathNameToNtPathName_U(path, &uni_path, NULL, NULL)) {
            SetLastError(ERROR_PATH_NOT_FOUND);
            return false;
        }
        HANDLE            fh = NULL;
        IO_STATUS_BLOCK   io;
        OBJECT_ATTRIBUTES attr;
        InitializeObjectAttributes(&attr, &uni_path, 0, NULL, NULL);
        NTSTATUS status = NtOpenFile(&fh, DELETE, &attr, &io, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, FILE_OPEN_FOR_BACKUP_INTENT | FILE_OPEN_REPARSE_POINT);
        if (NT_SUCCESS(status)) {
            FILE_DISPOSITION_INFORMATION_EX fdie;
            fdie.Flags = FILE_DISPOSITION_DELETE | FILE_DISPOSITION_POSIX_SEMANTICS | FILE_DISPOSITION_IGNORE_READONLY_ATTRIBUTE;
            status = NtSetInformationFile(fh, &io, &fdie, sizeof(fdie), FileDispositionInformationEx);
            NtClose(fh);
        }
        RtlFreeUnicodeString(&uni_path);
        if (NT_SUCCESS(status)) {
            return true;
        }
        DWORD code = RtlNtStatusToDosError(status);
        SetLastError(code);
        return false;
    }
}
