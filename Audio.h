#pragma once

#include <cstdint>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <endpointvolume.h>
#include <mmdeviceapi.h>

class Audio;

// Smart wrapper for audio device that automatically releases the device.
class AudioDevice {
  public:
    explicit AudioDevice(IMMDevice* device);
    AudioDevice(AudioDevice&& otherDevice) : device_(otherDevice.device_) {
        otherDevice.device_ = nullptr;
    }

    AudioDevice& operator=(AudioDevice&& otherDevice) {
        device_ = otherDevice.device_;
        otherDevice.device_ = nullptr;
        return *this;
    }

    ~AudioDevice();

    AudioDevice(const AudioDevice&) = delete;
    AudioDevice& operator=(AudioDevice const&) = delete;
    const IMMDevice* operator->() const { return device_; }
    IMMDevice* operator->() { return device_; }

    void setVolume(float volume);

    std::wstring getName() const;

  private:
    IMMDevice* device_;
};

// Monitors volume changes for a particular device.
class VolumeMonitor : public IAudioEndpointVolumeCallback {
  public:
    explicit VolumeMonitor(Audio* audio, AudioDevice& device);
    ~VolumeMonitor();

    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, VOID **ppvInterface);
    HRESULT STDMETHODCALLTYPE OnNotify(PAUDIO_VOLUME_NOTIFICATION_DATA pNotify);
    void Stop();

  private:
    LONG cRef_;
    Audio* audio_;
    IAudioEndpointVolume* endpointVolume_;
};

// Top level audio manager.
class Audio {
  public:
    Audio();
    ~Audio();

    Audio(const Audio&) = delete;
    Audio& operator=(Audio const&) = delete;

    AudioDevice&& getDefaultDevice();
    AudioDevice* getDevice(const std::wstring& deviceName);
    std::vector<std::wstring> getDevices();

    void addSyncTo(const std::wstring& targetName);
    void removeSyncTo(const std::wstring& targetName);

    void updateDefaultDevice();
    void setVolume(float volume);

  private:
    std::unique_ptr<AudioDevice> deviceSrc_;
    std::list<AudioDevice> deviceTargets_;
    VolumeMonitor* volumeMonitor_;
};
