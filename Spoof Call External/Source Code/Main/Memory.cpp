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

// ------------------------------------------------------------------
// NT function typedefs (internal)
// ------------------------------------------------------------------
typedef NTSTATUS(NTAPI* pNtClose)                  (HANDLE);
typedef NTSTATUS(NTAPI* pNtDuplicateObject)        (HANDLE, HANDLE, HANDLE, PHANDLE, ACCESS_MASK, ULONG, ULONG);
typedef NTSTATUS(NTAPI* pNtQuerySystemInformation) (SYSTEM_INFORMATION_CLASS, PVOID, ULONG, PULONG);
typedef NTSTATUS(NTAPI* pNtQueryInformationProcess)(HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG);
typedef NTSTATUS(NTAPI* pNtSetInformationThread)   (HANDLE, ULONG, PVOID, ULONG);

// ------------------------------------------------------------------
// NT constants
// ------------------------------------------------------------------
#ifndef STATUS_INFO_LENGTH_MISMATCH
#define STATUS_INFO_LENGTH_MISMATCH 0xC0000004L
#endif

#define SystemHandleInformation  (SYSTEM_INFORMATION_CLASS)16
#define SystemProcessInformation (SYSTEM_INFORMATION_CLASS)5
#define ThreadHideFromDebugger   0x11

// ------------------------------------------------------------------
// Handle enumeration structs
// ------------------------------------------------------------------
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
// Process enumeration struct — renamed to avoid clash with winternl.h
// ------------------------------------------------------------------
typedef struct _MY_SYSTEM_PROCESS_INFORMATION {
    ULONG          NextEntryOffset;
    ULONG          NumberOfThreads;
    LARGE_INTEGER  SpareLi1;
    LARGE_INTEGER  SpareLi2;
    LARGE_INTEGER  SpareLi3;
    LARGE_INTEGER  CreateTime;
    LARGE_INTEGER  UserTime;
    LARGE_INTEGER  KernelTime;
    UNICODE_STRING ImageName;
    ULONG          BasePriority;
    HANDLE         UniqueProcessId;
    HANDLE         InheritedFromUniqueProcessId;
    ULONG          HandleCount;
    ULONG          SessionId;
    ULONG_PTR      UniqueProcessKey;
    SIZE_T         PeakVirtualSize;
    SIZE_T         VirtualSize;
    ULONG          PageFaultCount;
    SIZE_T         PeakWorkingSetSize;
    SIZE_T         WorkingSetSize;
    SIZE_T         QuotaPeakPagedPoolUsage;
    SIZE_T         QuotaPagedPoolUsage;
    SIZE_T         QuotaPeakNonPagedPoolUsage;
    SIZE_T         QuotaNonPagedPoolUsage;
    SIZE_T         PagefileUsage;
    SIZE_T         PeakPagefileUsage;
    SIZE_T         PrivatePageCount;
    LARGE_INTEGER  ReadOperationCount;
    LARGE_INTEGER  WriteOperationCount;
    LARGE_INTEGER  OtherOperationCount;
    LARGE_INTEGER  ReadTransferCount;
    LARGE_INTEGER  WriteTransferCount;
    LARGE_INTEGER  OtherTransferCount;
} MY_SYSTEM_PROCESS_INFORMATION, * PMY_SYSTEM_PROCESS_INFORMATION;

// ------------------------------------------------------------------
// Full LDR_DATA_TABLE_ENTRY — winternl.h only exposes a stub
// ------------------------------------------------------------------
typedef struct _LDR_DATA_TABLE_ENTRY_FULL {
    LIST_ENTRY     InLoadOrderLinks;
    LIST_ENTRY     InMemoryOrderLinks;
    LIST_ENTRY     InInitializationOrderLinks;
    PVOID          DllBase;
    PVOID          EntryPoint;
    ULONG          SizeOfImage;
    UNICODE_STRING FullDllName;
    UNICODE_STRING BaseDllName;
} LDR_DATA_TABLE_ENTRY_FULL, * PLDR_DATA_TABLE_ENTRY_FULL;

// ------------------------------------------------------------------
// Manual ntdll export resolution — walks PEB, no GetProcAddress
// ------------------------------------------------------------------
static HMODULE GetNtdllBase() {
    PPEB peb = (PPEB)__readgsqword(0x60);
    PLIST_ENTRY head = &peb->Ldr->InMemoryOrderModuleList;
    for (PLIST_ENTRY e = head->Flink; e != head; e = e->Flink) {
        auto* mod = CONTAINING_RECORD(e, LDR_DATA_TABLE_ENTRY_FULL, InMemoryOrderLinks);
        if (mod->BaseDllName.Buffer &&
            _wcsicmp(mod->BaseDllName.Buffer, xorstr_(L"ntdll.dll")) == 0)
            return (HMODULE)mod->DllBase;
    }
    return nullptr;
}

static FARPROC GetNtdllExport(const char* name) {
    HMODULE ntdll = GetNtdllBase();
    if (!ntdll) return nullptr;

    auto* dos = (PIMAGE_DOS_HEADER)ntdll;
    auto* nt = (PIMAGE_NT_HEADERS)((BYTE*)ntdll + dos->e_lfanew);
    auto& exp_dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    auto* exp = (PIMAGE_EXPORT_DIRECTORY)((BYTE*)ntdll + exp_dir.VirtualAddress);

    auto* names = (DWORD*)((BYTE*)ntdll + exp->AddressOfNames);
    auto* ordinals = (WORD*)((BYTE*)ntdll + exp->AddressOfNameOrdinals);
    auto* functions = (DWORD*)((BYTE*)ntdll + exp->AddressOfFunctions);

    for (DWORD i = 0; i < exp->NumberOfNames; i++) {
        if (strcmp((const char*)((BYTE*)ntdll + names[i]), name) == 0)
            return (FARPROC)((BYTE*)ntdll + functions[ordinals[i]]);
    }
    return nullptr;
}

// ------------------------------------------------------------------
// Lazy-loaded NT function pointers — public ones match header decls
// ------------------------------------------------------------------
pNtReadVirtualMemory_t GetNtReadVirtualMemory() {
    static pNtReadVirtualMemory_t pFn =
        (pNtReadVirtualMemory_t)GetNtdllExport(xorstr_("NtReadVirtualMemory"));
    return pFn;
}

pNtWriteVirtualMemory_t GetNtWriteVirtualMemory() {
    static pNtWriteVirtualMemory_t pFn =
        (pNtWriteVirtualMemory_t)GetNtdllExport(xorstr_("NtWriteVirtualMemory"));
    return pFn;
}

static pNtClose GetNtClose() {
    static pNtClose pFn = (pNtClose)GetNtdllExport(xorstr_("NtClose"));
    return pFn;
}

static pNtDuplicateObject GetNtDuplicateObject() {
    static pNtDuplicateObject pFn =
        (pNtDuplicateObject)GetNtdllExport(xorstr_("NtDuplicateObject"));
    return pFn;
}

static pNtQuerySystemInformation GetNtQuerySystemInformation() {
    static pNtQuerySystemInformation pFn =
        (pNtQuerySystemInformation)GetNtdllExport(xorstr_("NtQuerySystemInformation"));
    return pFn;
}

static pNtQueryInformationProcess GetNtQueryInformationProcess() {
    static pNtQueryInformationProcess pFn =
        (pNtQueryInformationProcess)GetNtdllExport(xorstr_("NtQueryInformationProcess"));
    return pFn;
}

static pNtSetInformationThread GetNtSetInformationThread() {
    static pNtSetInformationThread pFn =
        (pNtSetInformationThread)GetNtdllExport(xorstr_("NtSetInformationThread"));
    return pFn;
}

// ------------------------------------------------------------------
// Thread hiding
// ------------------------------------------------------------------
static void HideCurrentThread() {
    auto fn = GetNtSetInformationThread();
    if (fn) fn(GetCurrentThread(), ThreadHideFromDebugger, nullptr, 0);
}

// ------------------------------------------------------------------
// Find process ID via NtQuerySystemInformation (no toolhelp)
// ------------------------------------------------------------------
static DWORD FindProcessId(const wchar_t* processName) {
    SPOOF_FUNC;
    ULONG bufferSize = 0x10000;
    std::vector<BYTE> buffer(bufferSize);
    NTSTATUS status;
    while ((status = GetNtQuerySystemInformation()(
        SystemProcessInformation, buffer.data(), bufferSize, &bufferSize))
        == (NTSTATUS)STATUS_INFO_LENGTH_MISMATCH)
        buffer.resize(bufferSize);

    if (!NT_SUCCESS(status)) return 0;

    auto* entry = (MY_SYSTEM_PROCESS_INFORMATION*)buffer.data();
    for (;;) {
        if (entry->ImageName.Buffer &&
            _wcsicmp(entry->ImageName.Buffer, processName) == 0)
            // explicit cast chain: HANDLE -> uintptr_t -> DWORD (no C4244)
            return static_cast<DWORD>(reinterpret_cast<uintptr_t>(entry->UniqueProcessId));
        if (!entry->NextEntryOffset) break;
        entry = (MY_SYSTEM_PROCESS_INFORMATION*)((BYTE*)entry + entry->NextEntryOffset);
    }
    return 0;
}

// ------------------------------------------------------------------
// Read remote memory
// ------------------------------------------------------------------
static bool ReadRemoteBuffer(HANDLE hProcess, uintptr_t address, void* buffer, SIZE_T size) {
    SIZE_T bytesRead = 0;
    NTSTATUS status = GetNtReadVirtualMemory()(
        hProcess, (PVOID)address, buffer, size, &bytesRead);
    return NT_SUCCESS(status) && bytesRead == size;
}

// ------------------------------------------------------------------
// Hijack handle from steam.exe (or csrss.exe as fallback)
// ------------------------------------------------------------------
static HANDLE HijackGameHandle(DWORD targetPid) {
    SPOOF_FUNC;

    DWORD steamPid = FindProcessId(xorstr_(L"steam.exe"));
    if (!steamPid) steamPid = FindProcessId(xorstr_(L"csrss.exe"));
    if (!steamPid) return nullptr;

    HANDLE hCandidate = OpenProcess(PROCESS_DUP_HANDLE, FALSE, steamPid);
    if (!hCandidate) return nullptr;

    ULONG bufferSize = 0x4000;
    std::vector<BYTE> buffer(bufferSize);
    NTSTATUS status;
    while ((status = GetNtQuerySystemInformation()(
        SystemHandleInformation, buffer.data(), bufferSize, &bufferSize))
        == (NTSTATUS)STATUS_INFO_LENGTH_MISMATCH)
        buffer.resize(bufferSize);

    if (!NT_SUCCESS(status)) { CloseHandle(hCandidate); return nullptr; }

    auto* handleInfo = reinterpret_cast<PSYSTEM_HANDLE_INFORMATION>(buffer.data());
    for (ULONG i = 0; i < handleInfo->NumberOfHandles; ++i) {
        auto* entry = &handleInfo->Handles[i];
        if (entry->UniqueProcessId != (USHORT)steamPid) continue;

        HANDLE dupHandle = nullptr;
        status = GetNtDuplicateObject()(
            hCandidate, (HANDLE)(ULONG_PTR)entry->HandleValue,
            GetCurrentProcess(), &dupHandle,
            0, 0, DUPLICATE_SAME_ACCESS);
        if (!NT_SUCCESS(status) || !dupHandle) continue;

        if (GetProcessId(dupHandle) == targetPid) {
            HANDLE finalHandle = nullptr;
            status = GetNtDuplicateObject()(
                hCandidate, (HANDLE)(ULONG_PTR)entry->HandleValue,
                GetCurrentProcess(), &finalHandle,
                PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION, 0, 0);
            GetNtClose()(dupHandle);
            CloseHandle(hCandidate);
            return (NT_SUCCESS(status) && finalHandle) ? finalHandle : nullptr;
        }
        GetNtClose()(dupHandle);
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
    HideCurrentThread();

    pid = FindProcessId(processName);
    if (!pid) return false;

    handle = HijackGameHandle(pid);
    if (!handle) return false;

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
    if (handle) { GetNtClose()(handle); handle = nullptr; }
    pid = 0; client = 0; engine = 0;
}

uint32_t Memory::GetProcessId(const wchar_t* processName) {
    return FindProcessId(processName);
}

uintptr_t Memory::GetModuleBase(const wchar_t* moduleName) {
    SPOOF_FUNC;

    auto NtQueryInformationProcess = GetNtQueryInformationProcess();
    if (!NtQueryInformationProcess) return 0;

    PROCESS_BASIC_INFORMATION pbi{};
    ULONG returnLength = 0;
    NTSTATUS status = NtQueryInformationProcess(
        handle, ProcessBasicInformation, &pbi, sizeof(pbi), &returnLength);
    if (!NT_SUCCESS(status) || !pbi.PebBaseAddress) return 0;

    PEB peb{};
    if (!ReadRemoteBuffer(handle, (uintptr_t)pbi.PebBaseAddress, &peb, sizeof(peb)))
        return 0;
    if (!peb.Ldr) return 0;

    PEB_LDR_DATA ldr{};
    if (!ReadRemoteBuffer(handle, (uintptr_t)peb.Ldr, &ldr, sizeof(ldr)))
        return 0;

    uintptr_t headAddr = (uintptr_t)peb.Ldr
        + offsetof(PEB_LDR_DATA, InMemoryOrderModuleList);
    uintptr_t currentAddr = (uintptr_t)ldr.InMemoryOrderModuleList.Flink;

    while (currentAddr && currentAddr != headAddr) {
        uintptr_t entryBase = currentAddr
            - offsetof(LDR_DATA_TABLE_ENTRY_FULL, InMemoryOrderLinks);
        LDR_DATA_TABLE_ENTRY_FULL entry{};
        if (!ReadRemoteBuffer(handle, entryBase, &entry, sizeof(entry))) break;

        if (entry.BaseDllName.Buffer && entry.BaseDllName.Length > 0) {
            WCHAR nameBuffer[MAX_PATH]{};
            SIZE_T nameSize = (std::min)(
                (SIZE_T)entry.BaseDllName.Length,
                (SIZE_T)(MAX_PATH - 1) * sizeof(WCHAR));
            if (ReadRemoteBuffer(handle, (uintptr_t)entry.BaseDllName.Buffer,
                nameBuffer, nameSize))
                if (_wcsicmp(nameBuffer, moduleName) == 0)
                    return (uintptr_t)entry.DllBase;
        }
        currentAddr = (uintptr_t)entry.InMemoryOrderLinks.Flink;
    }
    return 0;
}