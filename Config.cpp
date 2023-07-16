#include "Config.h"

#include <windows.h>
#include <fstream>
#include <string>
#include <vector>
#include <KnownFolders.h>
#include <ShlObj.h>

const std::wstring filename = L"SyncDevices.txt";
const std::wstring subDir = L"AudioSync";

bool CreateDirectoryIfNotExist(const std::wstring& path) {
    DWORD dwAttrib = GetFileAttributesW(path.c_str());

    if (dwAttrib != INVALID_FILE_ATTRIBUTES &&
        (dwAttrib & FILE_ATTRIBUTE_DIRECTORY)) {
        return true;
    }

    return CreateDirectoryW(path.c_str(), NULL);
}

bool WriteSyncDevices(const std::vector<std::wstring>& data) {
    PWSTR path = NULL;
    if (SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &path) != S_OK) {
        return false;
    }

    std::wstring fullPath(path);
    fullPath += L"\\" + subDir;
    CoTaskMemFree(path);

    if (!CreateDirectoryIfNotExist(fullPath)) {
        return false;
    }

    fullPath += L"\\" + filename;

    std::wofstream file(fullPath);
    if (!file) {
        return false;
    }

    for (const auto& str : data) {
        file << str << L'\n';
    }

    return true;
}

bool ReadSyncDevices(std::vector<std::wstring>& data) {
    PWSTR path = NULL;
    if (SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &path) != S_OK) {
        return false;
    }

    std::wstring fullPath(path);
    fullPath += L"\\" + subDir + L"\\" + filename;
    CoTaskMemFree(path);

    std::wifstream file(fullPath);
    if (!file) {
        return false;
    }

    std::wstring line;
    while (std::getline(file, line)) {
        data.push_back(line);
    }

    return true;
}
