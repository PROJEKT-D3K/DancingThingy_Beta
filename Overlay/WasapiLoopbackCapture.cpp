#include "WasapiLoopbackCapture.h"

#ifdef _WIN32

// windows.h first, then the WASAPI headers; same WINVER contract that
// OverlayWindow.cpp relies on.
#ifndef _WIN32_WINNT
 #define _WIN32_WINNT 0x0601
#endif
#ifndef WINVER
 #define WINVER 0x0601
#endif
#ifndef NOMINMAX
 #define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
 #define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <ksmedia.h>
#include <functiondiscoverykeys_devpkey.h>
#include <wrl/client.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <thread>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace dt
{

namespace
{
    juce::String hresultText (HRESULT hr)
    {
        return "HRESULT 0x" + juce::String::toHexString ((int) hr);
    }

    juce::String deviceIdOf (IMMDevice* dev)
    {
        LPWSTR id = nullptr;
        if (dev != nullptr && SUCCEEDED (dev->GetId (&id)) && id != nullptr)
        {
            juce::String s (id);
            CoTaskMemFree (id);
            return s;
        }
        return {};
    }

    juce::String friendlyNameOf (IMMDevice* dev)
    {
        if (dev == nullptr)
            return "unknown";

        IPropertyStore* props = nullptr;
        if (FAILED (dev->OpenPropertyStore (STGM_READ, &props)) || props == nullptr)
            return "unknown";

        juce::String name = "unknown";
        PROPVARIANT var;
        PropVariantInit (&var);

        if (SUCCEEDED (props->GetValue (PKEY_Device_FriendlyName, &var))
            && var.vt == VT_LPWSTR && var.pwszVal != nullptr)
        {
            name = juce::String (var.pwszVal);
        }

        PropVariantClear (&var);
        props->Release();
        return name;
    }
}

struct WasapiLoopbackCapture::Impl
{
    ComPtr<IMMDeviceEnumerator> enumerator;
    ComPtr<IMMDevice>           device;
    ComPtr<IAudioClient>        client;
    ComPtr<IAudioCaptureClient> capture;

    std::thread thread;   // owned by start()/stop(), joined there

    std::atomic<bool> running    { false };
    std::atomic<bool> openDone   { false };
    std::atomic<bool> openOk     { false };
    std::atomic<bool> threadDone { false };

    mutable juce::CriticalSection errorLock;
    juce::String lastError;
    juce::String deviceName;
    double       sampleRate = 48000.0;

    int  channels       = 2;
    bool isFloat        = true;
    int  bytesPerSample = 4;

    std::vector<float> convertBuffer;
    BlockCallback      callback;

    void setError (const juce::String& e)
    {
        const juce::ScopedLock sl (errorLock);
        lastError = e;
    }

    juce::String getError() const
    {
        const juce::ScopedLock sl (errorLock);
        return lastError;
    }

    juce::String getName() const
    {
        const juce::ScopedLock sl (errorLock);
        return deviceName;
    }

    // ------------------------------------------------------------------
    // Opens one render endpoint for loopback capture. Returns false and sets
    // lastError on failure, leaving client/capture cleared for the next try.
    bool tryOpenDevice (IMMDevice* dev, const juce::String& label)
    {
        if (dev == nullptr)
            return false;

        device = dev;

        const auto name = friendlyNameOf (dev);
        {
            juce::ScopedLock sl (errorLock);
            deviceName = name;
        }

        juce::Logger::writeToLog ("[WASAPI] " + label + ": '" + name + "'");

        HRESULT hr = device->Activate (__uuidof (IAudioClient), CLSCTX_ALL, nullptr,
                                       (void**) client.GetAddressOf());
        if (FAILED (hr))
        {
            setError ("Activate(IAudioClient) failed: " + hresultText (hr));
            client.Reset();
            return false;
        }

        WAVEFORMATEX* mix = nullptr;
        hr = client->GetMixFormat (&mix);
        if (FAILED (hr) || mix == nullptr)
        {
            setError ("GetMixFormat failed: " + hresultText (hr));
            client.Reset();
            return false;
        }

        channels       = (int) mix->nChannels;
        sampleRate     = (double) mix->nSamplesPerSec;
        bytesPerSample = (int) mix->wBitsPerSample / 8;
        isFloat        = (mix->wFormatTag == WAVE_FORMAT_IEEE_FLOAT);

        if (mix->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
        {
            auto* ext = reinterpret_cast<WAVEFORMATEXTENSIBLE*> (mix);
            isFloat = IsEqualGUID (ext->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);
        }

        // Feeding an unexpected layout into the onset detector would produce
        // nonsense, so refuse rather than guess.
        const bool supported = (isFloat && bytesPerSample == 4)
                            || (! isFloat && bytesPerSample == 2);

        if (! supported)
        {
            setError ("unsupported mix format: " + juce::String (mix->wBitsPerSample)
                      + "-bit, tag " + juce::String ((int) mix->wFormatTag));
            CoTaskMemFree (mix);
            client.Reset();
            return false;
        }

        // Duration 0 = let the shared-mode engine pick its own period. A fixed
        // 100 ms buffer makes some endpoints (notably Bluetooth headsets)
        // reject Initialize with ERROR_ENDPOINT_DUPLICATE.
        juce::Logger::writeToLog ("[WASAPI] Initialize(loopback, "
                                  + juce::String (channels) + " ch @ "
                                  + juce::String (sampleRate, 0) + " Hz)...");

        hr = client->Initialize (AUDCLNT_SHAREMODE_SHARED,
                                 AUDCLNT_STREAMFLAGS_LOOPBACK,
                                 0, 0, mix, nullptr);
        CoTaskMemFree (mix);

        if (FAILED (hr))
        {
            setError ("Initialize(loopback) failed: " + hresultText (hr));
            client.Reset();
            return false;
        }

        hr = client->GetService (IID_PPV_ARGS (capture.GetAddressOf()));
        if (FAILED (hr))
        {
            setError ("GetService(IAudioCaptureClient) failed: " + hresultText (hr));
            client.Reset();
            return false;
        }

        hr = client->Start();
        if (FAILED (hr))
        {
            setError ("IAudioClient::Start failed: " + hresultText (hr));
            capture.Reset();
            client.Reset();
            return false;
        }

        return true;
    }

    bool open()
    {
        juce::Logger::writeToLog ("[WASAPI] CoCreateInstance(MMDeviceEnumerator)...");
        HRESULT hr = CoCreateInstance (__uuidof (MMDeviceEnumerator), nullptr,
                                       CLSCTX_ALL,
                                       IID_PPV_ARGS (enumerator.GetAddressOf()));
        if (FAILED (hr))
        {
            setError ("CoCreateInstance(MMDeviceEnumerator) failed: " + hresultText (hr));
            return false;
        }

        // 1) the default multimedia endpoint, which is what the user hears
        juce::String defaultId;
        ComPtr<IMMDevice> def;

        if (SUCCEEDED (enumerator->GetDefaultAudioEndpoint (eRender, eMultimedia,
                                                             def.GetAddressOf())))
        {
            defaultId = deviceIdOf (def.Get());

            if (tryOpenDevice (def.Get(), "default endpoint"))
                return true;

            juce::Logger::writeToLog ("[WASAPI] default endpoint unusable ("
                                      + getError() + "), scanning other render endpoints");
        }
        else
        {
            juce::Logger::writeToLog ("[WASAPI] no default render endpoint: " + hresultText (hr));
        }

        // 2) any other active render endpoint (a Bluetooth headset often
        //    rejects loopback while the speakers or a USB interface accept it)
        ComPtr<IMMDeviceCollection> col;
        if (FAILED (enumerator->EnumAudioEndpoints (eRender, DEVICE_STATE_ACTIVE,
                                                     col.GetAddressOf())))
        {
            setError ("EnumAudioEndpoints failed");
            return false;
        }

        UINT count = 0;
        col->GetCount (&count);

        for (UINT i = 0; i < count; ++i)
        {
            ComPtr<IMMDevice> d;
            if (FAILED (col->Item (i, d.GetAddressOf())))
                continue;

            if (deviceIdOf (d.Get()) == defaultId)
                continue;   // already attempted above

            if (tryOpenDevice (d.Get(), "fallback endpoint"))
                return true;
        }

        setError ("no render endpoint accepted loopback capture");
        return false;
    }

    // ------------------------------------------------------------------
    void drain()
    {
        for (;;)
        {
            UINT32 packetFrames = 0;
            const HRESULT sizeHr = capture->GetNextPacketSize (&packetFrames);

            if (sizeHr == AUDCLNT_E_DEVICE_INVALIDATED)
            {
                setError ("device invalidated (default output changed?)");
                running.store (false);
                return;
            }

            if (FAILED (sizeHr) || packetFrames == 0)
                return;

            BYTE*  data   = nullptr;
            UINT32 frames = 0;
            DWORD  flags  = 0;

            if (FAILED (capture->GetBuffer (&data, &frames, &flags, nullptr, nullptr)))
                return;

            if (frames > 0 && callback)
            {
                const size_t n = (size_t) frames * (size_t) channels;
                convertBuffer.resize (n);

                if ((flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0 || data == nullptr)
                {
                    std::fill (convertBuffer.begin(), convertBuffer.end(), 0.0f);
                }
                else if (isFloat)
                {
                    std::memcpy (convertBuffer.data(), data, n * sizeof (float));
                }
                else
                {
                    const auto* src = reinterpret_cast<const int16_t*> (data);
                    for (size_t i = 0; i < n; ++i)
                        convertBuffer[i] = (float) src[i] * (1.0f / 32768.0f);
                }

                callback (convertBuffer.data(), (int) frames, channels, sampleRate);
            }

            capture->ReleaseBuffer (frames);
        }
    }

    void run()
    {
        // Every COM call of this capture happens here, on an MTA thread.
        // Creating the client on the UI's STA thread and calling Initialize
        // there wedges forever before the first message pump exists.
        const HRESULT coHr = CoInitializeEx (nullptr, COINIT_MULTITHREADED);

        APTTYPE aptType = APTTYPE_CURRENT;
        APTTYPEQUALIFIER aptQual = APTTYPEQUALIFIER_NONE;
        const HRESULT aptHr = CoGetApartmentType (&aptType, &aptQual);

        juce::Logger::writeToLog ("[WASAPI] CoInitializeEx=" + hresultText (coHr)
                                  + " -> aptType=" + juce::String ((int) aptType)
                                  + " (1=MTA, 0=STA), aptHr=" + hresultText (aptHr));

        const bool ok = open();
        openOk.store (ok);
        openDone.store (true);

        if (ok)
        {
            juce::Logger::writeToLog ("[WASAPI] capturing default output: '"
                                      + getName() + "' @ "
                                      + juce::String (sampleRate, 0) + " Hz");

            while (running.load())
            {
                drain();
                juce::Thread::sleep (10);
            }
        }
        else
        {
            juce::Logger::writeToLog ("[WASAPI] open failed: " + getError());
        }

        close();
        threadDone.store (true);

        if (SUCCEEDED (coHr))
            CoUninitialize();
    }

    void close()
    {
        if (client != nullptr)
            client->Stop();

        capture.Reset();
        client.Reset();
        device.Reset();
        enumerator.Reset();
    }
};

//==============================================================================
WasapiLoopbackCapture::WasapiLoopbackCapture()
    : impl (std::make_shared<Impl>())
{
}

WasapiLoopbackCapture::~WasapiLoopbackCapture()
{
    stop();
}

bool WasapiLoopbackCapture::start (BlockCallback cb)
{
    if (impl->running.load())
        return true;

    if (impl->thread.joinable())
    {
        if (impl->threadDone.load())
        {
            impl->thread.join();
        }
        else
        {
            // The previous attempt wedged inside a COM call. Abandon it: the
            // detached thread keeps its own shared_ptr, so nothing dangles.
            impl->thread.detach();
            impl = std::make_shared<Impl>();
        }
    }
    else
    {
        impl->close();
    }

    impl->setError ({});
    impl->callback = std::move (cb);
    impl->openDone.store (false);
    impl->openOk.store (false);
    impl->threadDone.store (false);
    impl->running.store (true);

    auto keepAlive = impl;

    try
    {
        impl->thread = std::thread ([keepAlive] { keepAlive->run(); });
    }
    catch (...)
    {
        impl->running.store (false);
        impl->setError ("could not create capture thread");
        return false;
    }

    // Opening continues on the capture thread: the first Initialize on a
    // sleeping audio engine can take a while, and the overlay must not freeze.
    return true;
}

void WasapiLoopbackCapture::stop()
{
    impl->running.store (false);

    if (! impl->thread.joinable())
        return;

    const auto deadline = juce::Time::getMillisecondCounter() + 2000;
    while (! impl->threadDone.load() && juce::Time::getMillisecondCounter() < deadline)
        juce::Thread::sleep (10);

    if (impl->threadDone.load())
        impl->thread.join();
    else
        impl->thread.detach();   // wedged COM call: never block the caller
}

bool WasapiLoopbackCapture::isRunning() const
{
    return impl->running.load();
}

juce::String WasapiLoopbackCapture::getLastError() const
{
    return impl->getError();
}

juce::String WasapiLoopbackCapture::getDeviceName() const
{
    return impl->getName();
}

double WasapiLoopbackCapture::getSampleRate() const
{
    return impl->sampleRate;
}

} // namespace dt

#else  // ! _WIN32

namespace dt
{

struct WasapiLoopbackCapture::Impl {};

WasapiLoopbackCapture::WasapiLoopbackCapture() : impl (std::make_shared<Impl>()) {}
WasapiLoopbackCapture::~WasapiLoopbackCapture() = default;

bool WasapiLoopbackCapture::start (BlockCallback) { return false; }
void WasapiLoopbackCapture::stop() {}
bool WasapiLoopbackCapture::isRunning() const { return false; }
juce::String WasapiLoopbackCapture::getLastError() const { return "not supported on this platform"; }
juce::String WasapiLoopbackCapture::getDeviceName() const { return {}; }
double WasapiLoopbackCapture::getSampleRate() const { return 0.0; }

} // namespace dt

#endif
