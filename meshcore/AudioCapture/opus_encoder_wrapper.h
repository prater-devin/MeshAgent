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

#ifndef OPUS_ENCODER_WRAPPER_H
#define OPUS_ENCODER_WRAPPER_H

#include <opus/opus.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opus encoder context
typedef struct OpusEncoderWrapper {
    OpusEncoder* encoder;
    int32_t sampleRate;
    int channels;
    int bitrate;
    int frameSize;          // In samples (e.g., 960 for 20ms at 48kHz)
    int isInitialized;
} OpusEncoderWrapper;

// Initialize Opus encoder
// Parameters:
//   - wrapper: Encoder context
//   - sampleRate: Sample rate in Hz (8000, 12000, 16000, 24000, or 48000)
//   - channels: Number of channels (1=mono, 2=stereo)
//   - bitrate: Target bitrate in bits/sec (e.g., 64000 for 64kbps)
//   - application: OPUS_APPLICATION_VOIP, OPUS_APPLICATION_AUDIO, or OPUS_APPLICATION_RESTRICTED_LOWDELAY
// Returns: 0 on success, -1 on failure
int OpusEncoder_Initialize(
    OpusEncoderWrapper* wrapper,
    int32_t sampleRate,
    int channels,
    int bitrate,
    int application
);

// Encode PCM audio to Opus
// Parameters:
//   - wrapper: Encoder context
//   - pcm: Input PCM audio (int16_t format)
//   - frameSize: Number of samples per channel (must match encoder frame size)
//   - output: Output buffer for encoded data
//   - maxOutputSize: Maximum size of output buffer
// Returns: Number of bytes encoded, or negative error code
int OpusEncoder_Encode(
    OpusEncoderWrapper* wrapper,
    const int16_t* pcm,
    int frameSize,
    unsigned char* output,
    int maxOutputSize
);

// Get recommended frame size for current sample rate
// Returns: Number of samples per channel for 20ms frame
int OpusEncoder_GetFrameSize(OpusEncoderWrapper* wrapper);

// Set encoder bitrate
// Parameters:
//   - wrapper: Encoder context
//   - bitrate: New bitrate in bits/sec
// Returns: 0 on success, -1 on failure
int OpusEncoder_SetBitrate(OpusEncoderWrapper* wrapper, int bitrate);

// Set encoder complexity (0-10, higher = better quality but slower)
// Parameters:
//   - wrapper: Encoder context
//   - complexity: Complexity level (0-10)
// Returns: 0 on success, -1 on failure
int OpusEncoder_SetComplexity(OpusEncoderWrapper* wrapper, int complexity);

// Cleanup and release resources
void OpusEncoder_Cleanup(OpusEncoderWrapper* wrapper);

#ifdef __cplusplus
}
#endif

#endif // OPUS_ENCODER_WRAPPER_H
