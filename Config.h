#pragma once

#include <string>
#include <vector>

bool WriteSyncDevices(const std::vector<std::wstring>& data);

bool ReadSyncDevices(std::vector<std::wstring>& data);
