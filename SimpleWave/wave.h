#ifndef WAVE_H
#define WAVE_H

#include <stdint.h>
#include <stdbool.h>

#pragma pack(push, 1)
typedef struct {
    char chunkID[4];
    uint32_t chunkSize;
    char format[4];
} RiffHeader;

typedef struct {
    char subChunk1ID[4];
    uint32_t subChunk1Size;
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
} WavFormat;

typedef struct {
    char subChunk2ID[4];
    uint32_t subChunk2Size;
} WavData;
#pragma pack(pop)

typedef struct {
    RiffHeader riffHeader;
    WavFormat wavFormat;
    WavData wavData;
    uint8_t* data;
} WavFile;

WavFile* loadWavFile(const char* filename);
bool playWavFile(WavFile* wavFile);
void freeWavFile(WavFile* wavFile);
const char* getAudioError();

#endif
