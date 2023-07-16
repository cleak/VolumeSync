#pragma once

#include <cstdint>
#include <list>
#include <memory>
#include <optional>
#include <set>
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

// Listener for when the default audio device changes.
class DeviceChangeMonitor : public IMMNotificationClient {
  public:
    explicit DeviceChangeMonitor(Audio* audio);
    ~DeviceChangeMonitor();

    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID riid, VOID **ppvInterface);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    // IMMNotificationClient methods
    STDMETHODIMP OnDeviceAdded(LPCWSTR pwstrDeviceId) { return S_OK; }
    STDMETHODIMP OnDeviceRemoved(LPCWSTR pwstrDeviceId) { return S_OK; }
    STDMETHODIMP OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState) { return S_OK; }
    STDMETHODIMP OnPropertyValueChanged(LPCWSTR pwstrDeviceId, const PROPERTYKEY key) { return S_OK; }

    STDMETHODIMP OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDefaultDeviceId);

  private:
    LONG cRef_;
    Audio* audio_;
    IMMDeviceEnumerator* deviceEnumerator_;
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

    void resyncVolume();

    std::set<std::wstring> getSyncedDevices() const;

  private:
    void saveTargetList();
    void loadTargetList();

    std::unique_ptr<AudioDevice> deviceSrc_;
    std::unique_ptr<DeviceChangeMonitor> deviceChangeMonitor_;
    std::list<AudioDevice> deviceTargets_;
    VolumeMonitor* volumeMonitor_;
};
