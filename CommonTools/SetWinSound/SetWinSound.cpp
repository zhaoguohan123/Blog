#include "SetWinSound.h"

SetWinSound::SetWinSound()
{
}

SetWinSound::~SetWinSound()
{
}

auto SetWinSound::get_audio_devices() -> const std::vector<AUDIODEVICEPARAMETERS>&
{
	DeviceParameters_.clear();

	ComPtr<IMMDeviceEnumerator> enumerator;
	ComPtr<IMMDeviceCollection> collection;

	CoInitialize(NULL);
    UINT count = 0;
    do
    {
        HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)enumerator.GetAddressOf());
        if (FAILED(hr)) {
            LOGGER_ERROR("Failed to create device enumerator {}", hr);
            break;
        }

        hr = enumerator->EnumAudioEndpoints(eAll, DEVICE_STATE_ACTIVE, collection.GetAddressOf());
        if (FAILED(hr)) {
            LOGGER_ERROR("Failed to enumerate audio endpoints {}", hr);
            break;
        }

        hr = collection->GetCount(&count);
        if (FAILED(hr)) {
            LOGGER_ERROR("Failed to get device count{}", hr);
            break;
        }

        for (UINT i = 0; i < count; i++) {
            ComPtr<IMMDevice> device;
            ComPtr<IMMEndpoint> endpoint;
            AUDIODEVICEPARAMETERS device_parameters;
            hr = collection->Item(i, device.GetAddressOf());
            if (FAILED(hr)) {
                LOGGER_ERROR("Failed to get device{}", hr);
                continue;
            }

            LPWSTR deviceId;
            hr = device->GetId(&deviceId);
            if (FAILED(hr)) {
                LOGGER_ERROR("Failed to get device ID {}", hr);
                continue;
            }

            hr = device->QueryInterface(__uuidof(IMMEndpoint), (void**)endpoint.GetAddressOf());
            if (FAILED(hr)) {
                LOGGER_ERROR("Failed to get endpoint interface {}", hr);
                CoTaskMemFree(deviceId);
                continue;
            }

            EDataFlow dataFlow;
            hr = endpoint->GetDataFlow(&dataFlow);
            if (FAILED(hr)) {
                LOGGER_ERROR("Failed to get data flow direction {}", hr);
                CoTaskMemFree(deviceId);
                continue;
            }

            ComPtr<IPropertyStore> propertyStore;
            hr = device->OpenPropertyStore(STGM_READ, propertyStore.GetAddressOf());
            if (FAILED(hr)) {
                LOGGER_ERROR("Failed to open property store {}", hr);
                CoTaskMemFree(deviceId);
                continue;
            }

            PROPVARIANT deviceDesc;
            PropVariantInit(&deviceDesc);
            hr = propertyStore->GetValue(PKEY_Device_FriendlyName, &deviceDesc);
            if (FAILED(hr)) {
                LOGGER_ERROR("Failed to get device description {}", hr);
                CoTaskMemFree(deviceId);
                continue;
            }

            ComPtr<IAudioEndpointVolume> endpointVolume;
            hr = device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL, (void**)endpointVolume.GetAddressOf());
            if (FAILED(hr)) {
                LOGGER_ERROR("Failed to get endpoint volume control interface {}", hr);
                CoTaskMemFree(deviceId);
                continue;
            }

            float volumeLevel;
            hr = endpointVolume->GetMasterVolumeLevelScalar(&volumeLevel);
            if (FAILED(hr)) {
                LOGGER_ERROR("Failed to get volume level {}", hr);
                CoTaskMemFree(deviceId);
                continue;
            }

            ComPtr<IMMDevice> defaultDevice;
            hr = enumerator->GetDefaultAudioEndpoint(dataFlow, eConsole, defaultDevice.GetAddressOf());
            if (FAILED(hr)) {
                LOGGER_ERROR("Failed to get default device {}", hr);
            }
            else {
                LPWSTR defaultDeviceId;
                hr = defaultDevice->GetId(&defaultDeviceId);
                if (SUCCEEDED(hr)) {
                    bool isDefault = (wcscmp(deviceId, defaultDeviceId) == 0);
                    if (isDefault) {
                        device_parameters.defaultdevice = true;
                       LOGGER_INFO( "Device #{}  is the default device", i);
                    }
                    CoTaskMemFree(defaultDeviceId);
                }
            }

            device_parameters.name = U2A(deviceDesc.pwszVal);
            device_parameters.value = volumeLevel * 100;
            device_parameters.device_id = deviceId;

            if (dataFlow == eRender) {
                device_parameters.properties = Render;
                LOGGER_INFO("Output Device #{} : {}, Volume:{}", i, (device_parameters.name).c_str(), device_parameters.value);
            }
            else if (dataFlow == eCapture) {
                device_parameters.properties = Capture;
                LOGGER_INFO("Input Device # {} :{},  Volume:{}", i, (device_parameters.name).c_str(), device_parameters.value);
            }
            DeviceParameters_.push_back(device_parameters);
            PropVariantClear(&deviceDesc);
            CoTaskMemFree(deviceId);
        }
    } while (FALSE);

    CoUninitialize();

    return DeviceParameters_;
}

auto SetWinSound::set_volume_by_name(std::string& deviceName, float volumeLevel) -> BOOL
{
    BOOL bRet = FALSE;
    
    ComPtr<IMMDeviceEnumerator> enumerator;
    ComPtr<IMMDeviceCollection> collection;

    CoInitialize(NULL);
    do
    {
        HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)enumerator.GetAddressOf());
        if (FAILED(hr)) {
            LOGGER_ERROR("Failed to create device enumerator {}", hr);
            break;
        }

        hr = enumerator->EnumAudioEndpoints(eAll, DEVICE_STATE_ACTIVE, collection.GetAddressOf());
        if (FAILED(hr)) {
            LOGGER_ERROR("Failed to enumerate audio endpoints {}", hr);
            break;
        }

        UINT count = 0;
        hr = collection->GetCount(&count);
        if (FAILED(hr)) {
            LOGGER_ERROR("Failed to get device count {}", hr);
            break;
        }

        for (UINT i = 0; i < count; i++) {
            ComPtr<IMMDevice> device;
            ComPtr<IAudioEndpointVolume> endpointVolume;

            hr = collection->Item(i, device.GetAddressOf());
            if (FAILED(hr)) {
                LOGGER_ERROR("Failed to get device {}", hr);
                continue;
            }

            LPWSTR deviceId;
            hr = device->GetId(&deviceId);
            if (FAILED(hr)) {
                LOGGER_ERROR("Failed to get device ID {}", hr);
                continue;
            }

            ComPtr<IPropertyStore> propertyStore;
            hr = device->OpenPropertyStore(STGM_READ, propertyStore.GetAddressOf());
            if (FAILED(hr)) {
                LOGGER_ERROR("Failed to open property store {}", hr);
                CoTaskMemFree(deviceId);
                continue;
            }

            PROPVARIANT deviceDesc;
            PropVariantInit(&deviceDesc);
            hr = propertyStore->GetValue(PKEY_Device_FriendlyName, &deviceDesc);
            if (FAILED(hr)) {
                LOGGER_ERROR("Failed to get device description {}", hr);
                CoTaskMemFree(deviceId);
                continue;
            }

            std::string friendlyName = U2A(deviceDesc.pwszVal);
            if (friendlyName == deviceName) {
                hr = device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL, (void**)endpointVolume.GetAddressOf());
                if (FAILED(hr)) {
                    LOGGER_ERROR("Failed to get endpoint volume control interface {}", hr);
                    CoTaskMemFree(deviceId);
                    continue;
                }

                hr = endpointVolume->SetMasterVolumeLevelScalar(volumeLevel, NULL);
                if (FAILED(hr)) {
                    LOGGER_ERROR("Failed to set volume level {}", hr);
                    CoTaskMemFree(deviceId);
                    continue;
                }

                LOGGER_INFO("Volume for device '{}' set to ", friendlyName.c_str(), volumeLevel);
                CoTaskMemFree(deviceId);
                break;
            }

            PropVariantClear(&deviceDesc);
            CoTaskMemFree(deviceId);
        }

        bRet = TRUE;
    } while (FALSE);
   
    CoUninitialize();
    return bRet;
}

void SetWinSound::set_default_audio_devices(std::wstring & device_id)
{

    HRESULT hr = CoInitialize(NULL);
    ComPtr<IPolicyConfigVista> pPolicyConfig;
    ERole reserved = eConsole;

    hr = CoCreateInstance(__uuidof(CPolicyConfigVistaClient), NULL, CLSCTX_ALL, __uuidof(IPolicyConfigVista), (LPVOID*)&pPolicyConfig);

    if (SUCCEEDED(hr))
    {
        hr = pPolicyConfig->SetDefaultEndpoint(device_id.c_str(), reserved);
    }
    else
    {
        LOGGER_ERROR("SetDefaultEndpoint failed! errcode {}", hr);
    }

    CoUninitialize();
}
