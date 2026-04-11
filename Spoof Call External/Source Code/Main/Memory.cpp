#include "Memory.h"
#include "CallStack-Spoofer.h"
#include "xorstr.hpp"
#include <winternl.h>
#include <string>
#include <algorithm>
#include <vector>

#pragma comment(lib, "ntdll.lib")

// ------------------------------------------------------------------
// Forward declarations
// ------------------------------------------------------------------
static bool ReadRemoteBuffer(HANDLE hProcess, uintptr_t address, void* buffer, SIZE_T size);
static bool EnableDebugPrivilege();

// ------------------------------------------------------------------
// Direct syscall function pointers
// ------------------------------------------------------------------
typedef NTSTATUS(NTAPI* pNtReadVirtualMemory)(HANDLE, PVOID, PVOID, SIZE_T, PSIZE_T);
typedef NTSTATUS(NTAPI* pNtWriteVirtualMemory)(HANDLE, PVOID, PVOID, SIZE_T, PSIZE_T);
typedef NTSTATUS(NTAPI* pNtClose)(HANDLE);
typedef NTSTATUS(NTAPI* pNtDuplicateObject)(HANDLE, HANDLE, HANDLE, PHANDLE, ACCESS_MASK, ULONG, ULONG);
typedef NTSTATUS(NTAPI* pNtQuerySystemInformation)(SYSTEM_INFORMATION_CLASS, PVOID, ULONG, PULONG);
typedef NTSTATUS(NTAPI* pNtQueryInformationProcess)(HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG);

// ------------------------------------------------------------------
// NT constants not exposed by winternl.h
// ------------------------------------------------------------------
#ifndef STATUS_INFO_LENGTH_MISMATCH
#define STATUS_INFO_LENGTH_MISMATCH 0xC0000004L
#endif

#define SystemHandleInformation (SYSTEM_INFORMATION_CLASS)16

// Handle enumeration structures
typedef struct _SYSTEM_HANDLE_TABLE_ENTRY_INFO {
    USHORT      UniqueProcessId;
    USHORT      CreatorBackTraceIndex;
    UCHAR       ObjectTypeIndex;
    UCHAR       HandleAttributes;
    USHORT      HandleValue;
    PVOID       Object;
    ACCESS_MASK GrantedAccess;
} SYSTEM_HANDLE_TABLE_ENTRY_INFO, * PSYSTEM_HANDLE_TABLE_ENTRY_INFO;

typedef struct _SYSTEM_HANDLE_INFORMATION {
    ULONG                          NumberOfHandles;
    SYSTEM_HANDLE_TABLE_ENTRY_INFO Handles[1];
} SYSTEM_HANDLE_INFORMATION, * PSYSTEM_HANDLE_INFORMATION;

// ------------------------------------------------------------------
// Lazy-loaded NT function pointers
// ------------------------------------------------------------------
static pNtReadVirtualMemory GetNtReadVirtualMemory() {
    static pNtReadVirtualMemory pFn = nullptr;
    if (!pFn) pFn = (pNtReadVirtualMemory)GetProcAddress(
        GetModuleHandleA(xorstr_("ntdll.dll")), xorstr_("NtReadVirtualMemory"));
    return pFn;
}

static pNtWriteVirtualMemory GetNtWriteVirtualMemory() {
    static pNtWriteVirtualMemory pFn = nullptr;
    if (!pFn) pFn = (pNtWriteVirtualMemory)GetProcAddress(
        GetModuleHandleA(xorstr_("ntdll.dll")), xorstr_("NtWriteVirtualMemory"));
    return pFn;
}

static pNtClose GetNtClose() {
    static pNtClose pFn = nullptr;
    if (!pFn) pFn = (pNtClose)GetProcAddress(
        GetModuleHandleA(xorstr_("ntdll.dll")), xorstr_("NtClose"));
    return pFn;
}

static pNtDuplicateObject GetNtDuplicateObject() {
    static pNtDuplicateObject pFn = nullptr;
    if (!pFn) pFn = (pNtDuplicateObject)GetProcAddress(
        GetModuleHandleA(xorstr_("ntdll.dll")), xorstr_("NtDuplicateObject"));
    return pFn;
}

static pNtQuerySystemInformation GetNtQuerySystemInformation() {
    static pNtQuerySystemInformation pFn = nullptr;
    if (!pFn) pFn = (pNtQuerySystemInformation)GetProcAddress(
        GetModuleHandleA(xorstr_("ntdll.dll")), xorstr_("NtQuerySystemInformation"));
    return pFn;
}

static pNtQueryInformationProcess GetNtQueryInformationProcess() {
    static pNtQueryInformationProcess pFn = nullptr;
    if (!pFn) pFn = (pNtQueryInformationProcess)GetProcAddress(
        GetModuleHandleA(xorstr_("ntdll.dll")), xorstr_("NtQueryInformationProcess"));
    return pFn;
}

// ------------------------------------------------------------------
// Enable SeDebugPrivilege (required to open system processes)
// ------------------------------------------------------------------
static bool EnableDebugPrivilege() {
    HANDLE hToken;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
        return false;

    LUID luid;
    if (!LookupPrivilegeValue(nullptr, SE_DEBUG_NAME, &luid)) {
        CloseHandle(hToken);
        return false;
    }

    TOKEN_PRIVILEGES tp;
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    bool success = AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), nullptr, nullptr);
    CloseHandle(hToken);
    return success && GetLastError() == ERROR_SUCCESS;
}

// ------------------------------------------------------------------
// Helper: find process ID by name
// ------------------------------------------------------------------
static DWORD FindProcessId(const wchar_t* processName) {
    SPOOF_FUNC;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    DWORD pid = 0;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, processName) == 0) {
                pid = pe.th32ProcessID;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return pid;
}

// ------------------------------------------------------------------
// Read remote memory via NT syscall
// ------------------------------------------------------------------
static bool ReadRemoteBuffer(HANDLE hProcess, uintptr_t address, void* buffer, SIZE_T size) {
    SIZE_T bytesRead = 0;
    NTSTATUS status = GetNtReadVirtualMemory()(hProcess, (PVOID)address, buffer, size, &bytesRead);
    return NT_SUCCESS(status) && bytesRead == size;
}

// ------------------------------------------------------------------
// Hijack a handle to the target process from a list of candidates
// ------------------------------------------------------------------
static HANDLE HijackGameHandle(DWORD targetPid) {
    SPOOF_FUNC;

    // Enable debug privilege (required for many candidates)
    if (!EnableDebugPrivilege()) {
        printf("[!] Failed to enable debug privilege.\n");
        return nullptr;
    }

    // List of candidate processes that may have a handle to the game
    // construct std::wstring from xorstr_ (avoid calling .crypt_get() on pointer types)
    const std::wstring candidates[] = {
        std::wstring(xorstr_(L"steam.exe")),
        std::wstring(xorstr_(L"csrss.exe")),
        std::wstring(xorstr_(L"winlogon.exe")),
        std::wstring(xorstr_(L"lsass.exe"))
    };

    DWORD candidatePid = 0;
    for (const auto& name : candidates) {
        candidatePid = FindProcessId(name.c_str());
        if (candidatePid) {
            printf("[*] Found candidate process: %ls (PID: %d)\n", name.c_str(), candidatePid);
            break;
        }
    }
    if (!candidatePid) {
        printf("[!] No candidate process found.\n");
        return nullptr;
    }

    // Open the candidate process with PROCESS_DUP_HANDLE
    HANDLE hCandidate = OpenProcess(PROCESS_DUP_HANDLE, FALSE, candidatePid);
    if (!hCandidate) {
        DWORD err = GetLastError();
        printf("[!] Failed to open candidate process. Error: %lu\n", err);
        return nullptr;
    }

    // Query system handle information – dynamic buffer
    ULONG bufferSize = 0x10000;
    std::vector<BYTE> buffer(bufferSize);
    NTSTATUS status;
    while ((status = GetNtQuerySystemInformation()(SystemHandleInformation, buffer.data(), bufferSize, &bufferSize)) == STATUS_INFO_LENGTH_MISMATCH) {
        buffer.resize(bufferSize);
    }
    if (!NT_SUCCESS(status)) {
        printf("[!] NtQuerySystemInformation failed. Status: 0x%08lx\n", status);
        CloseHandle(hCandidate);
        return nullptr;
    }

    auto* handleInfo = reinterpret_cast<PSYSTEM_HANDLE_INFORMATION>(buffer.data());
    printf("[*] Enumerating %lu handles...\n", handleInfo->NumberOfHandles);

    for (ULONG i = 0; i < handleInfo->NumberOfHandles; ++i) {
        auto* entry = &handleInfo->Handles[i];
        if (entry->UniqueProcessId != candidatePid) continue;

        HANDLE dupHandle = nullptr;
        status = GetNtDuplicateObject()(hCandidate, (HANDLE)(ULONG_PTR)entry->HandleValue,
            GetCurrentProcess(), &dupHandle,
            0, 0, DUPLICATE_SAME_ACCESS);
        if (!NT_SUCCESS(status) || !dupHandle) continue;

        // Check if this handle belongs to our target process
        if (GetProcessId(dupHandle) == targetPid) {
            printf("[+] Found a handle to target process in candidate process.\n");
            // Duplicate it with desired access
            HANDLE finalHandle = nullptr;
            status = GetNtDuplicateObject()(hCandidate, (HANDLE)(ULONG_PTR)entry->HandleValue,
                GetCurrentProcess(), &finalHandle,
                PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION,
                0, 0);
            GetNtClose()(dupHandle);
            CloseHandle(hCandidate);
            if (NT_SUCCESS(status) && finalHandle) {
                printf("[+] Handle hijacked successfully.\n");
                return finalHandle;
            }
            else {
                printf("[!] Failed to duplicate handle with desired access.\n");
                return nullptr;
            }
        }
        GetNtClose()(dupHandle);
    }

    printf("[!] No handle to target process found in candidate process.\n");
    CloseHandle(hCandidate);
    return nullptr;
}

// ------------------------------------------------------------------
// Full LDR_DATA_TABLE_ENTRY for PEB walking
// ------------------------------------------------------------------
typedef struct _LDR_DATA_TABLE_ENTRY_FULL {
    LIST_ENTRY  InLoadOrderLinks;
    LIST_ENTRY  InMemoryOrderLinks;
    LIST_ENTRY  InInitializationOrderLinks;
    PVOID       DllBase;
    PVOID       EntryPoint;
    ULONG       SizeOfImage;
    UNICODE_STRING FullDllName;
    UNICODE_STRING BaseDllName;
} LDR_DATA_TABLE_ENTRY_FULL, * PLDR_DATA_TABLE_ENTRY_FULL;

// ------------------------------------------------------------------
// Memory class implementation
// ------------------------------------------------------------------
Memory mem;

bool Memory::Init(const wchar_t* processName) {
    SPOOF_FUNC;

    pid = FindProcessId(processName);
    if (!pid) {
        printf("[!] Could not find process %ls\n", processName);
        return false;
    }
    printf("[*] Target process PID: %d\n", pid);

    // ONLY handle hijacking – NO OpenProcess fallback
    handle = HijackGameHandle(pid);
    if (!handle) {
        printf("[!] Handle hijacking failed. Cheat cannot continue.\n");
        return false;
    }

    // Obfuscated module names
    std::wstring clientDll = xorstr_(L"client.dll");
    std::wstring engineDll = xorstr_(L"engine2.dll");

    client = GetModuleBase(clientDll.c_str());
    engine = GetModuleBase(engineDll.c_str());

    if (!client || !engine) {
        printf("[!] Failed to get module bases.\n");
        return false;
    }

    return true;
}

bool Memory::ReadRaw(uintptr_t address, void* buffer, size_t size) const {
    SPOOF_FUNC;
    if (!address || !buffer) return false;

    SIZE_T bytesRead = 0;
    NTSTATUS status = GetNtReadVirtualMemory()(handle, (PVOID)address, buffer, size, &bytesRead);
    return NT_SUCCESS(status) && bytesRead == size;
}

void Memory::Cleanup() {
    SPOOF_FUNC;
    if (handle) {
        GetNtClose()(handle);
        handle = nullptr;
    }
    pid = 0;
    client = 0;
    engine = 0;
}

uint32_t Memory::GetProcessId(const wchar_t* processName) {
    SPOOF_FUNC;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    uint32_t result = 0;
    if (Process32FirstW(snap, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, processName) == 0) {
                result = entry.th32ProcessID;
                break;
            }
        } while (Process32NextW(snap, &entry));
    }
    CloseHandle(snap);
    return result;
}

uintptr_t Memory::GetModuleBase(const wchar_t* moduleName) {
    SPOOF_FUNC;

    auto NtQueryInformationProcess = GetNtQueryInformationProcess();
    if (!NtQueryInformationProcess) return 0;

    // 1. Get PEB address
    PROCESS_BASIC_INFORMATION pbi{};
    ULONG returnLength = 0;
    NTSTATUS status = NtQueryInformationProcess(handle, ProcessBasicInformation, &pbi, sizeof(pbi), &returnLength);
    if (!NT_SUCCESS(status) || !pbi.PebBaseAddress) return 0;

    // 2. Read PEB
    PEB peb{};
    if (!ReadRemoteBuffer(handle, (uintptr_t)pbi.PebBaseAddress, &peb, sizeof(peb)))
        return 0;
    if (!peb.Ldr) return 0;

    // 3. Read PEB_LDR_DATA
    PEB_LDR_DATA ldr{};
    if (!ReadRemoteBuffer(handle, (uintptr_t)peb.Ldr, &ldr, sizeof(ldr)))
        return 0;

    // 4. Walk the InMemoryOrderModuleList
    uintptr_t headAddr = (uintptr_t)peb.Ldr + offsetof(PEB_LDR_DATA, InMemoryOrderModuleList);
    uintptr_t currentAddr = (uintptr_t)ldr.InMemoryOrderModuleList.Flink;

    while (currentAddr && currentAddr != headAddr) {
        uintptr_t entryBase = currentAddr - offsetof(LDR_DATA_TABLE_ENTRY_FULL, InMemoryOrderLinks);
        LDR_DATA_TABLE_ENTRY_FULL entry{};
        if (!ReadRemoteBuffer(handle, entryBase, &entry, sizeof(entry)))
            break;

        if (entry.BaseDllName.Buffer && entry.BaseDllName.Length > 0) {
            WCHAR nameBuffer[MAX_PATH]{};
            SIZE_T nameSize = (std::min)((SIZE_T)entry.BaseDllName.Length, (SIZE_T)(MAX_PATH - 1) * sizeof(WCHAR));
            if (ReadRemoteBuffer(handle, (uintptr_t)entry.BaseDllName.Buffer, nameBuffer, nameSize)) {
                if (_wcsicmp(nameBuffer, moduleName) == 0)
                    return (uintptr_t)entry.DllBase;
            }
        }
        currentAddr = (uintptr_t)entry.InMemoryOrderLinks.Flink;
    }
    return 0;
}
