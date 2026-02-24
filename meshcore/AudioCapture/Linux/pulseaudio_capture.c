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

#if defined(__linux__) && !defined(_FREEBSD)

#include "pulseaudio_capture.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Capture thread procedure
static void* CaptureThreadProc(void* param)
{
    PulseAudio_Capture* capture = (PulseAudio_Capture*)param;
    uint8_t* buffer = malloc(capture->bufferSize);
    int error;

    if (buffer == NULL) {
        fprintf(stderr, "Failed to allocate audio buffer\n");
        return NULL;
    }

    while (capture->isCapturing) {
        // Read audio data (blocking call)
        if (pa_simple_read(capture->pulseAudio, buffer, capture->bufferSize, &error) < 0) {
            fprintf(stderr, "PulseAudio read failed: %s\n", pa_strerror(error));
            break;
        }

        // Call user callback with audio data
        if (capture->callback != NULL) {
            capture->callback(
                buffer,
                capture->bufferSize,
                capture->sampleRate,
                capture->channels,
                capture->bitsPerSample,
                capture->userData
            );
        }
    }

    free(buffer);
    return NULL;
}

int PulseAudio_Capture_Initialize(PulseAudio_Capture* capture)
{
    if (capture == NULL) {
        return -1;
    }

    // Zero out the structure
    memset(capture, 0, sizeof(PulseAudio_Capture));

    // Set audio format parameters
    capture->sampleRate = 48000;      // 48kHz sample rate
    capture->channels = 2;             // Stereo
    capture->bitsPerSample = 16;       // 16-bit samples

    // Calculate buffer size for 20ms of audio
    // Formula: (sampleRate * duration_seconds * channels * bytesPerSample)
    capture->bufferSize = (capture->sampleRate * 20 / 1000) * capture->channels * (capture->bitsPerSample / 8);

    // Configure PulseAudio sample spec
    pa_sample_spec spec;
    spec.format = PA_SAMPLE_S16LE;     // 16-bit signed little-endian
    spec.rate = capture->sampleRate;    // 48kHz sample rate
    spec.channels = capture->channels;  // Stereo

    // Configure buffer attributes
    pa_buffer_attr bufferAttr;
    bufferAttr.maxlength = (uint32_t) -1;  // Default
    bufferAttr.fragsize = capture->bufferSize;  // 20ms fragments

    int error;

    // Create a new simple recording stream
    // Note: Using NULL for device will use the default monitor source
    // To explicitly use a monitor source, query PulseAudio for the monitor
    // of the default sink (e.g., "alsa_output.pci-0000_00_1f.3.analog-stereo.monitor")
    capture->pulseAudio = pa_simple_new(
        NULL,                        // Use default server
        "MeshCentral Agent",         // Application name
        PA_STREAM_RECORD,            // Record stream
        NULL,                        // Use default monitor device
        "Desktop Audio Capture",     // Stream description
        &spec,                       // Sample format
        NULL,                        // Use default channel map
        &bufferAttr,                 // Buffer attributes
        &error                       // Error code
    );

    if (capture->pulseAudio == NULL) {
        fprintf(stderr, "PulseAudio initialization failed: %s\n", pa_strerror(error));
        fprintf(stderr, "Note: To capture system audio, you may need to:\n");
        fprintf(stderr, "  1. Ensure PulseAudio is running\n");
        fprintf(stderr, "  2. Set the monitor source as default:\n");
        fprintf(stderr, "     pactl load-module module-loopback\n");
        fprintf(stderr, "  3. Or specify the monitor source explicitly\n");
        return -1;
    }

    capture->isInitialized = 1;
    return 0;
}

void PulseAudio_Capture_SetCallback(
    PulseAudio_Capture* capture,
    PulseAudio_Callback callback,
    void* userData)
{
    if (capture == NULL) {
        return;
    }

    capture->callback = callback;
    capture->userData = userData;
}

int PulseAudio_Capture_Start(PulseAudio_Capture* capture)
{
    if (capture == NULL || !capture->isInitialized) {
        return -1;
    }

    if (capture->isCapturing) {
        return 0; // Already capturing
    }

    capture->isCapturing = 1;

    // Create capture thread
    int result = pthread_create(&capture->captureThread, NULL, CaptureThreadProc, capture);
    if (result != 0) {
        fprintf(stderr, "Failed to create capture thread: %d\n", result);
        capture->isCapturing = 0;
        return -1;
    }

    return 0;
}

void PulseAudio_Capture_Stop(PulseAudio_Capture* capture)
{
    if (capture == NULL || !capture->isCapturing) {
        return;
    }

    // Signal stop
    capture->isCapturing = 0;

    // Wait for capture thread to finish
    if (capture->captureThread) {
        pthread_join(capture->captureThread, NULL);
        capture->captureThread = 0;
    }
}

void PulseAudio_Capture_Cleanup(PulseAudio_Capture* capture)
{
    if (capture == NULL) {
        return;
    }

    // Stop capturing if still active
    PulseAudio_Capture_Stop(capture);

    // Free PulseAudio resources
    if (capture->pulseAudio) {
        pa_simple_free(capture->pulseAudio);
        capture->pulseAudio = NULL;
    }

    capture->isInitialized = 0;
}

void PulseAudio_Capture_GetFormat(
    PulseAudio_Capture* capture,
    uint32_t* sampleRate,
    uint16_t* channels,
    uint16_t* bitsPerSample)
{
    if (capture == NULL || !capture->isInitialized) {
        if (sampleRate) *sampleRate = 0;
        if (channels) *channels = 0;
        if (bitsPerSample) *bitsPerSample = 0;
        return;
    }

    if (sampleRate) {
        *sampleRate = capture->sampleRate;
    }
    if (channels) {
        *channels = capture->channels;
    }
    if (bitsPerSample) {
        *bitsPerSample = capture->bitsPerSample;
    }
}

#endif // __linux__ && !_FREEBSD
