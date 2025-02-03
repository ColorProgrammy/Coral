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
//#define TRY_ALSA
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

static char lastError[256] = {0};

const char* getAudioError() {
    return lastError;
}

WavFile* loadWavFile(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        snprintf(lastError, sizeof(lastError), "Failed to open file: %s", filename);
        return NULL;
    }

    WavFile* wavFile = malloc(sizeof(WavFile));
    if (!wavFile) {
        fclose(file);
        snprintf(lastError, sizeof(lastError), "Memory allocation failed");
        return NULL;
    }

    if (fread(&wavFile->riffHeader, sizeof(RiffHeader), 1, file) != 1) {
        snprintf(lastError, sizeof(lastError), "Invalid WAV file header");
        goto error;
    }

    if (memcmp(wavFile->riffHeader.chunkID, "RIFF", 4) != 0 || 
        memcmp(wavFile->riffHeader.format, "WAVE", 4) != 0) {
        snprintf(lastError, sizeof(lastError), "Not a valid WAV file");
        goto error;
    }

    if (fread(&wavFile->wavFormat, sizeof(WavFormat), 1, file) != 1) {
        snprintf(lastError, sizeof(lastError), "Invalid format chunk");
        goto error;
    }

    if (memcmp(wavFile->wavFormat.subChunk1ID, "fmt ", 4) != 0) {
        snprintf(lastError, sizeof(lastError), "Format chunk missing");
        goto error;
    }

    if (fread(&wavFile->wavData, sizeof(WavData), 1, file) != 1) {
        snprintf(lastError, sizeof(lastError), "Invalid data chunk");
        goto error;
    }

    if (memcmp(wavFile->wavData.subChunk2ID, "data", 4) != 0) {
        snprintf(lastError, sizeof(lastError), "Data chunk missing");
        goto error;
    }

    wavFile->data = malloc(wavFile->wavData.subChunk2Size);
    if (!wavFile->data) {
        snprintf(lastError, sizeof(lastError), "Memory allocation failed for audio data");
        goto error;
    }

    if (fread(wavFile->data, wavFile->wavData.subChunk2Size, 1, file) != 1) {
        snprintf(lastError, sizeof(lastError), "Failed to read audio data");
        goto error;
    }

    fclose(file);
    return wavFile;

error:
    fclose(file);
    freeWavFile(wavFile);
    return NULL;
}

bool playWavFile(WavFile* wavFile) {
    if (!wavFile) {
        snprintf(lastError, sizeof(lastError), "Null WAV file pointer");
        return false;
    }

#ifdef PLATFORM_WINDOWS
    HWAVEOUT hWaveOut;
    WAVEFORMATEX wfx = {
        .wFormatTag = WAVE_FORMAT_PCM,
        .nChannels = wavFile->wavFormat.numChannels,
        .nSamplesPerSec = wavFile->wavFormat.sampleRate,
        .nAvgBytesPerSec = wavFile->wavFormat.byteRate,
        .nBlockAlign = wavFile->wavFormat.blockAlign,
        .wBitsPerSample = wavFile->wavFormat.bitsPerSample,
        .cbSize = 0
    };

    WAVEHDR waveHdr = {
        .lpData = (LPSTR)wavFile->data,
        .dwBufferLength = wavFile->wavData.subChunk2Size,
        .dwFlags = 0
    };

    MMRESULT result = waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL);
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

    while (waveOutUnprepareHeader(hWaveOut, &waveHdr, sizeof(WAVEHDR)) == WAVERR_STILLPLAYING) {
        Sleep(100);
    }

    waveOutClose(hWaveOut);
    return true;

#elif defined(PLATFORM_MACOS)
    AudioComponentInstance audioUnit;
    OSStatus status = AudioComponentInstanceNew(AudioComponentFindNext(NULL, &(AudioComponentDescription){
        .componentType = kAudioUnitType_Output,
        .componentSubType = kAudioUnitSubType_DefaultOutput,
        .componentManufacturer = kAudioUnitManufacturer_Apple
    }), &audioUnit);

    if (status != noErr) {
        snprintf(lastError, sizeof(lastError), "Failed to create audio unit (Error %d)", (int)status);
        return false;
    }

    AudioStreamBasicDescription audioFormat = {
        .mSampleRate = wavFile->wavFormat.sampleRate,
        .mFormatID = kAudioFormatLinearPCM,
        .mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked,
        .mBytesPerPacket = wavFile->wavFormat.blockAlign,
        .mFramesPerPacket = 1,
        .mBytesPerFrame = wavFile->wavFormat.blockAlign,
        .mChannelsPerFrame = wavFile->wavFormat.numChannels,
        .mBitsPerChannel = wavFile->wavFormat.bitsPerSample
    };

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

    AudioBufferList bufferList = {
        .mNumberBuffers = 1,
        .mBuffers[0] = {
            .mNumberChannels = audioFormat.mChannelsPerFrame,
            .mDataByteSize = wavFile->wavData.subChunk2Size,
            .mData = wavFile->data
        }
    };

    status = AudioUnitRender(audioUnit, NULL, kAudioUnitRenderAction_OutputData, 0, 0, &bufferList);
    AudioComponentInstanceDispose(audioUnit);

    if (status != noErr) {
        snprintf(lastError, sizeof(lastError), "Failed to render audio (Error %d)", (int)status);
        return false;
    }

    return true;

#elif defined(PLATFORM_LINUX)
    #ifdef TRY_PULSE_AUDIO
    pa_simple *s = NULL;
    int error;
    pa_sample_spec ss = {
        .format = PA_SAMPLE_S16LE,
        .rate = wavFile->wavFormat.sampleRate,
        .channels = wavFile->wavFormat.numChannels
    };

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

    #elif defined(TRY_ALSA)
    snd_pcm_t *pcm_handle;
    int err;

    if ((err = snd_pcm_open(&pcm_handle, "default", SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        snprintf(lastError, sizeof(lastError), "ALSA open error: %s", snd_strerror(err));
        return false;
    }

    snd_pcm_hw_params_t *hw_params;
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

    snd_pcm_format_t format;
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

    unsigned int sample_rate = wavFile->wavFormat.sampleRate;
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

    snd_pcm_uframes_t frames = snd_pcm_bytes_to_frames(pcm_handle, wavFile->wavData.subChunk2Size);
    const uint8_t *data_ptr = wavFile->data;
    
    while (frames > 0) {
        int frames_written = snd_pcm_writei(pcm_handle, data_ptr, frames);
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
#endif

    snprintf(lastError, sizeof(lastError), "Unsupported platform");
    return false;
}

void freeWavFile(WavFile* wavFile) {
    if (wavFile) {
        free(wavFile->data);
        free(wavFile);
    }
}
