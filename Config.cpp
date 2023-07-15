#include "Config.h"

bool WriteSyncDevices(
    const std::vector<std::wstring>& data,
    const std::wstring& subKey,
    const std::wstring& valueName
) {
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, subKey.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, NULL) != ERROR_SUCCESS) {
        return false;
    }

    for (size_t i = 0; i < data.size(); ++i) {
        if (RegSetValueExW(hKey, (valueName + std::to_wstring(i)).c_str(), 0, REG_SZ, (BYTE*)data[i].c_str(), (DWORD)((data[i].size() + 1) * sizeof(wchar_t))) != ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return false;
        }
    }

    RegCloseKey(hKey);
    return true;
}

bool ReadSyncDevices(
    std::vector<std::wstring>& data,
    const std::wstring& subKey,
    const std::wstring& valueName
) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, subKey.c_str(), 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return false;
    }

    DWORD i = 0;
    wchar_t value[256];
    DWORD valueSize = sizeof(value);
    while (RegEnumValueW(hKey, i, value, &valueSize, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
        if (std::wstring(value).substr(0, valueName.size()) == valueName) {
            DWORD dataSize;
            if (RegQueryValueExW(hKey, value, NULL, NULL, NULL, &dataSize) != ERROR_SUCCESS) {
                RegCloseKey(hKey);
                return false;
            }

            wchar_t* dataBuffer = new wchar_t[dataSize / sizeof(wchar_t)];
            if (RegQueryValueExW(hKey, value, NULL, NULL, (BYTE*)dataBuffer, &dataSize) != ERROR_SUCCESS) {
                delete[] dataBuffer;
                RegCloseKey(hKey);
                return false;
            }

            data.push_back(std::wstring(dataBuffer));
            delete[] dataBuffer;
        }

        i++;
        valueSize = sizeof(value);
    }

    RegCloseKey(hKey);
    return true;
}
