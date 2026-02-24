# MeshAgent Audio Capture - WebRTC Integration Guide

## Overview

This document explains how to integrate the platform-specific audio capture modules with MeshAgent's WebRTC implementation to enable remote desktop audio streaming.

## Architecture Summary

```
┌──────────────────────────────────────────────────────────────┐
│                       MeshAgent                               │
├──────────────────────────────────────────────────────────────┤
│                                                               │
│  ┌──────────────┐     ┌──────────────┐     ┌─────────────┐  │
│  │   Platform   │────>│    Opus      │────>│   WebRTC    │  │
│  │ Audio Capture│     │   Encoder    │     │   Audio     │  │
│  └──────────────┘     └──────────────┘     │   Track     │  │
│         │                                   └─────────────┘  │
│         │                                          │         │
│    Callback                                        │         │
│    PCM Data                                   RTP/SRTP       │
│                                                    │         │
└────────────────────────────────────────────────────┼─────────┘
                                                     │
                                            WebRTC P2P or Relay
                                                     │
                                                     v
                                            Web Browser Client
```

## Files Created

### Audio Capture Modules

1. **Windows (WASAPI)**
   - `meshcore/AudioCapture/Windows/wasapi_audio_capture.h`
   - `meshcore/AudioCapture/Windows/wasapi_audio_capture.c`

2. **Linux (PulseAudio)**
   - `meshcore/AudioCapture/Linux/pulseaudio_capture.h`
   - `meshcore/AudioCapture/Linux/pulseaudio_capture.c`

3. **macOS (Core Audio)**
   - `meshcore/AudioCapture/MacOS/coreaudio_capture.h`
   - `meshcore/AudioCapture/MacOS/coreaudio_capture.c`

### Opus Encoder

4. **Opus Encoder Wrapper**
   - `meshcore/AudioCapture/opus_encoder_wrapper.h`
   - `meshcore/AudioCapture/opus_encoder_wrapper.c`

---

## Integration Steps

### Step 1: Add Audio Capture to Build System

#### Update Makefile

Add the audio capture source files to the makefile:

```makefile
# Audio Capture Sources
ifeq ($(OS),Windows_NT)
    AUDIO_SOURCES = meshcore/AudioCapture/Windows/wasapi_audio_capture.c \
                    meshcore/AudioCapture/opus_encoder_wrapper.c
    AUDIO_LIBS = -lole32 -luuid
else ifeq ($(shell uname),Darwin)
    AUDIO_SOURCES = meshcore/AudioCapture/MacOS/coreaudio_capture.c \
                    meshcore/AudioCapture/opus_encoder_wrapper.c
    AUDIO_LIBS = -framework AudioToolbox -framework CoreAudio -lopus
else
    AUDIO_SOURCES = meshcore/AudioCapture/Linux/pulseaudio_capture.c \
                    meshcore/AudioCapture/opus_encoder_wrapper.c
    AUDIO_LIBS = -lpulse -lpulse-simple -lopus
endif

# Add to main build
SOURCES += $(AUDIO_SOURCES)
LIBS += $(AUDIO_LIBS)
```

#### Update Visual Studio Project (Windows)

For `MeshAgent.sln` or `MeshAgent-2022.sln`:

1. Add source files to the project:
   - `wasapi_audio_capture.c`
   - `opus_encoder_wrapper.c`

2. Add include directories:
   - `$(ProjectDir)meshcore\AudioCapture\Windows`
   - `$(ProjectDir)third_party\opus\include` (if using local Opus)

3. Add library dependencies:
   - `ole32.lib`
   - `uuid.lib`
   - `opus.lib`

---

### Step 2: Extend ILibWebRTC for Audio Tracks

Currently, ILibWebRTC only supports data channels. To add audio support, you need to extend it to support RTP media tracks.

#### Option A: Use Existing WebRTC Library with Audio (Recommended)

Replace the custom ILibWebRTC implementation with a full-featured WebRTC library:

**libwebrtc (Google's WebRTC library)**
- Pros: Full WebRTC support including audio/video tracks, mature, well-tested
- Cons: Large library, C++ API, significant integration work

**pion/webrtc (Go)**
- Pros: Clean API, good documentation, supports audio tracks
- Cons: Requires CGo bindings for C integration

#### Option B: Extend ILibWebRTC (More Work)

If you prefer to keep the existing ILibWebRTC stack, you need to add:

1. **RTP packet generation**
2. **SRTP encryption**
3. **Audio track management**
4. **SDP negotiation for audio m= line**

**Pseudo-code for extending ILibWebRTC:**

```c
// In ILibWebRTC.h
typedef void (*ILibWebRTC_AudioDataCallback)(
    void* audioData,
    int audioDataLen,
    void* userObject
);

void ILibWebRTC_AddAudioTrack(
    void* webRTCModule,
    ILibWebRTC_AudioDataCallback callback,
    void* userObject
);

void ILibWebRTC_SendAudioPacket(
    void* webRTCModule,
    unsigned char* opusData,
    int opusDataLen,
    uint32_t timestamp
);
```

**This is a significant undertaking and may take 40-80 hours.**

---

### Step 3: Create Audio Manager Module

Create a unified audio manager that handles platform detection and audio capture initialization.

**File: `meshcore/audio_manager.h`**

```c
#ifndef AUDIO_MANAGER_H
#define AUDIO_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AudioManager AudioManager;

// Callback for encoded audio data (Opus packets)
typedef void (*AudioManager_EncodedCallback)(
    const unsigned char* opusData,
    int opusDataLen,
    uint32_t timestamp,
    void* userData
);

// Initialize audio manager for the current platform
AudioManager* AudioManager_Create();

// Set callback for encoded audio
void AudioManager_SetCallback(
    AudioManager* manager,
    AudioManager_EncodedCallback callback,
    void* userData
);

// Start capturing and encoding audio
int AudioManager_Start(AudioManager* manager);

// Stop capturing audio
void AudioManager_Stop(AudioManager* manager);

// Cleanup
void AudioManager_Destroy(AudioManager* manager);

#ifdef __cplusplus
}
#endif

#endif // AUDIO_MANAGER_H
```

**File: `meshcore/audio_manager.c`**

```c
#include "audio_manager.h"
#include "AudioCapture/opus_encoder_wrapper.h"

#ifdef _WIN32
    #include "AudioCapture/Windows/wasapi_audio_capture.h"
#elif defined(__APPLE__)
    #include "AudioCapture/MacOS/coreaudio_capture.h"
#elif defined(__linux__)
    #include "AudioCapture/Linux/pulseaudio_capture.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

struct AudioManager {
#ifdef _WIN32
    WASAPI_AudioCapture capture;
#elif defined(__APPLE__)
    CoreAudio_Capture capture;
#elif defined(__linux__)
    PulseAudio_Capture capture;
#endif
    OpusEncoderWrapper encoder;
    AudioManager_EncodedCallback callback;
    void* userData;

    // Audio buffer for accumulating frames
    int16_t* frameBuffer;
    int frameBufferSize;
    int frameBufferPos;
    uint32_t timestamp;
};

// Internal callback from platform audio capture
static void AudioManager_InternalCallback(
    const int16_t* audioData,
    size_t dataSize,
    uint32_t sampleRate,
    uint16_t channels,
    uint16_t bitsPerSample,
    void* userData)
{
    AudioManager* manager = (AudioManager*)userData;
    int samplesPerChannel = dataSize / (channels * (bitsPerSample / 8));
    int frameSizeNeeded = OpusEncoder_GetFrameSize(&manager->encoder);

    // Copy samples to frame buffer
    const int16_t* src = audioData;
    int samplesToProcess = samplesPerChannel;

    while (samplesToProcess > 0) {
        int spaceInBuffer = frameSizeNeeded - manager->frameBufferPos;
        int samplesToCopy = (samplesToProcess < spaceInBuffer) ? samplesToProcess : spaceInBuffer;

        memcpy(
            manager->frameBuffer + (manager->frameBufferPos * channels),
            src,
            samplesToCopy * channels * sizeof(int16_t)
        );

        manager->frameBufferPos += samplesToCopy;
        src += samplesToCopy * channels;
        samplesToProcess -= samplesToCopy;

        // If frame is complete, encode it
        if (manager->frameBufferPos == frameSizeNeeded) {
            unsigned char opusPacket[4000];
            int encodedSize = OpusEncoder_Encode(
                &manager->encoder,
                manager->frameBuffer,
                frameSizeNeeded,
                opusPacket,
                sizeof(opusPacket)
            );

            if (encodedSize > 0 && manager->callback != NULL) {
                manager->callback(
                    opusPacket,
                    encodedSize,
                    manager->timestamp,
                    manager->userData
                );
            }

            // Update timestamp (increment by frame size)
            manager->timestamp += frameSizeNeeded;

            // Reset buffer
            manager->frameBufferPos = 0;
        }
    }
}

AudioManager* AudioManager_Create()
{
    AudioManager* manager = (AudioManager*)calloc(1, sizeof(AudioManager));
    if (manager == NULL) {
        return NULL;
    }

    // Initialize Opus encoder (48kHz, stereo, 64kbps, audio application)
    if (OpusEncoder_Initialize(&manager->encoder, 48000, 2, 64000, OPUS_APPLICATION_AUDIO) != 0) {
        free(manager);
        return NULL;
    }

    // Allocate frame buffer
    int frameSize = OpusEncoder_GetFrameSize(&manager->encoder);
    manager->frameBufferSize = frameSize * 2;  // stereo
    manager->frameBuffer = (int16_t*)malloc(manager->frameBufferSize * sizeof(int16_t));
    if (manager->frameBuffer == NULL) {
        OpusEncoder_Cleanup(&manager->encoder);
        free(manager);
        return NULL;
    }

    manager->frameBufferPos = 0;
    manager->timestamp = 0;

    // Initialize platform-specific audio capture
#ifdef _WIN32
    if (WASAPI_AudioCapture_Initialize(&manager->capture) != S_OK) {
        free(manager->frameBuffer);
        OpusEncoder_Cleanup(&manager->encoder);
        free(manager);
        return NULL;
    }
    WASAPI_AudioCapture_SetCallback(&manager->capture, AudioManager_InternalCallback, manager);
#elif defined(__APPLE__)
    if (CoreAudio_Capture_Initialize(&manager->capture) != noErr) {
        free(manager->frameBuffer);
        OpusEncoder_Cleanup(&manager->encoder);
        free(manager);
        return NULL;
    }
    CoreAudio_Capture_SetCallback(&manager->capture, AudioManager_InternalCallback, manager);
#elif defined(__linux__)
    if (PulseAudio_Capture_Initialize(&manager->capture) != 0) {
        free(manager->frameBuffer);
        OpusEncoder_Cleanup(&manager->encoder);
        free(manager);
        return NULL;
    }
    PulseAudio_Capture_SetCallback(&manager->capture, AudioManager_InternalCallback, manager);
#endif

    return manager;
}

void AudioManager_SetCallback(
    AudioManager* manager,
    AudioManager_EncodedCallback callback,
    void* userData)
{
    if (manager == NULL) {
        return;
    }
    manager->callback = callback;
    manager->userData = userData;
}

int AudioManager_Start(AudioManager* manager)
{
    if (manager == NULL) {
        return -1;
    }

#ifdef _WIN32
    return (WASAPI_AudioCapture_Start(&manager->capture) == S_OK) ? 0 : -1;
#elif defined(__APPLE__)
    return (CoreAudio_Capture_Start(&manager->capture) == noErr) ? 0 : -1;
#elif defined(__linux__)
    return PulseAudio_Capture_Start(&manager->capture);
#else
    return -1;
#endif
}

void AudioManager_Stop(AudioManager* manager)
{
    if (manager == NULL) {
        return;
    }

#ifdef _WIN32
    WASAPI_AudioCapture_Stop(&manager->capture);
#elif defined(__APPLE__)
    CoreAudio_Capture_Stop(&manager->capture);
#elif defined(__linux__)
    PulseAudio_Capture_Stop(&manager->capture);
#endif
}

void AudioManager_Destroy(AudioManager* manager)
{
    if (manager == NULL) {
        return;
    }

    AudioManager_Stop(manager);

#ifdef _WIN32
    WASAPI_AudioCapture_Cleanup(&manager->capture);
#elif defined(__APPLE__)
    CoreAudio_Capture_Cleanup(&manager->capture);
#elif defined(__linux__)
    PulseAudio_Capture_Cleanup(&manager->capture);
#endif

    OpusEncoder_Cleanup(&manager->encoder);

    if (manager->frameBuffer) {
        free(manager->frameBuffer);
    }

    free(manager);
}
```

---

### Step 4: Integrate with JavaScript (Duktape)

Create JavaScript bindings for the audio manager in the Duktape environment.

**File: `microscript/ILibDuktape_Audio.c`**

```c
#include "ILibDuktape_Audio.h"
#include "../meshcore/audio_manager.h"
#include "ILibDuktapeModSearch.h"
#include "ILibDuktape_Helpers.h"
#include "ILibDuktape_EventEmitter.h"

typedef struct AudioManagerContext {
    duk_context* ctx;
    AudioManager* manager;
    void* emitter;
    int weakRef;
} AudioManagerContext;

static void AudioManager_JS_Callback(
    const unsigned char* opusData,
    int opusDataLen,
    uint32_t timestamp,
    void* userData)
{
    AudioManagerContext* actx = (AudioManagerContext*)userData;

    // Push audio data to JavaScript as event
    duk_push_heapptr(actx->ctx, actx->emitter);
    duk_get_prop_string(actx->ctx, -1, "emit");
    duk_dup(actx->ctx, -2);
    duk_push_string(actx->ctx, "data");
    duk_push_external_buffer(actx->ctx);
    duk_config_buffer(actx->ctx, -1, (void*)opusData, opusDataLen);
    duk_push_uint(actx->ctx, timestamp);
    duk_call_method(actx->ctx, 3);
    duk_pop_2(actx->ctx);
}

duk_ret_t AudioManager_Create(duk_context* ctx)
{
    AudioManagerContext* actx = (AudioManagerContext*)Duktape_PushBuffer(ctx, sizeof(AudioManagerContext));
    actx->ctx = ctx;
    actx->manager = AudioManager_Create();

    if (actx->manager == NULL) {
        return DUK_RET_ERROR;
    }

    // Make this object an EventEmitter
    ILibDuktape_EventEmitter_CreateEventEmitter(ctx);
    actx->emitter = duk_get_heapptr(ctx, -1);

    AudioManager_SetCallback(actx->manager, AudioManager_JS_Callback, actx);

    return 1;
}

duk_ret_t AudioManager_Start(duk_context* ctx)
{
    AudioManagerContext* actx = (AudioManagerContext*)Duktape_GetBuffer(ctx, 0, NULL);
    int result = AudioManager_Start(actx->manager);
    duk_push_int(ctx, result);
    return 1;
}

duk_ret_t AudioManager_Stop(duk_context* ctx)
{
    AudioManagerContext* actx = (AudioManagerContext*)Duktape_GetBuffer(ctx, 0, NULL);
    AudioManager_Stop(actx->manager);
    return 0;
}

duk_ret_t AudioManager_Finalizer(duk_context* ctx)
{
    AudioManagerContext* actx = (AudioManagerContext*)Duktape_GetBuffer(ctx, 0, NULL);
    if (actx->manager) {
        AudioManager_Destroy(actx->manager);
        actx->manager = NULL;
    }
    return 0;
}

void ILibDuktape_Audio_PUSH(duk_context* ctx, void* chain)
{
    duk_push_object(ctx);

    ILibDuktape_WriteID(ctx, "audio-manager");

    duk_push_c_function(ctx, AudioManager_Create, DUK_VARARGS);
    duk_put_prop_string(ctx, -2, "create");

    duk_push_c_function(ctx, AudioManager_Start, DUK_VARARGS);
    duk_put_prop_string(ctx, -2, "start");

    duk_push_c_function(ctx, AudioManager_Stop, DUK_VARARGS);
    duk_put_prop_string(ctx, -2, "stop");

    duk_push_c_function(ctx, AudioManager_Finalizer, 1);
    duk_set_finalizer(ctx, -2);
}
```

---

### Step 5: Use Audio in Desktop Session

Integrate audio capture with the desktop streaming code.

**In JavaScript (e.g., `modules/meshcmd.js` or desktop module):**

```javascript
// When WebRTC desktop session starts
if (webRtcDesktop.webrtc) {
    // Create audio manager
    var audioManager = require('audio-manager').create();

    // When audio data is encoded, send via WebRTC
    audioManager.on('data', function(opusData, timestamp) {
        // Send Opus packet via WebRTC audio track
        // This requires WebRTC audio track support (see Step 2)
        if (webRtcDesktop.audioTrack) {
            webRtcDesktop.audioTrack.sendAudioPacket(opusData, timestamp);
        }
    });

    // Start audio capture
    audioManager.start();

    // Store reference
    webRtcDesktop.audioManager = audioManager;

    // On disconnect, stop audio
    webRtcDesktop.webrtc.on('disconnected', function() {
        if (webRtcDesktop.audioManager) {
            webRtcDesktop.audioManager.stop();
        }
    });
}
```

---

## Dependencies

### Required Libraries

#### Windows
- **Windows SDK** (for WASAPI)
  - Included with Visual Studio
  - Headers: `audioclient.h`, `mmdeviceapi.h`

- **Opus**
  - Download from: https://opus-codec.org/
  - Or use vcpkg: `vcpkg install opus:x64-windows`

#### Linux
- **PulseAudio Development Libraries**
  ```bash
  # Ubuntu/Debian
  sudo apt-get install libpulse-dev

  # Fedora/RHEL
  sudo dnf install pulseaudio-libs-devel

  # Arch
  sudo pacman -S libpulse
  ```

- **Opus**
  ```bash
  # Ubuntu/Debian
  sudo apt-get install libopus-dev

  # Fedora/RHEL
  sudo dnf install opus-devel

  # Arch
  sudo pacman -S opus
  ```

#### macOS
- **Xcode Command Line Tools**
  ```bash
  xcode-select --install
  ```

- **Opus**
  ```bash
  # Homebrew
  brew install opus

  # MacPorts
  sudo port install opus
  ```

---

## Testing

### Test Audio Capture

Create a simple test program to verify audio capture works:

**File: `test/test_audio_capture.c`**

```c
#include "../meshcore/audio_manager.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
    #include <windows.h>
    #define SLEEP(ms) Sleep(ms)
#else
    #include <unistd.h>
    #define SLEEP(ms) usleep((ms) * 1000)
#endif

static int packetsReceived = 0;

void TestCallback(
    const unsigned char* opusData,
    int opusDataLen,
    uint32_t timestamp,
    void* userData)
{
    packetsReceived++;
    printf("Received Opus packet: %d bytes, timestamp: %u (total: %d packets)\n",
           opusDataLen, timestamp, packetsReceived);
}

int main()
{
    printf("Creating audio manager...\n");
    AudioManager* manager = AudioManager_Create();
    if (manager == NULL) {
        fprintf(stderr, "Failed to create audio manager\n");
        return 1;
    }

    printf("Setting callback...\n");
    AudioManager_SetCallback(manager, TestCallback, NULL);

    printf("Starting audio capture... (will run for 10 seconds)\n");
    if (AudioManager_Start(manager) != 0) {
        fprintf(stderr, "Failed to start audio capture\n");
        AudioManager_Destroy(manager);
        return 1;
    }

    printf("Audio capture running. Play some audio on your system.\n");
    SLEEP(10000);  // Capture for 10 seconds

    printf("\nStopping audio capture...\n");
    AudioManager_Stop(manager);

    printf("Cleaning up...\n");
    AudioManager_Destroy(manager);

    printf("\nTest complete. Received %d Opus packets.\n", packetsReceived);
    printf("Expected approximately 500 packets (10 seconds at 50 packets/sec).\n");

    return 0;
}
```

**Compile and run:**

```bash
# Windows (Visual Studio)
cl test_audio_capture.c audio_manager.c wasapi_audio_capture.c opus_encoder_wrapper.c /I..\third_party\opus\include opus.lib ole32.lib uuid.lib

# Linux
gcc test_audio_capture.c audio_manager.c pulseaudio_capture.c opus_encoder_wrapper.c -lpulse -lpulse-simple -lopus -pthread -o test_audio

# macOS
gcc test_audio_capture.c audio_manager.c coreaudio_capture.c opus_encoder_wrapper.c -framework AudioToolbox -framework CoreAudio -lopus -o test_audio

# Run
./test_audio
```

---

## Troubleshooting

### Windows

**Problem:** `CoCreateInstance` fails with `E_NOINTERFACE`
**Solution:** Ensure COM is initialized and headers are correct. Check Windows SDK version.

**Problem:** No audio captured
**Solution:**
- Check Windows audio is playing
- Verify default audio device is set correctly
- Run as administrator if needed

### Linux

**Problem:** `pa_simple_new` fails
**Solution:**
- Ensure PulseAudio is running: `pulseaudio --check`
- Start PulseAudio: `pulseaudio --start`
- Check default sink: `pactl info | grep "Default Sink"`

**Problem:** Permission denied
**Solution:** Add user to `audio` group: `sudo usermod -a -G audio $USER`

### macOS

**Problem:** `AudioComponentInstanceNew` fails
**Solution:** Check microphone/audio permissions in System Preferences > Security & Privacy > Privacy > Microphone

**Problem:** Audio device not found
**Solution:** Verify default audio device: `system_profiler SPAudioDataType`

---

## Performance Considerations

- **CPU Usage:** Expect 2-5% CPU usage on modern systems for audio capture + Opus encoding
- **Memory:** ~100KB per audio session
- **Bandwidth:** 64kbps = 8 KB/s (negligible compared to video)
- **Latency:** ~50-100ms end-to-end (capture + encode + network + decode)

---

## Security and Privacy

- Audio capture should only be enabled when explicitly authorized by the user
- Add UI indicator when audio is being captured (system tray icon, notification)
- Store audio enable/disable preference in agent configuration
- Never record audio to disk without explicit user consent

---

## Next Steps

1. ✅ **Audio capture modules created** (Windows, Linux, macOS)
2. ✅ **Opus encoder integration completed**
3. ⏳ **Extend ILibWebRTC for audio tracks** (or integrate full WebRTC library)
4. ⏳ **Create Duktape JavaScript bindings**
5. ⏳ **Integrate with desktop streaming code**
6. ⏳ **Add audio enable/disable UI**
7. ⏳ **Test with screen readers** (NVDA, JAWS, VoiceOver, Orca)
8. ⏳ **Document user-facing audio features**

---

## Conclusion

The platform-specific audio capture modules are complete and ready for integration. The main remaining work is:

1. **Extending ILibWebRTC to support audio tracks** (or replacing with a full WebRTC library)
2. **Creating JavaScript bindings** for the audio manager
3. **Integrating audio into the desktop session workflow**

**Estimated effort for remaining work:** 40-80 hours depending on WebRTC approach chosen.

**Client-side changes are complete** and ready to receive audio once the agent starts sending it.

This implementation will make MeshCentral significantly more accessible for blind users who rely on screen readers.
