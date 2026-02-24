# MeshAgent Audio Capture Modules

## Overview

This directory contains platform-specific audio capture modules for MeshCentral remote desktop audio support. These modules enable capturing system audio output (loopback) for streaming to remote users via WebRTC.

## Modules

### Windows (WASAPI)
- **Files:** `Windows/wasapi_audio_capture.{h,c}`
- **API:** Windows Audio Session API (WASAPI)
- **Requires:** Windows Vista or later, Windows SDK
- **Features:** System audio loopback capture, low latency, no special permissions needed

### Linux (PulseAudio)
- **Files:** `Linux/pulseaudio_capture.{h,c}`
- **API:** PulseAudio Simple API
- **Requires:** PulseAudio, libpulse-dev
- **Features:** Monitor source capture, cross-distribution support

### macOS (Core Audio)
- **Files:** `MacOS/coreaudio_capture.{h,c}`
- **API:** Core Audio HAL (AudioToolbox)
- **Requires:** macOS 10.7 or later, Xcode Command Line Tools
- **Features:** HAL output unit loopback, system audio capture

### Opus Encoder
- **Files:** `opus_encoder_wrapper.{h,c}`
- **API:** libopus
- **Requires:** libopus
- **Features:** PCM to Opus encoding, 48kHz stereo, 64kbps bitrate

## Audio Format

All modules capture audio in a consistent format:
- **Sample Rate:** 48000 Hz (48 kHz)
- **Channels:** 2 (stereo)
- **Bit Depth:** 16-bit signed integer (PCM)
- **Frame Size:** 960 samples (20ms at 48kHz)

After capture, audio is encoded with Opus codec:
- **Codec:** Opus
- **Bitrate:** 64 kbps
- **Application:** OPUS_APPLICATION_AUDIO (optimized for speech/music)
- **Complexity:** 10 (maximum quality)

## Usage Example

```c
#include "audio_manager.h"

void OnAudioEncoded(const unsigned char* opusData, int opusDataLen,
                    uint32_t timestamp, void* userData)
{
    // Send Opus packet via WebRTC
    printf("Encoded audio: %d bytes, timestamp: %u\n", opusDataLen, timestamp);
}

int main()
{
    // Create audio manager (automatically detects platform)
    AudioManager* manager = AudioManager_Create();

    // Set callback for encoded audio
    AudioManager_SetCallback(manager, OnAudioEncoded, NULL);

    // Start capturing
    AudioManager_Start(manager);

    // ... run for some time ...

    // Stop and cleanup
    AudioManager_Stop(manager);
    AudioManager_Destroy(manager);

    return 0;
}
```

## Build Instructions

### Windows (Visual Studio)

```cmd
cl /c wasapi_audio_capture.c opus_encoder_wrapper.c
lib /OUT:audio_capture.lib wasapi_audio_capture.obj opus_encoder_wrapper.obj
```

Link with: `ole32.lib uuid.lib opus.lib`

### Linux (GCC)

```bash
gcc -c pulseaudio_capture.c opus_encoder_wrapper.c
ar rcs libaudio_capture.a pulseaudio_capture.o opus_encoder_wrapper.o
```

Link with: `-lpulse -lpulse-simple -lopus -pthread`

### macOS (Clang)

```bash
clang -c coreaudio_capture.c opus_encoder_wrapper.c
ar rcs libaudio_capture.a coreaudio_capture.o opus_encoder_wrapper.o
```

Link with: `-framework AudioToolbox -framework CoreAudio -lopus`

## Dependencies

### All Platforms
- **libopus:** https://opus-codec.org/

### Platform-Specific

#### Windows
- Windows SDK (included with Visual Studio)
- No additional runtime dependencies

#### Linux
- PulseAudio: `sudo apt-get install libpulse-dev` (Ubuntu/Debian)
- PulseAudio: `sudo dnf install pulseaudio-libs-devel` (Fedora/RHEL)

#### macOS
- Xcode Command Line Tools: `xcode-select --install`
- Opus: `brew install opus`

## Testing

A test program is provided in `../../test/test_audio_capture.c`:

```bash
# Compile test (adjust paths as needed)
gcc test_audio_capture.c audio_manager.c [platform]_capture.c opus_encoder_wrapper.c \
    [platform-specific libs] -o test_audio

# Run test (captures audio for 10 seconds)
./test_audio
```

Expected output:
- Approximately 500 Opus packets (50 packets/second for 10 seconds)
- Each packet should be 100-400 bytes (varies with audio content)

## Performance

- **CPU Usage:** 2-5% on modern systems
- **Memory:** ~100KB per session
- **Latency:** 20-40ms capture + 5-10ms encoding = ~30-50ms total
- **Bandwidth:** 64 kbps = 8 KB/s

## Troubleshooting

### No Audio Captured

**Windows:**
- Verify audio is playing on the system
- Check default audio device in Sound settings
- May need administrator privileges

**Linux:**
- Check PulseAudio is running: `pulseaudio --check`
- Verify default sink: `pactl info`
- May need to configure monitor source

**macOS:**
- Grant microphone permission in System Preferences
- Check audio device in Audio MIDI Setup

### High CPU Usage

- Reduce Opus complexity (default is 10, try 5-8)
- Increase frame size (currently 20ms, try 40ms or 60ms)
- Check for audio device driver issues

### Poor Audio Quality

- Increase bitrate (default is 64kbps, try 96kbps or 128kbps)
- Check capture format matches Opus input (48kHz, stereo, 16-bit)
- Verify network latency is acceptable

## License

Licensed under the Apache License, Version 2.0.
See the LICENSE file in the root directory.

## Integration

For integration instructions, see:
- `../../AUDIO-WEBRTC-INTEGRATION.md` - WebRTC integration guide
- `../../AGENT-AUDIO-IMPLEMENTATION.md` - High-level implementation overview

## Contributing

When adding new platform support:
1. Follow the existing API pattern (Initialize, SetCallback, Start, Stop, Cleanup)
2. Use 48kHz, stereo, 16-bit PCM format
3. Provide 20ms audio frames (960 samples at 48kHz)
4. Include platform-specific build instructions
5. Add error handling and logging
