#ifndef _SET_WIN_SOUND_
#define _SET_WIN_SOUND_

#include "../../Common/CommonHead.h"
#include <wrl/client.h>
#include <Windows.h>
#include <Mmdeviceapi.h>
#include <Endpointvolume.h>
#include <Audioclient.h>
#include "PolicyConfig.h" // 注意头文件引用顺序

template <class T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

typedef enum Deviceproperties {
    Render,
    Capture
} Deviceproperties;

struct AUDIODEVICEPARAMETERS {
    std::string          name;
    Deviceproperties     properties;
    int                  value;
    bool                 defaultdevice = false;
    std::wstring         device_id;
};

const PROPERTYKEY PKEY_Device_FriendlyName = { {0xa45c254e, 0xdf1c, 0x4efd, {0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0}}, 14 };

class SetWinSound
{
public:
    SetWinSound();
    ~SetWinSound();
    auto get_audio_devices() -> const std::vector<AUDIODEVICEPARAMETERS> &;

    auto set_volume_by_name(std::string & deviceName, float volumeLevel) -> BOOL;

    void set_default_audio_devices(std::wstring&);

private:
    std::vector<AUDIODEVICEPARAMETERS> DeviceParameters_;
};


#endif // !_SET_WIN_SOUND