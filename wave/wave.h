#ifndef WAVE_H
#define WAVE_H

#include <stdint.h>

typedef struct {
    uint32_t chunkId;
    uint32_t chunkSize;
    uint32_t format;
} RiffHeader;

typedef struct {
    uint32_t subChunk1Id;
    uint32_t subChunk1Size;
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
} WavFormat;

typedef struct {
    uint32_t subChunk2Id;
    uint32_t subChunk2Size;
} WavData;

typedef struct {
    RiffHeader riffHeader;
    WavFormat wavFormat;
    WavData wavData;
    uint8_t* data;
} WavFile;

WavFile* loadWavFile(const char* filename);
void playWavFile(WavFile* wavFile);
void freeWavFile(WavFile* wavFile);

#endif // WAVE_H
