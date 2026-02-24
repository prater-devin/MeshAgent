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

#ifndef COREAUDIO_CAPTURE_H
#define COREAUDIO_CAPTURE_H

#ifdef __APPLE__

#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Audio callback function type
// Called when audio data is captured
// Parameters:
//   - audioData: PCM audio data buffer (int16_t format)
//   - dataSize: Size of audio data in bytes
//   - sampleRate: Sample rate in Hz (e.g., 48000)
//   - channels: Number of audio channels (1=mono, 2=stereo)
//   - bitsPerSample: Bits per sample (16)
//   - userData: User-provided context pointer
typedef void (*CoreAudio_Callback)(
    const int16_t* audioData,
    size_t dataSize,
    uint32_t sampleRate,
    uint16_t channels,
    uint16_t bitsPerSample,
    void* userData
);

// Audio capture context
typedef struct CoreAudio_Capture {
    AudioComponentInstance audioUnit;
    int isCapturing;
    int isInitialized;
    uint32_t sampleRate;
    uint16_t channels;
    uint16_t bitsPerSample;
    CoreAudio_Callback callback;
    void* userData;
} CoreAudio_Capture;

// Initialize audio capture
// Returns: noErr (0) on success, error code on failure
OSStatus CoreAudio_Capture_Initialize(CoreAudio_Capture* capture);

// Set the audio callback function
void CoreAudio_Capture_SetCallback(
    CoreAudio_Capture* capture,
    CoreAudio_Callback callback,
    void* userData
);

// Start capturing audio
// Returns: noErr (0) on success, error code on failure
OSStatus CoreAudio_Capture_Start(CoreAudio_Capture* capture);

// Stop capturing audio
void CoreAudio_Capture_Stop(CoreAudio_Capture* capture);

// Cleanup and release resources
void CoreAudio_Capture_Cleanup(CoreAudio_Capture* capture);

// Get current audio format information
void CoreAudio_Capture_GetFormat(
    CoreAudio_Capture* capture,
    uint32_t* sampleRate,
    uint16_t* channels,
    uint16_t* bitsPerSample
);

#ifdef __cplusplus
}
#endif

#endif // __APPLE__
#endif // COREAUDIO_CAPTURE_H
