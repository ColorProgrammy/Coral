#include "SimpleWave/wave.h"
#include <stdio.h>

int main() {
    const char* filename = "audio/sound.wav";

    WavFile* wav = loadWavFile(filename);
    if (!wav) {
        printf("Error: %s\n", getAudioError());
        return 1;
    }

    if (!playWavFile(wav)) {
        printf("Playback failed: %s\n", getAudioError());
    } else {
        printf("Playback completed successfully\n");
    }

    freeWavFile(wav);
    return 0;
}
