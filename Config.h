#pragma once

#include <windows.h>
#include <string>
#include <vector>

constexpr wchar_t kRegistryKey[] = L"Software\\AudioSync";
constexpr wchar_t kRegistryValName[] = L"SyncDevices";

bool WriteSyncDevices(
    const std::vector<std::wstring>& data,
    const std::wstring& subKey = kRegistryKey,
    const std::wstring& valueName = kRegistryValName
);

bool ReadSyncDevices(
    std::vector<std::wstring>& data,
    const std::wstring& subKey = kRegistryKey,
    const std::wstring& valueName = kRegistryValName
);
