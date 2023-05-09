# wave.h

I wrote this library because [Tinywav](https://github.com/mhroth/tinywav) doesn't handle the RIFF container correctly.

Currently can only properly read 16bit integer PCM .wav files and does not handle multiple channels.  
It simply gives you the raw data.

## Usage

Read the data off a .wav file

```c
#include <stdint.h>
#include <stdio.h>
#include "wave.h"

int main() {

	FILE *file = fopen("test.wav", "rb");

	WavFile  wav;
    WavError error = WavFile_readMinimal(&wav, file);

    // Data is found in WavFile.Data.data.data
    int16_t* pcm_data = wav.Data.data.data;
    // Size of the data in bytes
    uint32_t pcm_data_size = wav.Data.dataSize;
    // Length of the data
    uint32_t pcm_data_length = wav.Data.dataSize / sizeof(int16_t);

	return 0;
}
```

