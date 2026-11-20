#pragma once

#include <Windows.h>

// Opens the process that started this one, given its id, to wait on it. aGone is set when that process has already
// exited: its id no longer exists, or now belongs to a process that started after this one (ids are reused).
inline HANDLE OpenParentProcess(const DWORD aParentId, bool& aGone) noexcept
{
    aGone = false;

    const HANDLE hParent = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, aParentId);
    if (!hParent)
    {
        aGone = GetLastError() == ERROR_INVALID_PARAMETER;
        return nullptr;
    }

    FILETIME parentStart{}, ownStart{}, unused{};
    if (!GetProcessTimes(hParent, &parentStart, &unused, &unused, &unused) || !GetProcessTimes(GetCurrentProcess(), &ownStart, &unused, &unused, &unused))
    {
        CloseHandle(hParent);
        return nullptr;
    }

    if (CompareFileTime(&parentStart, &ownStart) > 0)
    {
        aGone = true;
        CloseHandle(hParent);
        return nullptr;
    }

    return hParent;
}
