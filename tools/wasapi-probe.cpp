// Minimal WASAPI probe: does shared-mode / loopback IAudioClient::Initialize
// work on this machine at all? No JUCE, no threads - just the exact call
// sequence the overlay uses, to separate "our code" from "the system".
//
// Build (from a VS Developer Command Prompt):
//   cl /EHsc /utf-8 wasapi-probe.cpp ole32.lib
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <ksmedia.h>
#include <functiondiscoverykeys_devpkey.h>
#include <wrl/client.h>

#include <cstdio>

using Microsoft::WRL::ComPtr;

static const wchar_t* deviceName (IMMDevice* dev)
{
    static wchar_t buffer[512] = L"?";

    IPropertyStore* props = nullptr;
    if (dev != nullptr && SUCCEEDED (dev->OpenPropertyStore (STGM_READ, &props)))
    {
        PROPVARIANT var;
        PropVariantInit (&var);
        if (SUCCEEDED (props->GetValue (PKEY_Device_FriendlyName, &var)) && var.pwszVal != nullptr)
            wcsncpy_s (buffer, var.pwszVal, _TRUNCATE);
        PropVariantClear (&var);
        props->Release();
    }
    return buffer;
}

int wmain()
{
    const HRESULT coHr = CoInitializeEx (nullptr, COINIT_MULTITHREADED);
    wprintf (L"CoInitializeEx: 0x%08X\n", (unsigned) coHr);

    ComPtr<IMMDeviceEnumerator> en;
    HRESULT hr = CoCreateInstance (__uuidof (MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                   IID_PPV_ARGS (en.GetAddressOf()));
    wprintf (L"CoCreateInstance: 0x%08X\n", (unsigned) hr);
    if (FAILED (hr))
        return 1;

    ComPtr<IMMDevice> dev;
    hr = en->GetDefaultAudioEndpoint (eRender, eMultimedia, dev.GetAddressOf());
    wprintf (L"GetDefaultAudioEndpoint: 0x%08X\n", (unsigned) hr);
    if (FAILED (hr))
        return 1;

    wprintf (L"device: %s\n", deviceName (dev.Get()));

    ComPtr<IAudioClient> client;
    hr = dev->Activate (__uuidof (IAudioClient), CLSCTX_ALL, nullptr,
                        (void**) client.GetAddressOf());
    wprintf (L"Activate: 0x%08X\n", (unsigned) hr);
    if (FAILED (hr))
        return 1;

    WAVEFORMATEX* mix = nullptr;
    hr = client->GetMixFormat (&mix);
    wprintf (L"GetMixFormat: 0x%08X  ch=%u rate=%u bits=%u tag=%u cbSize=%u\n",
             (unsigned) hr,
             mix ? mix->nChannels : 0, mix ? mix->nSamplesPerSec : 0,
             mix ? mix->wBitsPerSample : 0, mix ? mix->wFormatTag : 0,
             mix ? mix->cbSize : 0);

    if (mix != nullptr)
    {
        hr = client->Initialize (AUDCLNT_SHAREMODE_SHARED, 0, 0, 0, mix, nullptr);
        wprintf (L"Initialize(plain):    0x%08X\n", (unsigned) hr);

        hr = client->Initialize (AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK,
                                 0, 0, mix, nullptr);
        wprintf (L"Initialize(loopback): 0x%08X\n", (unsigned) hr);

        CoTaskMemFree (mix);
    }

    if (SUCCEEDED (coHr))
        CoUninitialize();

    return 0;
}
