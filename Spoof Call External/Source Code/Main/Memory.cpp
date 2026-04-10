#include "Memory.h"
#include "CallStack-Spoofer.h"
#include "xorstr.hpp"
#include <winternl.h>
#include <vector>
#include <string>       // std::wstring

#pragma comment(lib, "ntdll.lib")

// ------------------------------------------------------------------
// NT structs not exposed by winternl.h
// ------------------------------------------------------------------
#ifndef STATUS_INFO_LENGTH_MISMATCH
#define STATUS_INFO_LENGTH_MISMATCH 0xC0000004L
#endif

#define SystemHandleInformation (SYSTEM_INFORMATION_CLASS)16

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
// NT function typedefs
// ------------------------------------------------------------------
typedef NTSTATUS(NTAPI* pNtReadVirtualMemory)      (HANDLE, PVOID, PVOID, SIZE_T, PSIZE_T);
typedef NTSTATUS(NTAPI* pNtWriteVirtualMemory)     (HANDLE, PVOID, PVOID, SIZE_T, PSIZE_T);
typedef NTSTATUS(NTAPI* pNtClose)                  (HANDLE);
typedef NTSTATUS(NTAPI* pNtDuplicateObject)        (HANDLE, HANDLE, HANDLE, PHANDLE, ACCESS_MASK, ULONG, ULONG);
typedef NTSTATUS(NTAPI* pNtQuerySystemInformation) (SYSTEM_INFORMATION_CLASS, PVOID, ULONG, PULONG);

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

// ------------------------------------------------------------------
// Find process ID by name
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
// Hijack a handle to the target process from a trusted system process
// ------------------------------------------------------------------
static HANDLE HijackGameHandle(DWORD targetPid, ACCESS_MASK desiredAccess) {
    SPOOF_FUNC;

    auto NtQuerySystemInformation = GetNtQuerySystemInformation();
    auto NtDuplicateObject = GetNtDuplicateObject();
    auto NtClose = GetNtClose();
    if (!NtQuerySystemInformation || !NtDuplicateObject || !NtClose)
        return nullptr;

    // xorstr_ returns a temporary — store as std::wstring so the pointer stays valid
    const std::wstring candidateNames[] = {
        xorstr_(L"svchost.exe"),
        xorstr_(L"lsass.exe"),
        xorstr_(L"winlogon.exe"),
        xorstr_(L"csrss.exe")
    };

    DWORD candidatePid = 0;
    for (const auto& name : candidateNames) {
        candidatePid = FindProcessId(name.c_str());
        if (candidatePid) break;
    }
    if (!candidatePid) return nullptr;

    HANDLE hCandidate = OpenProcess(PROCESS_DUP_HANDLE, FALSE, candidatePid);
    if (!hCandidate) return nullptr;

    ULONG bufferSize = 0x10000;
    std::vector<BYTE> buffer(bufferSize);

    NTSTATUS status = NtQuerySystemInformation(
        SystemHandleInformation, buffer.data(), bufferSize, &bufferSize);
    if (status == (NTSTATUS)STATUS_INFO_LENGTH_MISMATCH) {
        buffer.resize(bufferSize);
        status = NtQuerySystemInformation(
            SystemHandleInformation, buffer.data(), bufferSize, &bufferSize);
    }
    if (!NT_SUCCESS(status)) {
        CloseHandle(hCandidate);
        return nullptr;
    }

    auto* handleInfo = reinterpret_cast<PSYSTEM_HANDLE_INFORMATION>(buffer.data());
    for (ULONG i = 0; i < handleInfo->NumberOfHandles; ++i) {
        auto* entry = &handleInfo->Handles[i];
        if (entry->UniqueProcessId != (USHORT)candidatePid)
            continue;

        HANDLE dupHandle = nullptr;
        status = NtDuplicateObject(
            hCandidate, (HANDLE)(ULONG_PTR)entry->HandleValue,
            GetCurrentProcess(), &dupHandle,
            0, 0, DUPLICATE_SAME_ACCESS);
        if (!NT_SUCCESS(status) || !dupHandle)
            continue;

        if (GetProcessId(dupHandle) == targetPid) {
            HANDLE finalHandle = nullptr;
            status = NtDuplicateObject(
                hCandidate, (HANDLE)(ULONG_PTR)entry->HandleValue,
                GetCurrentProcess(), &finalHandle,
                desiredAccess, 0, 0);
            NtClose(dupHandle);
            CloseHandle(hCandidate);
            return (NT_SUCCESS(status) && finalHandle) ? finalHandle : nullptr;
        }
        NtClose(dupHandle);
    }

    CloseHandle(hCandidate);
    return nullptr;
}

// ------------------------------------------------------------------
// Memory implementation
// ------------------------------------------------------------------
Memory mem;

bool Memory::Init(const wchar_t* processName) {
    SPOOF_FUNC;

    pid = FindProcessId(processName);
    if (!pid) return false;

    handle = HijackGameHandle(pid, PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION);
    if (!handle) return false;

    // Store xorstr_ temporaries in std::wstring before passing to GetModuleBase
    std::wstring clientDll = xorstr_(L"client.dll");
    std::wstring engineDll = xorstr_(L"engine2.dll");

    client = GetModuleBase(clientDll.c_str());
    engine = GetModuleBase(engineDll.c_str());

    return client != 0 && engine != 0;
}

bool Memory::ReadRaw(uintptr_t address, void* buffer, size_t size) const {
    SPOOF_FUNC;
    if (!address || !buffer) return false;

    SIZE_T bytesRead = 0;
    NTSTATUS status = GetNtReadVirtualMemory()(
        handle, (PVOID)address, buffer, size, &bytesRead);
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
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap == INVALID_HANDLE_VALUE) return 0;

    MODULEENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    uintptr_t result = 0;
    if (Module32FirstW(snap, &entry)) {
        do {
            if (_wcsicmp(entry.szModule, moduleName) == 0) {
                result = reinterpret_cast<uintptr_t>(entry.modBaseAddr);
                break;
            }
        } while (Module32NextW(snap, &entry));
    }
    CloseHandle(snap);
    return result;
}