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

#ifndef PULSEAUDIO_CAPTURE_H
#define PULSEAUDIO_CAPTURE_H

#if defined(__linux__) && !defined(_FREEBSD)

#include <pulse/simple.h>
#include <pulse/error.h>
#include <pthread.h>
#include <stdint.h>

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
typedef void (*PulseAudio_Callback)(
    const uint8_t* audioData,
    size_t dataSize,
    uint32_t sampleRate,
    uint16_t channels,
    uint16_t bitsPerSample,
    void* userData
);

// Audio capture context
typedef struct PulseAudio_Capture {
    pa_simple* pulseAudio;
    pthread_t captureThread;
    int isCapturing;
    int isInitialized;
    uint32_t sampleRate;
    uint16_t channels;
    uint16_t bitsPerSample;
    size_t bufferSize;
    PulseAudio_Callback callback;
    void* userData;
} PulseAudio_Capture;

// Initialize audio capture
// Returns: 0 on success, -1 on failure
int PulseAudio_Capture_Initialize(PulseAudio_Capture* capture);

// Set the audio callback function
void PulseAudio_Capture_SetCallback(
    PulseAudio_Capture* capture,
    PulseAudio_Callback callback,
    void* userData
);

// Start capturing audio
// Returns: 0 on success, -1 on failure
int PulseAudio_Capture_Start(PulseAudio_Capture* capture);

// Stop capturing audio
void PulseAudio_Capture_Stop(PulseAudio_Capture* capture);

// Cleanup and release resources
void PulseAudio_Capture_Cleanup(PulseAudio_Capture* capture);

// Get current audio format information
void PulseAudio_Capture_GetFormat(
    PulseAudio_Capture* capture,
    uint32_t* sampleRate,
    uint16_t* channels,
    uint16_t* bitsPerSample
);

#ifdef __cplusplus
}
#endif

#endif // __linux__ && !_FREEBSD
#endif // PULSEAUDIO_CAPTURE_H
