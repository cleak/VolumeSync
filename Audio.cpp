#include "Audio.h"

#include <iostream>
#include <stdio.h>
#include <cstdint>

#include <Functiondiscoverykeys_devpkey.h>

#include "Config.h"

//----------------------------------------------------------------------------------
// Volume Monitor
VolumeMonitor::VolumeMonitor(Audio* audio, AudioDevice& device) : cRef_(1), audio_(audio) {
    HRESULT hr;
    
    // Activalte volume endpoint
    endpointVolume_ = nullptr;
    hr = device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, (void**)&endpointVolume_);
    if (FAILED(hr)) {
        printf("Failed to activate IAudioEndpointVolume: %x\n", hr);
        exit(1);
    }

    hr = endpointVolume_->RegisterControlChangeNotify((IAudioEndpointVolumeCallback*)this);
    if (FAILED(hr)) {
        printf("Failed to register audio endpoint volume callback: %x\n", hr);
        exit(1);
    }
}

VolumeMonitor::~VolumeMonitor() {
    endpointVolume_->Release();
}

ULONG STDMETHODCALLTYPE VolumeMonitor::AddRef() {
    return InterlockedIncrement(&cRef_);
}

ULONG STDMETHODCALLTYPE VolumeMonitor::Release() {
    ULONG ulRef = InterlockedDecrement(&cRef_);
    if (ulRef == 0) {
        delete this;
    }
    return ulRef;
}

HRESULT STDMETHODCALLTYPE VolumeMonitor::QueryInterface(REFIID riid, VOID **ppvInterface) {
    if (riid == IID_IUnknown) {
        AddRef();
        *ppvInterface = (IUnknown*)this;
    } else if (__uuidof(IAudioEndpointVolumeCallback) == riid) {
        AddRef();
        *ppvInterface = (IAudioEndpointVolumeCallback*)this;
    } else {
        *ppvInterface = nullptr;
        return E_NOINTERFACE;
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE VolumeMonitor::OnNotify(PAUDIO_VOLUME_NOTIFICATION_DATA pNotify) {
    if (pNotify == nullptr) {
        return E_INVALIDARG;
    }

    if (audio_) {
        audio_->setVolume(pNotify->fMasterVolume);
    }

    return S_OK;
}

void VolumeMonitor::Stop() {
    audio_ = nullptr;
}

/*
    int64_t cRef_;
    Audio* audio_;
};*/

//----------------------------------------------------------------------------------
// Audio Device

AudioDevice::AudioDevice(IMMDevice* device) 
    : device_(device)
{}

AudioDevice::~AudioDevice() {
    if (device_) {
        device_->Release();
    }
}

std::wstring AudioDevice::getName() const {
    HRESULT hr;
    IPropertyStore *pProps = nullptr;
    LPWSTR pwszID = nullptr;

    hr = device_->OpenPropertyStore(STGM_READ, &pProps);
    if (FAILED(hr)) {
        printf("Failed to open property store: %x\n", hr);
        return {};
    }

    PROPVARIANT varName;
    PropVariantInit(&varName);
    hr = pProps->GetValue(PKEY_Device_FriendlyName, &varName);
    if (FAILED(hr)) {
        printf("Failed to get device name: %x\n", hr);
        pProps->Release();
        return {};
    }

    std::wstring name(varName.pwszVal);
    PropVariantClear(&varName);
    pProps->Release();

    return name;
}

void AudioDevice::setVolume(float volume) {
    HRESULT hr;
    IAudioEndpointVolume* pAudioEndpointVolume = nullptr;

    hr = device_->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, (void**)&pAudioEndpointVolume);
    if (FAILED(hr)) {
        printf("Failed to activate endpoint volume: %x\n", hr);
        return;
    }

    hr = pAudioEndpointVolume->SetMasterVolumeLevelScalar(volume, nullptr);
    if (FAILED(hr)) {
        printf("Failed to set master volume level: %x\n", hr);
        pAudioEndpointVolume->Release();
        return;
    }

    pAudioEndpointVolume->Release();
}

//----------------------------------------------------------------------------------
// DeviceChangeMonitor

DeviceChangeMonitor::DeviceChangeMonitor(Audio* audio)
        : audio_(audio), cRef_(1), deviceEnumerator_(nullptr) {
    HRESULT hr;

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&deviceEnumerator_);
    if (FAILED(hr)) {
        printf("Failed to create device enumerator: %x\n", hr);
        exit(1);
    }

    hr = deviceEnumerator_->RegisterEndpointNotificationCallback((IMMNotificationClient*)this);
    if (FAILED(hr)) {
        printf("Failed to register endpoint notification callback: %x\n", hr);
        exit(1);
    }
}

DeviceChangeMonitor::~DeviceChangeMonitor() {
    HRESULT hr;

    hr = deviceEnumerator_->UnregisterEndpointNotificationCallback((IMMNotificationClient*)this);
    if (FAILED(hr)) {
        printf("Failed to unregister endpoint notification callback: %x\n", hr);
        exit(1);
    }

    deviceEnumerator_->Release();
}

STDMETHODIMP DeviceChangeMonitor::QueryInterface(REFIID riid, VOID **ppvInterface) {
    if (IID_IUnknown == riid) {
        AddRef();
        *ppvInterface = (IUnknown*)this;
    } else if (__uuidof(IMMNotificationClient) == riid) {
        AddRef();
        *ppvInterface = (IMMNotificationClient*)this;
    } else {
        *ppvInterface = NULL;
        return E_NOINTERFACE;
    }
    return S_OK;
}

STDMETHODIMP_(ULONG) DeviceChangeMonitor::AddRef() {
    return InterlockedIncrement(&cRef_);
}

STDMETHODIMP_(ULONG) DeviceChangeMonitor::Release() {
    ULONG ulRef = InterlockedDecrement(&cRef_);
    if (0 == ulRef) {
        delete this;
    }
    return ulRef;
}

STDMETHODIMP DeviceChangeMonitor::OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDefaultDeviceId) {
    if (flow == eRender && role == eConsole) {
        audio_->updateDefaultDevice();
    }
    return S_OK;
}

//----------------------------------------------------------------------------------
// Audio

Audio::Audio() : volumeMonitor_(nullptr) {
    HRESULT hr = CoInitialize(nullptr);
    if (FAILED(hr)) {
        printf("Failed to initialize COM: %x\n", hr);
        exit(1);
    }
    loadTargetList();
    updateDefaultDevice();
    deviceChangeMonitor_ = std::make_unique<DeviceChangeMonitor>(this);
}

AudioDevice&& Audio::getDefaultDevice() {
    HRESULT hr;
    IMMDeviceEnumerator* pEnumerator = nullptr;
    IMMDevice* pDevice = nullptr;

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    if (FAILED(hr)) {
        printf("Failed to create device enumerator: %x\n", hr);
        exit(1);
    }

    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
    if (FAILED(hr)) {
        printf("Failed to get default audio endpoint: %x\n", hr);
        pEnumerator->Release();
        exit(1);
    }

    pEnumerator->Release();

    deviceChangeMonitor_.reset();

    return std::move(*(new AudioDevice(pDevice)));
}

AudioDevice* Audio::getDevice(const std::wstring& deviceName) {
    HRESULT hr;
    IMMDeviceEnumerator *pEnumerator = nullptr;
    IMMDeviceCollection *pCollection = nullptr;
    IMMDevice *pDevice = nullptr;
    IPropertyStore *pProps = nullptr;
    LPWSTR pwszID = nullptr;

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    if (FAILED(hr)) {
        printf("Failed to create MMDeviceEnumerator: %x\n", hr);
        return {};
    }

    hr = pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pCollection);
    if (FAILED(hr)) {
        printf("Failed to enumerate audio endpoints: %x\n", hr);
        pEnumerator->Release();
        return {};
    }

    uint32_t deviceCount;
    hr = pCollection->GetCount(&deviceCount);
    if (FAILED(hr)) {
        printf("Failed to get count of audio endpoints: %x\n", hr);
        pEnumerator->Release();
        pCollection->Release();
        return {};
    }

    std::vector<std::wstring> devices;
    for (uint32_t i = 0; i < deviceCount; ++i) {
        hr = pCollection->Item(i, &pDevice);
        if (FAILED(hr)) {
            printf("Failed to get audio endpoint: %x\n", hr);
            pEnumerator->Release();
            pCollection->Release();
            return {};
        }

        hr = pDevice->OpenPropertyStore(STGM_READ, &pProps);
        if (FAILED(hr)) {
            printf("Failed to open property store: %x\n", hr);
            pEnumerator->Release();
            pCollection->Release();
            pDevice->Release();
            return {};
        }

        PROPVARIANT varName;
        PropVariantInit(&varName);
        hr = pProps->GetValue(PKEY_Device_FriendlyName, &varName);
        if (FAILED(hr)) {
            printf("Failed to get device name: %x\n", hr);
            pEnumerator->Release();
            pCollection->Release();
            pDevice->Release();
            pProps->Release();
            return {};
        }

        if (varName.pwszVal == deviceName) {
            pEnumerator->Release();
            pCollection->Release();
            pProps->Release();
            return new AudioDevice(pDevice);
        }

        pDevice->Release();
        PropVariantClear(&varName);
    }

    pEnumerator->Release();
    pCollection->Release();

    return {};
}

std::vector<std::wstring> Audio::getDevices() {
    HRESULT hr;
    IMMDeviceEnumerator *pEnumerator = nullptr;
    IMMDeviceCollection *pCollection = nullptr;
    IMMDevice *pDevice = nullptr;
    IPropertyStore *pProps = nullptr;
    LPWSTR pwszID = nullptr;


    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    if (FAILED(hr)) {
        printf("Failed to create MMDeviceEnumerator: %x\n", hr);
        return {};
    }

    hr = pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pCollection);
    if (FAILED(hr)) {
        printf("Failed to enumerate audio endpoints: %x\n", hr);
        pEnumerator->Release();
        return {};
    }

    uint32_t deviceCount;
    hr = pCollection->GetCount(&deviceCount);
    if (FAILED(hr)) {
        printf("Failed to get count of audio endpoints: %x\n", hr);
        pEnumerator->Release();
        pCollection->Release();
        return {};
    }

    std::vector<std::wstring> devices;
    for (uint32_t i = 0; i < deviceCount; ++i) {
        hr = pCollection->Item(i, &pDevice);
        if (FAILED(hr)) {
            printf("Failed to get audio endpoint: %x\n", hr);
            pEnumerator->Release();
            pCollection->Release();
            return {};
        }

        hr = pDevice->OpenPropertyStore(STGM_READ, &pProps);
        if (FAILED(hr)) {
            printf("Failed to open property store: %x\n", hr);
            pEnumerator->Release();
            pCollection->Release();
            pDevice->Release();
            return {};
        }

        PROPVARIANT varName;
        PropVariantInit(&varName);
        hr = pProps->GetValue(PKEY_Device_FriendlyName, &varName);
        if (FAILED(hr)) {
            printf("Failed to get device name: %x\n", hr);
            pEnumerator->Release();
            pCollection->Release();
            pDevice->Release();
            pProps->Release();
            return {};
        }

        devices.push_back(varName.pwszVal);
        PropVariantClear(&varName);
        pProps->Release();

        pDevice->Release();
        PropVariantClear(&varName);
    }

    pEnumerator->Release();
    pCollection->Release();

    return devices;
}

Audio::~Audio() {
    // Make sure all devices are released before CoUninitialize
    deviceSrc_.reset();
    deviceTargets_.clear();

    CoUninitialize();
}

void Audio::addSyncTo(const std::wstring& targetName) {
    auto device = getDevice(targetName);
    if (!device) {
        std::wcerr << L"Failed to find device: " << targetName << std::endl;
        return;
    }
    deviceTargets_.push_back(std::move(*device));

    saveTargetList();
    resyncVolume();
}

void Audio::removeSyncTo(const std::wstring& targetName) {
    for (auto it = deviceTargets_.begin(); it != deviceTargets_.end(); ++it) {
        if (it->getName() == targetName) {
            deviceTargets_.erase(it);
            break;
        }
    }

    saveTargetList();
}

void Audio::saveTargetList() {
    std::vector<std::wstring> deviceNames;
    for (const auto& device : deviceTargets_) {
        deviceNames.push_back(device.getName());
    }
    WriteSyncDevices(deviceNames);
}

void Audio::loadTargetList() {
    std::vector<std::wstring> deviceNames;
    if (!ReadSyncDevices(deviceNames)) {
        return;
    }

    for (const auto& deviceName : deviceNames) {
        addSyncTo(deviceName);
    }
}

void Audio::updateDefaultDevice() {
    if (volumeMonitor_) {
        volumeMonitor_->Stop();
        volumeMonitor_->Release();
        volumeMonitor_ = nullptr;
    }

    deviceSrc_ = std::make_unique<AudioDevice>(getDefaultDevice());
    volumeMonitor_ = new VolumeMonitor(this, *deviceSrc_);

    // Remove it from the list of targets if it's there
    removeSyncTo(deviceSrc_->getName());

    resyncVolume();
}

void Audio::setVolume(float volume) {
    for (auto& device : deviceTargets_) {
        device.setVolume(volume);
    }
}

std::set<std::wstring> Audio::getSyncedDevices() const {
    std::set<std::wstring> devices;
    for (const auto& device : deviceTargets_) {
        devices.insert(device.getName());
    }
    return devices;
}

void Audio::resyncVolume() {
    if (!deviceSrc_) {
        return;
    }

    HRESULT hr;
    IAudioEndpointVolume* pAudioEndpointVolume = nullptr;

    hr = (*deviceSrc_)->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, (void**)&pAudioEndpointVolume);
    if (FAILED(hr)) {
        printf("Failed to activate endpoint volume: %x\n", hr);
        return;
    }

    float volume;
    hr = pAudioEndpointVolume->GetMasterVolumeLevelScalar(&volume);
    if (FAILED(hr)) {
        printf("Failed to get master volume level: %x\n", hr);
        pAudioEndpointVolume->Release();
        return;
    }

    pAudioEndpointVolume->Release();

    setVolume(volume);
}