#include <stdio.h>
#include <iostream>

#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <Functiondiscoverykeys_devpkey.h>

#include "Audio.h"

int main() {
    Audio audio;

    std::cout << "Devices: " << std::endl;
    for (const auto& device : audio.getDevices()) {
        std::wcout << L"  " << device << std::endl;
    }

    // std::wstring deviceName = L"SteelSeries Sonar - Media (SteelSeries Sonar Virtual Audio Device)";
    // audio.addSyncTo(deviceName);
    // audio.updateDefaultDevice();

    while (true) {}

    return 0;
}