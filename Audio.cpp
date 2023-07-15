#include "Audio.h"

#include <iostream>
#include <stdio.h>
#include <cstdint>

#include <Functiondiscoverykeys_devpkey.h>

//----------------------------------------------------------------------------------
// Volume Monitor
VolumeMonitor::VolumeMonitor(Audio* audio, AudioDevice& device) : cRef_(1), audio_(audio) {
    HRESULT hr;
    
    // Activalte volume endpoint
    endpointVolume_ = nullptr;
    hr = device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL, (void**)&endpointVolume_);
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
        *ppvInterface = NULL;
        return E_NOINTERFACE;
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE VolumeMonitor::OnNotify(PAUDIO_VOLUME_NOTIFICATION_DATA pNotify) {
    if (pNotify == NULL) {
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
    IPropertyStore *pProps = NULL;
    LPWSTR pwszID = NULL;

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
    IAudioEndpointVolume* pAudioEndpointVolume = NULL;

    hr = device_->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL, (void**)&pAudioEndpointVolume);
    if (FAILED(hr)) {
        printf("Failed to activate endpoint volume: %x\n", hr);
        return;
    }

    hr = pAudioEndpointVolume->SetMasterVolumeLevelScalar(volume, NULL);
    if (FAILED(hr)) {
        printf("Failed to set master volume level: %x\n", hr);
        pAudioEndpointVolume->Release();
        return;
    }

    pAudioEndpointVolume->Release();
}

//----------------------------------------------------------------------------------
// Audio

Audio::Audio() : volumeMonitor_(nullptr) {
    HRESULT hr = CoInitialize(NULL);
}

AudioDevice&& Audio::getDefaultDevice() {
    HRESULT hr;
    IMMDeviceEnumerator* pEnumerator = NULL;
    IMMDevice* pDevice = NULL;

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
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

    // return std::move(AudioDevice(pDevice));
    return std::move(*(new AudioDevice(pDevice)));
}

AudioDevice* Audio::getDevice(const std::wstring& deviceName) {
    HRESULT hr;
    IMMDeviceEnumerator *pEnumerator = NULL;
    IMMDeviceCollection *pCollection = NULL;
    IMMDevice *pDevice = NULL;
    IPropertyStore *pProps = NULL;
    LPWSTR pwszID = NULL;

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
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
    IMMDeviceEnumerator *pEnumerator = NULL;
    IMMDeviceCollection *pCollection = NULL;
    IMMDevice *pDevice = NULL;
    IPropertyStore *pProps = NULL;
    LPWSTR pwszID = NULL;


    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
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
        std::wcerr << L"Failed to find device " << targetName << std::endl;
    }
    deviceTargets_.push_back(std::move(*device));
}

void Audio::removeSyncTo(const std::wstring& targetName) {
    for (auto it = deviceTargets_.begin(); it != deviceTargets_.end(); ++it) {
        if (it->getName() == targetName) {
            deviceTargets_.erase(it);
            break;
        }
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
}

void Audio::setVolume(float volume) {
    for (auto& device : deviceTargets_) {
        device.setVolume(volume);
    }
}
