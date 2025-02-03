#include "wave/wave.h"
#include <stdio.h>

int main() {
    WavFile* wavFile = loadWavFile("path/to/your/file.wav");
    if (wavFile) {
        playWavFile(wavFile);
        freeWavFile(wavFile);
    }
    else {
        printf("Failed to load WAV file.\n");
    }

    return 0;
}
