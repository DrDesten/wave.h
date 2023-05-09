# wave.h

I wrote this library because [Tinywav](https://github.com/mhroth/tinywav) doesn't handle the RIFF container correctly.

Currently does not handle multiple channels, it simply gives you the raw data.

## Usage

Read the data off a .wav file

```c
...
#include "wave.h"

int main() {

	FILE *file = fopen("test.wav", "rb");

	WavFile  wav;
    WavError error = WavFile_readMinimal(&wav, file);

    wav.Data.data;     // Sample Data               (void*)
    wav.Data.dataSize; // Size of the data in bytes (uint32_t)

	return 0;
}
```

Read all RIFF chunks off a .wav file

```c
...
#include "wave.h"

int main() {

	FILE *file = fopen("test.wav", "rb");

	WavFile   wav;
    WavChunks chunks;
    WavError  error = WavFile_read(&wav, &chunks, file);

    // 1st Data chunk
    wav.Data.data;     // Sample Data                 (void*)
    wav.Data.dataSize; // Size of the data in bytes   (uint32_t)

    // All other chunks
    chunks.length;     // Number of chunks            (uint32_t)
    WavChunk chunk = chunks.data[...];
    chunk.tag;         // RIFF tag of chunk           (uint8_t[4])
    chunk.size;        // Size in bytes of chunk data (uint32_t)
    chunk.data;        // Chunk data                  (void*)

	return 0;
}
```
