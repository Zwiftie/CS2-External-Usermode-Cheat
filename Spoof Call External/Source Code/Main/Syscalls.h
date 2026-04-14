#pragma once
#include <windows.h>

typedef NTSTATUS(NTAPI* _NtReadVirtualMemory)(HANDLE, PVOID, PVOID, SIZE_T, PSIZE_T);
typedef NTSTATUS(NTAPI* _NtWriteVirtualMemory)(HANDLE, PVOID, PVOID, SIZE_T, PSIZE_T);
typedef NTSTATUS(NTAPI* _NtClose)(HANDLE);

// Get syscall stub from ntdll (bypasses hooks)
inline _NtReadVirtualMemory GetNtReadVirtualMemory() {
    static _NtReadVirtualMemory pFn = (_NtReadVirtualMemory)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtReadVirtualMemory");
    return pFn;
}

inline _NtWriteVirtualMemory GetNtWriteVirtualMemory() {
    static _NtWriteVirtualMemory pFn = (_NtWriteVirtualMemory)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtWriteVirtualMemory");
    return pFn;
}

// Direct syscall macros (using inline assembly for x64)
// For MSVC, you need to write a wrapper. Simpler: just call the function pointer from ntdll.
// That still goes through ntdll, but not hooked by user‑mode.