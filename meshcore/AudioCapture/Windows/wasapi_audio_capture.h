/*
Copyright 2026 MeshCentral Audio Support

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#ifndef WASAPI_AUDIO_CAPTURE_H
#define WASAPI_AUDIO_CAPTURE_H

#ifdef _WIN32

#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>

#ifdef __cplusplus
extern "C" {
#endif

// Audio callback function type
// Called when audio data is captured
// Parameters:
//   - audioData: PCM audio data buffer
//   - dataSize: Size of audio data in bytes
//   - sampleRate: Sample rate in Hz (e.g., 48000)
//   - channels: Number of audio channels (1=mono, 2=stereo)
//   - bitsPerSample: Bits per sample (typically 16)
//   - userData: User-provided context pointer
typedef void (*AudioCaptureCallback)(
    const BYTE* audioData,
    UINT32 dataSize,
    UINT32 sampleRate,
    UINT16 channels,
    UINT16 bitsPerSample,
    void* userData
);

// Audio capture context
typedef struct WASAPI_AudioCapture {
    IMMDeviceEnumerator* deviceEnumerator;
    IMMDevice* audioDevice;
    IAudioClient* audioClient;
    IAudioCaptureClient* captureClient;
    WAVEFORMATEX* mixFormat;
    HANDLE captureThread;
    HANDLE stopEvent;
    AudioCaptureCallback callback;
    void* userData;
    BOOL isCapturing;
    BOOL isInitialized;
} WASAPI_AudioCapture;

// Initialize audio capture
// Returns: S_OK on success, error code on failure
HRESULT WASAPI_AudioCapture_Initialize(WASAPI_AudioCapture* capture);

// Set the audio callback function
void WASAPI_AudioCapture_SetCallback(
    WASAPI_AudioCapture* capture,
    AudioCaptureCallback callback,
    void* userData
);

// Start capturing audio
// Returns: S_OK on success, error code on failure
HRESULT WASAPI_AudioCapture_Start(WASAPI_AudioCapture* capture);

// Stop capturing audio
void WASAPI_AudioCapture_Stop(WASAPI_AudioCapture* capture);

// Cleanup and release resources
void WASAPI_AudioCapture_Cleanup(WASAPI_AudioCapture* capture);

// Get current audio format information
void WASAPI_AudioCapture_GetFormat(
    WASAPI_AudioCapture* capture,
    UINT32* sampleRate,
    UINT16* channels,
    UINT16* bitsPerSample
);

#ifdef __cplusplus
}
#endif

#endif // _WIN32
#endif // WASAPI_AUDIO_CAPTURE_H
