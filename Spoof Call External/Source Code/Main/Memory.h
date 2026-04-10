#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <cstdint>

class Memory {
public:
    uint32_t pid = 0;
    HANDLE handle = nullptr;
    uintptr_t client = 0;
    uintptr_t engine = 0;

    bool Init(const wchar_t* processName);
    void Cleanup();
    ~Memory() { Cleanup(); }

    template <typename T>
    T Read(uintptr_t address) const {
        T buffer{};
        if (address)
            ReadProcessMemory(handle, reinterpret_cast<LPCVOID>(address), &buffer, sizeof(T), nullptr);
        return buffer;
    }

    bool ReadRaw(uintptr_t address, void* buffer, size_t size) const;

    template <typename T>
    bool Write(uintptr_t address, T value) const {
        if (!address) return false;
        return WriteProcessMemory(handle, reinterpret_cast<LPVOID>(address), &value, sizeof(T), nullptr) != 0;
    }

private:
    uint32_t GetProcessId(const wchar_t* processName);
    uintptr_t GetModuleBase(const wchar_t* moduleName);
};

extern Memory mem;