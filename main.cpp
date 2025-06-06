#include <iostream>
#include <Windows.h>

#if defined(_MSC_VER) && _MSC_VER < 1600
    typedef unsigned char uint8_t;
    typedef unsigned short uint16_t;
    typedef unsigned long uint32_t;
#else
    #include <stdint.h>
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
#pragma pack(pop)

typedef WavFile* (*LoadWavFunc)(const char*);
typedef bool (*PlayWavFunc)(WavFile*);
typedef void (*FreeWavFunc)(WavFile*);
typedef const char* (*GetErrorFunc)();
typedef bool (*AdjustVolumeFunc)(WavFile*, float);
typedef WavMetadata (*GetMetadataFunc)(const WavFile*);

int main() {
    HMODULE dllHandle = LoadLibraryA("coral.dll");
    if (!dllHandle) {
        std::cerr << "Error loading DLL: " << GetLastError() << std::endl;
        return 1;
    }

    LoadWavFunc loadWav = (LoadWavFunc)GetProcAddress(dllHandle, "loadWavFile");
    PlayWavFunc playWav = (PlayWavFunc)GetProcAddress(dllHandle, "playWavFile");
    FreeWavFunc freeWav = (FreeWavFunc)GetProcAddress(dllHandle, "freeWavFile");
    GetErrorFunc getError = (GetErrorFunc)GetProcAddress(dllHandle, "getAudioError");
    AdjustVolumeFunc adjustVol = (AdjustVolumeFunc)GetProcAddress(dllHandle, "adjustVolume");
    GetMetadataFunc getMetadata = (GetMetadataFunc)GetProcAddress(dllHandle, "getWavMetadata");

    if (!loadWav || !playWav || !freeWav || !getError || !adjustVol || !getMetadata) {
        std::cerr << "Error loading functions from DLL" << std::endl;
        FreeLibrary(dllHandle);
        return 1;
    }

    WavFile* wav = loadWav("example.wav");
    if (!wav) {
        std::cerr << "Error loading WAV: " << getError() << std::endl;
        FreeLibrary(dllHandle);
        return 1;
    }

    WavMetadata meta = getMetadata(wav);
    std::cout << "Sample Rate: " << meta.sampleRate << "\n"
              << "Channels: " << meta.numChannels << "\n"
              << "Duration: " << meta.duration << " sec\n";

    if (!adjustVol(wav, 1.5f)) {
        std::cerr << "Volume adjust failed: " << getError() << std::endl;
    }
    if (!playWav(wav)) {
        std::cerr << "Playback failed: " << getError() << std::endl;
    }

    freeWav(wav);
    FreeLibrary(dllHandle);
    return 0;
}