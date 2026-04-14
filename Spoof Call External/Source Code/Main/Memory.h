#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <cstdint>

// NT_SUCCESS and NTSTATUS are defined here — required by Write<T> template
#include <winternl.h>

// ------------------------------------------------------------------
// NT function typedefs — defined before the class so Read<T>/Write<T>
// can reference them
// ------------------------------------------------------------------
typedef NTSTATUS(NTAPI* pNtReadVirtualMemory_t) (HANDLE, PVOID, PVOID, SIZE_T, PSIZE_T);
typedef NTSTATUS(NTAPI* pNtWriteVirtualMemory_t)(HANDLE, PVOID, PVOID, SIZE_T, PSIZE_T);

// Defined in Memory.cpp, called from the inline templates below
pNtReadVirtualMemory_t  GetNtReadVirtualMemory();
pNtWriteVirtualMemory_t GetNtWriteVirtualMemory();

class Memory {
public:
    uint32_t  pid = 0;
    HANDLE    handle = nullptr;
    uintptr_t client = 0;
    uintptr_t engine = 0;

    bool Init(const wchar_t* processName);
    void Cleanup();
    ~Memory() { Cleanup(); }

    template <typename T>
    T Read(uintptr_t address) const {
        T buffer{};
        if (address) {
            SIZE_T bytesRead = 0;
            GetNtReadVirtualMemory()(handle, (PVOID)address, &buffer, sizeof(T), &bytesRead);
        }
        return buffer;
    }

    bool ReadRaw(uintptr_t address, void* buffer, size_t size) const;

    template <typename T>
    bool Write(uintptr_t address, T value) const {
        if (!address) return false;
        SIZE_T bytesWritten = 0;
        NTSTATUS status = GetNtWriteVirtualMemory()(
            handle, (PVOID)address, &value, sizeof(T), &bytesWritten);
        return NT_SUCCESS(status) && bytesWritten == sizeof(T);
    }

    uintptr_t GetModuleBase(const wchar_t* moduleName);

private:
    // Returns uint32_t — cast from uintptr_t done explicitly in Memory.cpp
    uint32_t GetProcessId(const wchar_t* processName);
};

extern Memory mem;