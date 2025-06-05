/*
@file - wave.c
@developer - ColorProgrammy
@brief - The main code of the library.
@date - 18/02/2025
@description - The main code for playing .wav files.
*/

#define _CRT_SECURE_NO_WARNINGS
#define CORAL_DLL_EXPORTS

#if defined(_MSC_VER) && _MSC_VER < 1900
#define snprintf _snprintf
#endif

#include "wave.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Platform detection
#if defined(_WIN32)
#define PLATFORM_WINDOWS
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#elif defined(__linux__)
#define PLATFORM_LINUX
#elif defined(__APPLE__)
#define PLATFORM_MACOS
#include <CoreAudio/CoreAudio.h>
#include <AudioToolbox/AudioToolbox.h>
#endif

// Linux audio backend selection
#ifdef PLATFORM_LINUX
#define TRY_PULSE_AUDIO

#define TRY_ALSA
#if !defined(TRY_PULSE_AUDIO) && !defined(TRY_ALSA)
#error "Please define either TRY_PULSE_AUDIO or TRY_ALSA for Linux"
#endif

#ifdef TRY_PULSE_AUDIO
#include <pulse/simple.h>
#include <pulse/error.h>
#endif

#ifdef TRY_ALSA
#include <alsa/asoundlib.h>
#endif
#endif

static char lastError[256] = { 0 };

const char* getAudioError() {
    return lastError;
}

bool adjustVolume(WavFile* wavFile, float volumeFactor) {
    uint16_t bitsPerSample;
    uint32_t dataSize;
    uint8_t* data;
    uint32_t bytesPerSample;
    uint32_t numSamples;
    uint32_t i;
    uint8_t* samplePtr;
    uint8_t sample8;
    int16_t sample16;
    int32_t sample32;
    int16_t adjusted16;
    int32_t adjusted32;
    int64_t adjusted64;

    if (!wavFile) {
        snprintf(lastError, sizeof(lastError), "Null WAV file pointer");
        return false;
    }
    if (wavFile->wavFormat.audioFormat != 1) {
        snprintf(lastError, sizeof(lastError), "Volume adjustment only supports PCM format");
        return false;
    }

    bitsPerSample = wavFile->wavFormat.bitsPerSample;
    dataSize = wavFile->wavData.subChunk2Size;
    data = wavFile->data;

    if (bitsPerSample % 8 != 0) {
        snprintf(lastError, sizeof(lastError), "Unsupported bits per sample: %d", bitsPerSample);
        return false;
    }

    bytesPerSample = bitsPerSample / 8;
    numSamples = dataSize / bytesPerSample;

    for (i = 0; i < numSamples; ++i) {
        samplePtr = data + i * bytesPerSample;

        switch (bitsPerSample) {
        case 8:
            sample8 = *samplePtr;
            adjusted16 = (int16_t)((sample8 - 128) * volumeFactor) + 128;
            if (adjusted16 < 0) adjusted16 = 0;
            else if (adjusted16 > 255) adjusted16 = 255;
            *samplePtr = (uint8_t)adjusted16;
            break;
        case 16:
            sample16 = *(int16_t*)samplePtr;
            adjusted32 = (int32_t)(sample16 * volumeFactor);
            if (adjusted32 > INT16_MAX) adjusted32 = INT16_MAX;
            else if (adjusted32 < INT16_MIN) adjusted32 = INT16_MIN;
            *(int16_t*)samplePtr = (int16_t)adjusted32;
            break;
        case 24:
            sample32 = 0;
            memcpy(&sample32, samplePtr, 3);
            if (sample32 & 0x00800000) {
                sample32 |= 0xFF000000;
            }
            else {
                sample32 &= 0x00FFFFFF;
            }
            sample32 = (int32_t)(sample32 * volumeFactor);
            if (sample32 > 0x007FFFFF) sample32 = 0x007FFFFF;
            else if (sample32 < (int32_t)0xFF800000) sample32 = (int32_t)0xFF800000;
            memcpy(samplePtr, &sample32, 3);
            break;
        case 32:
            sample32 = *(int32_t*)samplePtr;
            adjusted64 = (int64_t)(sample32 * volumeFactor);
            if (adjusted64 > INT32_MAX) adjusted64 = INT32_MAX;
            else if (adjusted64 < INT32_MIN) adjusted64 = INT32_MIN;
            *(int32_t*)samplePtr = (int32_t)adjusted64;
            break;
        default:
            snprintf(lastError, sizeof(lastError), "Unsupported bits per sample: %d", bitsPerSample);
            return false;
        }
    }

    return true;
}

WavMetadata getWavMetadata(const WavFile* wavFile) {
    WavMetadata metadata;
    metadata.sampleRate = 0;
    metadata.numChannels = 0;
    metadata.bitsPerSample = 0;
    metadata.duration = 0.0;
    
    if (wavFile) {
        metadata.sampleRate = wavFile->wavFormat.sampleRate;
        metadata.numChannels = wavFile->wavFormat.numChannels;
        metadata.bitsPerSample = wavFile->wavFormat.bitsPerSample;
        if (wavFile->wavFormat.byteRate > 0) {
            metadata.duration = (double)wavFile->wavData.subChunk2Size / wavFile->wavFormat.byteRate;
        }
    }
    return metadata;
}

WavFile* loadWavFile(const char* filename) {
    FILE* file;
    WavFile* wavFile;
    size_t readResult;
    int memcmpResult1, memcmpResult2, memcmpResult3, memcmpResult4, memcmpResult5;

    file = fopen(filename, "rb");
    if (!file) {
        snprintf(lastError, sizeof(lastError), "Failed to open file: %s", filename);
        return NULL;
    }

    wavFile = (WavFile*)malloc(sizeof(WavFile));
    if (!wavFile) {
        fclose(file);
        snprintf(lastError, sizeof(lastError), "Memory allocation failed");
        return NULL;
    }

    readResult = fread(&wavFile->riffHeader, sizeof(RiffHeader), 1, file);
    if (readResult != 1) {
        snprintf(lastError, sizeof(lastError), "Invalid WAV file header");
        goto error;
    }

    memcmpResult1 = memcmp(wavFile->riffHeader.chunkID, "RIFF", 4);
    memcmpResult2 = memcmp(wavFile->riffHeader.format, "WAVE", 4);
    if (memcmpResult1 != 0 || memcmpResult2 != 0) {
        snprintf(lastError, sizeof(lastError), "Not a valid WAV file");
        goto error;
    }

    readResult = fread(&wavFile->wavFormat, sizeof(WavFormat), 1, file);
    if (readResult != 1) {
        snprintf(lastError, sizeof(lastError), "Invalid format chunk");
        goto error;
    }

    memcmpResult3 = memcmp(wavFile->wavFormat.subChunk1ID, "fmt ", 4);
    if (memcmpResult3 != 0) {
        snprintf(lastError, sizeof(lastError), "Format chunk missing");
        goto error;
    }

    readResult = fread(&wavFile->wavData, sizeof(WavData), 1, file);
    if (readResult != 1) {
        snprintf(lastError, sizeof(lastError), "Invalid data chunk");
        goto error;
    }

    memcmpResult4 = memcmp(wavFile->wavData.subChunk2ID, "data", 4);
    if (memcmpResult4 != 0) {
        snprintf(lastError, sizeof(lastError), "Data chunk missing");
        goto error;
    }

    wavFile->data = (uint8_t*)malloc(wavFile->wavData.subChunk2Size);
    if (!wavFile->data) {
        snprintf(lastError, sizeof(lastError), "Memory allocation failed for audio data");
        goto error;
    }

    readResult = fread(wavFile->data, wavFile->wavData.subChunk2Size, 1, file);
    if (readResult != 1) {
        snprintf(lastError, sizeof(lastError), "Failed to read audio data");
        goto error;
    }

    fclose(file);
    return wavFile;

error:
    if (file) {
        fclose(file);
    }
    if (wavFile) {
        freeWavFile(wavFile);
    }
    return NULL;
}

bool playWavFile(WavFile* wavFile) {
#ifdef PLATFORM_WINDOWS
    HWAVEOUT hWaveOut;
    WAVEFORMATEX wfx;
    WAVEHDR waveHdr;
    MMRESULT result;
#elif defined(PLATFORM_MACOS)
    AudioComponentInstance audioUnit;
    OSStatus status;
    AudioStreamBasicDescription audioFormat;
    AudioBufferList bufferList;
    AudioComponentDescription desc;
#elif defined(PLATFORM_LINUX) && defined(TRY_PULSE_AUDIO)
    pa_simple *s;
    int error;
    pa_sample_spec ss;
#elif defined(PLATFORM_LINUX) && defined(TRY_ALSA)
    snd_pcm_t *pcm_handle;
    int err;
    snd_pcm_hw_params_t *hw_params;
    snd_pcm_uframes_t frames;
    const uint8_t *data_ptr;
    int frames_written;
    snd_pcm_format_t format;
    unsigned int sample_rate;
#endif

    if (!wavFile) {
        snprintf(lastError, sizeof(lastError), "Null WAV file pointer");
        return false;
    }

#ifdef PLATFORM_WINDOWS
    ZeroMemory(&wfx, sizeof(WAVEFORMATEX));
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = wavFile->wavFormat.numChannels;
    wfx.nSamplesPerSec = wavFile->wavFormat.sampleRate;
    wfx.nAvgBytesPerSec = wavFile->wavFormat.byteRate;
    wfx.nBlockAlign = wavFile->wavFormat.blockAlign;
    wfx.wBitsPerSample = wavFile->wavFormat.bitsPerSample;
    wfx.cbSize = 0;

    ZeroMemory(&waveHdr, sizeof(WAVEHDR));
    waveHdr.lpData = (LPSTR)wavFile->data;
    waveHdr.dwBufferLength = wavFile->wavData.subChunk2Size;
    waveHdr.dwFlags = 0;

    result = waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL);
    if (result != MMSYSERR_NOERROR) {
        snprintf(lastError, sizeof(lastError), "Failed to open audio device (Error %d)", result);
        return false;
    }

    result = waveOutPrepareHeader(hWaveOut, &waveHdr, sizeof(WAVEHDR));
    if (result != MMSYSERR_NOERROR) {
        waveOutClose(hWaveOut);
        snprintf(lastError, sizeof(lastError), "Failed to prepare header (Error %d)", result);
        return false;
    }

    result = waveOutWrite(hWaveOut, &waveHdr, sizeof(WAVEHDR));
    if (result != MMSYSERR_NOERROR) {
        waveOutUnprepareHeader(hWaveOut, &waveHdr, sizeof(WAVEHDR));
        waveOutClose(hWaveOut);
        snprintf(lastError, sizeof(lastError), "Failed to play audio (Error %d)", result);
        return false;
    }

    // Wait for playback to complete
    while ((waveHdr.dwFlags & WHDR_DONE) == 0) {
        Sleep(100);
    }

    result = waveOutUnprepareHeader(hWaveOut, &waveHdr, sizeof(WAVEHDR));
    if (result != MMSYSERR_NOERROR) {
        waveOutClose(hWaveOut);
        snprintf(lastError, sizeof(lastError), "Failed to unprepare header (Error %d)", result);
        return false;
    }

    waveOutClose(hWaveOut);
    return true;

#elif defined(PLATFORM_MACOS)
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_DefaultOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;

    status = AudioComponentInstanceNew(AudioComponentFindNext(NULL, &desc), &audioUnit);
    if (status != noErr) {
        snprintf(lastError, sizeof(lastError), "Failed to create audio unit (Error %d)", (int)status);
        return false;
    }

    audioFormat.mSampleRate = wavFile->wavFormat.sampleRate;
    audioFormat.mFormatID = kAudioFormatLinearPCM;
    audioFormat.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
    audioFormat.mBytesPerPacket = wavFile->wavFormat.blockAlign;
    audioFormat.mFramesPerPacket = 1;
    audioFormat.mBytesPerFrame = wavFile->wavFormat.blockAlign;
    audioFormat.mChannelsPerFrame = wavFile->wavFormat.numChannels;
    audioFormat.mBitsPerChannel = wavFile->wavFormat.bitsPerSample;

    status = AudioUnitSetProperty(audioUnit,
        kAudioUnitProperty_StreamFormat,
        kAudioUnitScope_Input,
        0,
        &audioFormat,
        sizeof(audioFormat));
    if (status != noErr) {
        AudioComponentInstanceDispose(audioUnit);
        snprintf(lastError, sizeof(lastError), "Failed to set audio format (Error %d)", (int)status);
        return false;
    }

    bufferList.mNumberBuffers = 1;
    bufferList.mBuffers[0].mNumberChannels = audioFormat.mChannelsPerFrame;
    bufferList.mBuffers[0].mDataByteSize = wavFile->wavData.subChunk2Size;
    bufferList.mBuffers[0].mData = wavFile->data;

    status = AudioUnitRender(audioUnit, NULL, kAudioUnitRenderAction_OutputData, 0, 0, &bufferList);
    AudioComponentInstanceDispose(audioUnit);

    if (status != noErr) {
        snprintf(lastError, sizeof(lastError), "Failed to render audio (Error %d)", (int)status);
        return false;
    }

    return true;

#elif defined(PLATFORM_LINUX) && defined(TRY_PULSE_AUDIO)
    ss.format = PA_SAMPLE_S16LE;
    ss.rate = wavFile->wavFormat.sampleRate;
    ss.channels = wavFile->wavFormat.numChannels;

    s = pa_simple_new(NULL, "WAV Player", PA_STREAM_PLAYBACK, NULL, "Playback", &ss, NULL, NULL, &error);
    if (!s) {
        snprintf(lastError, sizeof(lastError), "PulseAudio error: %s", pa_strerror(error));
        return false;
    }

    if (pa_simple_write(s, wavFile->data, wavFile->wavData.subChunk2Size, &error) < 0) {
        pa_simple_free(s);
        snprintf(lastError, sizeof(lastError), "PulseAudio write error: %s", pa_strerror(error));
        return false;
    }

    if (pa_simple_drain(s, &error) < 0) {
        pa_simple_free(s);
        snprintf(lastError, sizeof(lastError), "PulseAudio drain error: %s", pa_strerror(error));
        return false;
    }

    pa_simple_free(s);
    return true;

#elif defined(PLATFORM_LINUX) && defined(TRY_ALSA)
    if ((err = snd_pcm_open(&pcm_handle, "default", SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        snprintf(lastError, sizeof(lastError), "ALSA open error: %s", snd_strerror(err));
        return false;
    }

    snd_pcm_hw_params_alloca(&hw_params);

    if ((err = snd_pcm_hw_params_any(pcm_handle, hw_params)) < 0) {
        snd_pcm_close(pcm_handle);
        snprintf(lastError, sizeof(lastError), "ALSA init error: %s", snd_strerror(err));
        return false;
    }

    if ((err = snd_pcm_hw_params_set_access(pcm_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
        snd_pcm_close(pcm_handle);
        snprintf(lastError, sizeof(lastError), "ALSA access error: %s", snd_strerror(err));
        return false;
    }

    switch (wavFile->wavFormat.bitsPerSample) {
    case 8:  format = SND_PCM_FORMAT_U8; break;
    case 16: format = SND_PCM_FORMAT_S16_LE; break;
    case 24: format = SND_PCM_FORMAT_S24_LE; break;
    case 32: format = SND_PCM_FORMAT_S32_LE; break;
    default:
        snd_pcm_close(pcm_handle);
        snprintf(lastError, sizeof(lastError), "Unsupported bit depth: %d", wavFile->wavFormat.bitsPerSample);
        return false;
    }

    if ((err = snd_pcm_hw_params_set_format(pcm_handle, hw_params, format)) < 0) {
        snd_pcm_close(pcm_handle);
        snprintf(lastError, sizeof(lastError), "ALSA format error: %s", snd_strerror(err));
        return false;
    }

    sample_rate = wavFile->wavFormat.sampleRate;
    if ((err = snd_pcm_hw_params_set_rate_near(pcm_handle, hw_params, &sample_rate, 0)) < 0) {
        snd_pcm_close(pcm_handle);
        snprintf(lastError, sizeof(lastError), "ALSA rate error: %s", snd_strerror(err));
        return false;
    }

    if ((err = snd_pcm_hw_params_set_channels(pcm_handle, hw_params, wavFile->wavFormat.numChannels)) < 0) {
        snd_pcm_close(pcm_handle);
        snprintf(lastError, sizeof(lastError), "ALSA channels error: %s", snd_strerror(err));
        return false;
    }

    if ((err = snd_pcm_hw_params(pcm_handle, hw_params)) < 0) {
        snd_pcm_close(pcm_handle);
        snprintf(lastError, sizeof(lastError), "ALSA apply params error: %s", snd_strerror(err));
        return false;
    }

    frames = snd_pcm_bytes_to_frames(pcm_handle, wavFile->wavData.subChunk2Size);
    data_ptr = wavFile->data;

    while (frames > 0) {
        frames_written = snd_pcm_writei(pcm_handle, data_ptr, frames);
        if (frames_written < 0) {
            snd_pcm_recover(pcm_handle, frames_written, 0);
            continue;
        }
        data_ptr += frames_written * wavFile->wavFormat.blockAlign;
        frames -= frames_written;
    }

    snd_pcm_drain(pcm_handle);
    snd_pcm_close(pcm_handle);
    return true;
#endif

    snprintf(lastError, sizeof(lastError), "Unsupported platform");
    return false;
}

void freeWavFile(WavFile* wavFile) {
    if (wavFile) {
        if (wavFile->data) {
            free(wavFile->data);
            wavFile->data = NULL;
        }
        free(wavFile);
    }
}