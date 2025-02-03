#include "wave.h"
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

WavFile* loadWavFile(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        return NULL;
    }

    WavFile* wavFile = (WavFile*)malloc(sizeof(WavFile));
    if (!wavFile) {
        fclose(file);
        return NULL;
    }

    fread(&wavFile->riffHeader, sizeof(RiffHeader), 1, file);
    fread(&wavFile->wavFormat, sizeof(WavFormat), 1, file);
    fread(&wavFile->wavData, sizeof(WavData), 1, file);

    wavFile->data = (uint8_t*)malloc(wavFile->wavData.subChunk2Size);
    if (!wavFile->data) {
        free(wavFile);
        fclose(file);
        return NULL;
    }

    fread(wavFile->data, wavFile->wavData.subChunk2Size, 1, file);
    fclose(file);

    return wavFile;
}

void playWavFile(WavFile* wavFile) {
    if (!wavFile) {
        return;
    }

    HWAVEOUT hWaveOut = 0;
    WAVEFORMATEX wfx = { 0 };
    WAVEHDR waveHdr = { 0 };

    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = wavFile->wavFormat.numChannels;
    wfx.nSamplesPerSec = wavFile->wavFormat.sampleRate;
    wfx.nAvgBytesPerSec = wavFile->wavFormat.byteRate;
    wfx.nBlockAlign = wavFile->wavFormat.blockAlign;
    wfx.wBitsPerSample = wavFile->wavFormat.bitsPerSample;
    wfx.cbSize = 0;

    waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL);

    waveHdr.lpData = (LPSTR)wavFile->data;
    waveHdr.dwBufferLength = wavFile->wavData.subChunk2Size;
    waveHdr.dwFlags = 0;

    waveOutPrepareHeader(hWaveOut, &waveHdr, sizeof(WAVEHDR));
    waveOutWrite(hWaveOut, &waveHdr, sizeof(WAVEHDR));

    while (waveOutUnprepareHeader(hWaveOut, &waveHdr, sizeof(WAVEHDR)) == WAVERR_STILLPLAYING) {
        Sleep(100);
    }

    waveOutClose(hWaveOut);
}

void freeWavFile(WavFile* wavFile) {
    if (wavFile) {
        free(wavFile->data);
        free(wavFile);
    }
}
