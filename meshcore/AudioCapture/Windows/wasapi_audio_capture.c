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

#ifdef _WIN32

#include "wasapi_audio_capture.h"
#include <stdio.h>

// Capture thread procedure
static DWORD WINAPI CaptureThreadProc(LPVOID param)
{
    WASAPI_AudioCapture* capture = (WASAPI_AudioCapture*)param;
    HRESULT hr;
    BYTE* audioData = NULL;
    UINT32 packetLength = 0;
    UINT32 numFramesAvailable = 0;
    DWORD flags = 0;

    // Set thread priority to time-critical for low latency
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

    while (capture->isCapturing) {
        // Wait for stop event or timeout (check every 10ms)
        if (WaitForSingleObject(capture->stopEvent, 10) == WAIT_OBJECT_0) {
            break;
        }

        // Get next packet size
        hr = capture->captureClient->lpVtbl->GetNextPacketSize(
            capture->captureClient,
            &packetLength
        );
        if (FAILED(hr)) {
            break;
        }

        while (packetLength != 0) {
            // Get captured buffer
            hr = capture->captureClient->lpVtbl->GetBuffer(
                capture->captureClient,
                &audioData,
                &numFramesAvailable,
                &flags,
                NULL,
                NULL
            );

            if (FAILED(hr)) {
                break;
            }

            // Check if buffer is silent
            BOOL isSilent = (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0;

            if (!isSilent && numFramesAvailable > 0 && capture->callback != NULL) {
                // Calculate buffer size in bytes
                UINT32 bufferSize = numFramesAvailable * capture->mixFormat->nBlockAlign;

                // Call user callback with audio data
                capture->callback(
                    audioData,
                    bufferSize,
                    capture->mixFormat->nSamplesPerSec,
                    capture->mixFormat->nChannels,
                    capture->mixFormat->wBitsPerSample,
                    capture->userData
                );
            }

            // Release buffer
            hr = capture->captureClient->lpVtbl->ReleaseBuffer(
                capture->captureClient,
                numFramesAvailable
            );
            if (FAILED(hr)) {
                break;
            }

            // Get next packet size
            hr = capture->captureClient->lpVtbl->GetNextPacketSize(
                capture->captureClient,
                &packetLength
            );
            if (FAILED(hr)) {
                break;
            }
        }
    }

    return 0;
}

HRESULT WASAPI_AudioCapture_Initialize(WASAPI_AudioCapture* capture)
{
    HRESULT hr;

    if (capture == NULL) {
        return E_POINTER;
    }

    // Zero out the structure
    ZeroMemory(capture, sizeof(WASAPI_AudioCapture));

    // Initialize COM
    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        // RPC_E_CHANGED_MODE means COM is already initialized, which is OK
        return hr;
    }

    // Create device enumerator
    hr = CoCreateInstance(
        &CLSID_MMDeviceEnumerator,
        NULL,
        CLSCTX_ALL,
        &IID_IMMDeviceEnumerator,
        (void**)&capture->deviceEnumerator
    );
    if (FAILED(hr)) {
        fprintf(stderr, "Failed to create device enumerator: 0x%08X\n", hr);
        return hr;
    }

    // Get default audio endpoint for loopback
    hr = capture->deviceEnumerator->lpVtbl->GetDefaultAudioEndpoint(
        capture->deviceEnumerator,
        eRender,  // Render device (speakers/headphones)
        eConsole, // Console role
        &capture->audioDevice
    );
    if (FAILED(hr)) {
        fprintf(stderr, "Failed to get default audio endpoint: 0x%08X\n", hr);
        return hr;
    }

    // Activate audio client
    hr = capture->audioDevice->lpVtbl->Activate(
        capture->audioDevice,
        &IID_IAudioClient,
        CLSCTX_ALL,
        NULL,
        (void**)&capture->audioClient
    );
    if (FAILED(hr)) {
        fprintf(stderr, "Failed to activate audio client: 0x%08X\n", hr);
        return hr;
    }

    // Get mix format
    hr = capture->audioClient->lpVtbl->GetMixFormat(
        capture->audioClient,
        &capture->mixFormat
    );
    if (FAILED(hr)) {
        fprintf(stderr, "Failed to get mix format: 0x%08X\n", hr);
        return hr;
    }

    // Initialize audio client in LOOPBACK mode
    hr = capture->audioClient->lpVtbl->Initialize(
        capture->audioClient,
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK,
        10000000,  // 1 second buffer
        0,         // Not used for shared mode
        capture->mixFormat,
        NULL
    );
    if (FAILED(hr)) {
        fprintf(stderr, "Failed to initialize audio client: 0x%08X\n", hr);
        return hr;
    }

    // Get capture client
    hr = capture->audioClient->lpVtbl->GetService(
        capture->audioClient,
        &IID_IAudioCaptureClient,
        (void**)&capture->captureClient
    );
    if (FAILED(hr)) {
        fprintf(stderr, "Failed to get capture client: 0x%08X\n", hr);
        return hr;
    }

    // Create stop event
    capture->stopEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (capture->stopEvent == NULL) {
        fprintf(stderr, "Failed to create stop event\n");
        return E_FAIL;
    }

    capture->isInitialized = TRUE;
    return S_OK;
}

void WASAPI_AudioCapture_SetCallback(
    WASAPI_AudioCapture* capture,
    AudioCaptureCallback callback,
    void* userData)
{
    if (capture == NULL) {
        return;
    }

    capture->callback = callback;
    capture->userData = userData;
}

HRESULT WASAPI_AudioCapture_Start(WASAPI_AudioCapture* capture)
{
    HRESULT hr;

    if (capture == NULL || !capture->isInitialized) {
        return E_FAIL;
    }

    if (capture->isCapturing) {
        return S_FALSE; // Already capturing
    }

    // Start audio client
    hr = capture->audioClient->lpVtbl->Start(capture->audioClient);
    if (FAILED(hr)) {
        fprintf(stderr, "Failed to start audio client: 0x%08X\n", hr);
        return hr;
    }

    capture->isCapturing = TRUE;

    // Create capture thread
    capture->captureThread = CreateThread(
        NULL,
        0,
        CaptureThreadProc,
        capture,
        0,
        NULL
    );

    if (capture->captureThread == NULL) {
        fprintf(stderr, "Failed to create capture thread\n");
        capture->audioClient->lpVtbl->Stop(capture->audioClient);
        capture->isCapturing = FALSE;
        return E_FAIL;
    }

    return S_OK;
}

void WASAPI_AudioCapture_Stop(WASAPI_AudioCapture* capture)
{
    if (capture == NULL || !capture->isCapturing) {
        return;
    }

    // Signal stop event
    capture->isCapturing = FALSE;
    if (capture->stopEvent) {
        SetEvent(capture->stopEvent);
    }

    // Wait for capture thread to finish
    if (capture->captureThread) {
        WaitForSingleObject(capture->captureThread, INFINITE);
        CloseHandle(capture->captureThread);
        capture->captureThread = NULL;
    }

    // Stop audio client
    if (capture->audioClient) {
        capture->audioClient->lpVtbl->Stop(capture->audioClient);
    }
}

void WASAPI_AudioCapture_Cleanup(WASAPI_AudioCapture* capture)
{
    if (capture == NULL) {
        return;
    }

    // Stop capturing if still active
    WASAPI_AudioCapture_Stop(capture);

    // Release COM interfaces
    if (capture->captureClient) {
        capture->captureClient->lpVtbl->Release(capture->captureClient);
        capture->captureClient = NULL;
    }

    if (capture->audioClient) {
        capture->audioClient->lpVtbl->Release(capture->audioClient);
        capture->audioClient = NULL;
    }

    if (capture->audioDevice) {
        capture->audioDevice->lpVtbl->Release(capture->audioDevice);
        capture->audioDevice = NULL;
    }

    if (capture->deviceEnumerator) {
        capture->deviceEnumerator->lpVtbl->Release(capture->deviceEnumerator);
        capture->deviceEnumerator = NULL;
    }

    if (capture->mixFormat) {
        CoTaskMemFree(capture->mixFormat);
        capture->mixFormat = NULL;
    }

    if (capture->stopEvent) {
        CloseHandle(capture->stopEvent);
        capture->stopEvent = NULL;
    }

    capture->isInitialized = FALSE;

    // Note: We don't call CoUninitialize() here as COM might be used elsewhere
}

void WASAPI_AudioCapture_GetFormat(
    WASAPI_AudioCapture* capture,
    UINT32* sampleRate,
    UINT16* channels,
    UINT16* bitsPerSample)
{
    if (capture == NULL || !capture->isInitialized || capture->mixFormat == NULL) {
        if (sampleRate) *sampleRate = 0;
        if (channels) *channels = 0;
        if (bitsPerSample) *bitsPerSample = 0;
        return;
    }

    if (sampleRate) {
        *sampleRate = capture->mixFormat->nSamplesPerSec;
    }
    if (channels) {
        *channels = capture->mixFormat->nChannels;
    }
    if (bitsPerSample) {
        *bitsPerSample = capture->mixFormat->wBitsPerSample;
    }
}

#endif // _WIN32
