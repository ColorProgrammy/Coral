#ifndef WAVE_H
#define WAVE_H

#if defined(_MSC_VER) && _MSC_VER < 1600
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long uint32_t;
typedef signed char int8_t;
typedef signed short int16_t;
typedef signed long int32_t;
typedef signed __int64 int64_t;
#define INT16_MIN (-32768)
#define INT16_MAX 32767
#define INT32_MIN (-2147483647-1)
#define INT32_MAX 2147483647
#else
#include <stdint.h>
#endif

#ifndef __cplusplus
#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L
typedef unsigned char bool;
#define true 1
#define false 0
#else
#include <stdbool.h>
#endif
#endif

// DLL
#if defined(_WIN32)
    #ifdef CORAL_DLL_EXPORTS
        #define CORAL_API __declspec(dllexport)
    #else
        #define CORAL_API __declspec(dllimport)
    #endif
#else
    #define CORAL_API
#endif

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

typedef struct {
    int sampleRate;
    int numChannels;
    int bitsPerSample;
    double duration;
} WavMetadata;

#ifdef __cplusplus
extern "C" {
#endif

    CORAL_API WavFile* loadWavFile(const char* filename);
    CORAL_API bool playWavFile(WavFile* wavFile);
    CORAL_API void freeWavFile(WavFile* wavFile);
    CORAL_API const char* getAudioError();
    CORAL_API bool adjustVolume(WavFile* wavFile, float volumeFactor);
    CORAL_API WavMetadata getWavMetadata(const WavFile* wavFile);

#ifdef __cplusplus
}
#endif

#endif