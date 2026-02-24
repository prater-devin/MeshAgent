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

#include "opus_encoder_wrapper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int OpusEncoder_Initialize(
    OpusEncoderWrapper* wrapper,
    int32_t sampleRate,
    int channels,
    int bitrate,
    int application)
{
    if (wrapper == NULL) {
        return -1;
    }

    // Zero out the structure
    memset(wrapper, 0, sizeof(OpusEncoderWrapper));

    // Validate parameters
    if (channels < 1 || channels > 2) {
        fprintf(stderr, "Invalid channel count: %d (must be 1 or 2)\n", channels);
        return -1;
    }

    // Validate sample rate (Opus supports 8k, 12k, 16k, 24k, 48k)
    if (sampleRate != 8000 && sampleRate != 12000 && sampleRate != 16000 &&
        sampleRate != 24000 && sampleRate != 48000) {
        fprintf(stderr, "Invalid sample rate: %d (must be 8k, 12k, 16k, 24k, or 48k)\n", sampleRate);
        return -1;
    }

    // Store parameters
    wrapper->sampleRate = sampleRate;
    wrapper->channels = channels;
    wrapper->bitrate = bitrate;

    // Calculate frame size for 20ms at the given sample rate
    // 20ms = 0.02 seconds
    wrapper->frameSize = sampleRate * 20 / 1000;

    int error;

    // Create Opus encoder
    wrapper->encoder = opus_encoder_create(
        sampleRate,
        channels,
        application,
        &error
    );

    if (error != OPUS_OK) {
        fprintf(stderr, "Failed to create Opus encoder: %s\n", opus_strerror(error));
        return -1;
    }

    // Configure encoder settings
    opus_encoder_ctl(wrapper->encoder, OPUS_SET_BITRATE(bitrate));
    opus_encoder_ctl(wrapper->encoder, OPUS_SET_COMPLEXITY(10));  // Max quality
    opus_encoder_ctl(wrapper->encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_MUSIC));  // Better for screen readers

    // Enable variable bitrate for better quality
    opus_encoder_ctl(wrapper->encoder, OPUS_SET_VBR(1));

    // Enable forward error correction for lossy networks
    opus_encoder_ctl(wrapper->encoder, OPUS_SET_INBAND_FEC(1));

    // Set packet loss percentage (0% expected, but FEC helps)
    opus_encoder_ctl(wrapper->encoder, OPUS_SET_PACKET_LOSS_PERC(0));

    wrapper->isInitialized = 1;

    printf("Opus encoder initialized: %dkHz, %d channels, %dkbps, frame size: %d samples\n",
           sampleRate / 1000, channels, bitrate / 1000, wrapper->frameSize);

    return 0;
}

int OpusEncoder_Encode(
    OpusEncoderWrapper* wrapper,
    const int16_t* pcm,
    int frameSize,
    unsigned char* output,
    int maxOutputSize)
{
    if (wrapper == NULL || !wrapper->isInitialized || wrapper->encoder == NULL) {
        return -1;
    }

    if (pcm == NULL || output == NULL) {
        return -1;
    }

    // Verify frame size matches encoder configuration
    if (frameSize != wrapper->frameSize) {
        fprintf(stderr, "Frame size mismatch: got %d, expected %d\n",
                frameSize, wrapper->frameSize);
        return -1;
    }

    // Encode PCM to Opus
    int encodedBytes = opus_encode(
        wrapper->encoder,
        pcm,
        frameSize,
        output,
        maxOutputSize
    );

    if (encodedBytes < 0) {
        fprintf(stderr, "Opus encoding failed: %s\n", opus_strerror(encodedBytes));
        return -1;
    }

    return encodedBytes;
}

int OpusEncoder_GetFrameSize(OpusEncoderWrapper* wrapper)
{
    if (wrapper == NULL || !wrapper->isInitialized) {
        return -1;
    }

    return wrapper->frameSize;
}

int OpusEncoder_SetBitrate(OpusEncoderWrapper* wrapper, int bitrate)
{
    if (wrapper == NULL || !wrapper->isInitialized || wrapper->encoder == NULL) {
        return -1;
    }

    int error = opus_encoder_ctl(wrapper->encoder, OPUS_SET_BITRATE(bitrate));
    if (error != OPUS_OK) {
        fprintf(stderr, "Failed to set bitrate: %s\n", opus_strerror(error));
        return -1;
    }

    wrapper->bitrate = bitrate;
    return 0;
}

int OpusEncoder_SetComplexity(OpusEncoderWrapper* wrapper, int complexity)
{
    if (wrapper == NULL || !wrapper->isInitialized || wrapper->encoder == NULL) {
        return -1;
    }

    if (complexity < 0 || complexity > 10) {
        fprintf(stderr, "Invalid complexity: %d (must be 0-10)\n", complexity);
        return -1;
    }

    int error = opus_encoder_ctl(wrapper->encoder, OPUS_SET_COMPLEXITY(complexity));
    if (error != OPUS_OK) {
        fprintf(stderr, "Failed to set complexity: %s\n", opus_strerror(error));
        return -1;
    }

    return 0;
}

void OpusEncoder_Cleanup(OpusEncoderWrapper* wrapper)
{
    if (wrapper == NULL) {
        return;
    }

    if (wrapper->encoder) {
        opus_encoder_destroy(wrapper->encoder);
        wrapper->encoder = NULL;
    }

    wrapper->isInitialized = 0;
}
