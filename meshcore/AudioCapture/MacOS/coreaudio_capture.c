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

#ifdef __APPLE__

#include "coreaudio_capture.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Audio input callback (called by Core Audio)
static OSStatus InputCallback(
    void* inRefCon,
    AudioUnitRenderActionFlags* ioActionFlags,
    const AudioTimeStamp* inTimeStamp,
    UInt32 inBusNumber,
    UInt32 inNumberFrames,
    AudioBufferList* ioData)
{
    CoreAudio_Capture* capture = (CoreAudio_Capture*)inRefCon;
    OSStatus status;

    // Allocate buffer list for captured audio (float32 format)
    size_t bufferSizeFloat = inNumberFrames * capture->channels * sizeof(Float32);
    Float32* floatBuffer = (Float32*)malloc(bufferSizeFloat);
    if (floatBuffer == NULL) {
        return noErr; // Continue capture even if allocation fails
    }

    AudioBufferList bufferList;
    bufferList.mNumberBuffers = 1;
    bufferList.mBuffers[0].mNumberChannels = capture->channels;
    bufferList.mBuffers[0].mDataByteSize = bufferSizeFloat;
    bufferList.mBuffers[0].mData = floatBuffer;

    // Render audio (capture from input)
    status = AudioUnitRender(
        capture->audioUnit,
        ioActionFlags,
        inTimeStamp,
        inBusNumber,
        inNumberFrames,
        &bufferList
    );

    if (status == noErr && capture->callback != NULL) {
        // Convert float32 to int16
        size_t bufferSizeInt16 = inNumberFrames * capture->channels * sizeof(int16_t);
        int16_t* int16Buffer = (int16_t*)malloc(bufferSizeInt16);
        if (int16Buffer != NULL) {
            // Convert samples
            size_t numSamples = inNumberFrames * capture->channels;
            for (size_t i = 0; i < numSamples; i++) {
                // Clamp and convert float [-1.0, 1.0] to int16 [-32768, 32767]
                Float32 sample = floatBuffer[i];
                if (sample > 1.0f) sample = 1.0f;
                if (sample < -1.0f) sample = -1.0f;
                int16Buffer[i] = (int16_t)(sample * 32767.0f);
            }

            // Call user callback with converted data
            capture->callback(
                int16Buffer,
                bufferSizeInt16,
                capture->sampleRate,
                capture->channels,
                capture->bitsPerSample,
                capture->userData
            );

            free(int16Buffer);
        }
    }

    free(floatBuffer);
    return status;
}

OSStatus CoreAudio_Capture_Initialize(CoreAudio_Capture* capture)
{
    OSStatus status;

    if (capture == NULL) {
        return paramErr;
    }

    // Zero out the structure
    memset(capture, 0, sizeof(CoreAudio_Capture));

    // Set audio format parameters
    capture->sampleRate = 48000;
    capture->channels = 2;
    capture->bitsPerSample = 16;

    // Describe the component we want (HAL output for loopback)
    AudioComponentDescription desc;
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_HALOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;

    // Find the component
    AudioComponent component = AudioComponentFindNext(NULL, &desc);
    if (component == NULL) {
        fprintf(stderr, "Could not find HAL output component\n");
        return kAudioUnitErr_NoConnection;
    }

    // Create audio unit instance
    status = AudioComponentInstanceNew(component, &capture->audioUnit);
    if (status != noErr) {
        fprintf(stderr, "Failed to create audio unit instance: %d\n", (int)status);
        return status;
    }

    // Enable input on the HAL output unit (for loopback)
    UInt32 enableIO = 1;
    status = AudioUnitSetProperty(
        capture->audioUnit,
        kAudioOutputUnitProperty_EnableIO,
        kAudioUnitScope_Input,
        1,  // Input element
        &enableIO,
        sizeof(enableIO)
    );
    if (status != noErr) {
        fprintf(stderr, "Failed to enable IO: %d\n", (int)status);
        return status;
    }

    // Disable output on output scope
    enableIO = 0;
    status = AudioUnitSetProperty(
        capture->audioUnit,
        kAudioOutputUnitProperty_EnableIO,
        kAudioUnitScope_Output,
        0,  // Output element
        &enableIO,
        sizeof(enableIO)
    );

    // Get default output device
    AudioDeviceID defaultDevice;
    UInt32 size = sizeof(defaultDevice);
    AudioObjectPropertyAddress propertyAddress = {
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };

    status = AudioObjectGetPropertyData(
        kAudioObjectSystemObject,
        &propertyAddress,
        0,
        NULL,
        &size,
        &defaultDevice
    );
    if (status != noErr) {
        fprintf(stderr, "Failed to get default output device: %d\n", (int)status);
        return status;
    }

    // Set the audio unit to use default output device
    status = AudioUnitSetProperty(
        capture->audioUnit,
        kAudioOutputUnitProperty_CurrentDevice,
        kAudioUnitScope_Global,
        0,
        &defaultDevice,
        sizeof(defaultDevice)
    );
    if (status != noErr) {
        fprintf(stderr, "Failed to set current device: %d\n", (int)status);
        return status;
    }

    // Set audio format (48kHz, stereo, float32)
    AudioStreamBasicDescription format;
    format.mSampleRate = capture->sampleRate;
    format.mFormatID = kAudioFormatLinearPCM;
    format.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    format.mBytesPerPacket = 8;    // 2 channels * 4 bytes (float32)
    format.mFramesPerPacket = 1;
    format.mBytesPerFrame = 8;
    format.mChannelsPerFrame = capture->channels;
    format.mBitsPerChannel = 32;   // float32

    status = AudioUnitSetProperty(
        capture->audioUnit,
        kAudioUnitProperty_StreamFormat,
        kAudioUnitScope_Output,
        1,  // Input element
        &format,
        sizeof(format)
    );
    if (status != noErr) {
        fprintf(stderr, "Failed to set stream format: %d\n", (int)status);
        return status;
    }

    // Set input callback
    AURenderCallbackStruct callbackStruct;
    callbackStruct.inputProc = InputCallback;
    callbackStruct.inputProcRefCon = capture;

    status = AudioUnitSetProperty(
        capture->audioUnit,
        kAudioOutputUnitProperty_SetInputCallback,
        kAudioUnitScope_Global,
        0,
        &callbackStruct,
        sizeof(callbackStruct)
    );
    if (status != noErr) {
        fprintf(stderr, "Failed to set input callback: %d\n", (int)status);
        return status;
    }

    // Initialize the audio unit
    status = AudioUnitInitialize(capture->audioUnit);
    if (status != noErr) {
        fprintf(stderr, "Failed to initialize audio unit: %d\n", (int)status);
        return status;
    }

    capture->isInitialized = 1;
    return noErr;
}

void CoreAudio_Capture_SetCallback(
    CoreAudio_Capture* capture,
    CoreAudio_Callback callback,
    void* userData)
{
    if (capture == NULL) {
        return;
    }

    capture->callback = callback;
    capture->userData = userData;
}

OSStatus CoreAudio_Capture_Start(CoreAudio_Capture* capture)
{
    if (capture == NULL || !capture->isInitialized) {
        return kAudioUnitErr_Uninitialized;
    }

    if (capture->isCapturing) {
        return noErr; // Already capturing
    }

    OSStatus status = AudioOutputUnitStart(capture->audioUnit);
    if (status != noErr) {
        fprintf(stderr, "Failed to start audio unit: %d\n", (int)status);
        return status;
    }

    capture->isCapturing = 1;
    return noErr;
}

void CoreAudio_Capture_Stop(CoreAudio_Capture* capture)
{
    if (capture == NULL || !capture->isCapturing) {
        return;
    }

    AudioOutputUnitStop(capture->audioUnit);
    capture->isCapturing = 0;
}

void CoreAudio_Capture_Cleanup(CoreAudio_Capture* capture)
{
    if (capture == NULL) {
        return;
    }

    // Stop capturing if still active
    CoreAudio_Capture_Stop(capture);

    // Cleanup audio unit
    if (capture->audioUnit && capture->isInitialized) {
        AudioUnitUninitialize(capture->audioUnit);
        AudioComponentInstanceDispose(capture->audioUnit);
        capture->audioUnit = NULL;
    }

    capture->isInitialized = 0;
}

void CoreAudio_Capture_GetFormat(
    CoreAudio_Capture* capture,
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

#endif // __APPLE__
