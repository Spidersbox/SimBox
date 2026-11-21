#ifndef WINAUDIOLATENCY_H
#define WINAUDIOLATENCY_H

#include <QtGlobal>

#ifdef Q_OS_WIN
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>

inline int getWindowsAudioLatencyMs()
{
  int latencyMs = 100; // Safe fallback if query fails
  HRESULT hrCo = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

  IMMDeviceEnumerator *enumerator = nullptr;
  IMMDevice *device = nullptr;
  IAudioClient *audioClient = nullptr;

  if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                 __uuidof(IMMDeviceEnumerator), (void**)&enumerator)))
  {
    if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device)))
    {
      if (SUCCEEDED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&audioClient)))
      {
        WAVEFORMATEX *mixFormat = nullptr;
        if (SUCCEEDED(audioClient->GetMixFormat(&mixFormat)))
        {
          // Initialize briefly in shared mode so GetStreamLatency() reports the endpoint buffer
          if (SUCCEEDED(audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 0, 0, mixFormat, nullptr)))
          {
            REFERENCE_TIME hnsLatency = 0;
            if (SUCCEEDED(audioClient->GetStreamLatency(&hnsLatency)))
            {
              // REFERENCE_TIME is in 100-nanosecond units (10,000 = 1 ms)
              latencyMs = static_cast<int>(hnsLatency / 10000);
            }
          }
          CoTaskMemFree(mixFormat);
        }
        audioClient->Release();
      }
      device->Release();
    }
    enumerator->Release();
  }

  if (SUCCEEDED(hrCo)) CoUninitialize();
  return latencyMs;
}
#else
inline int getWindowsAudioLatencyMs()
{
  return 0; // No extra delay needed on Linux
}
#endif

#endif // WINAUDIOLATENCY_H
