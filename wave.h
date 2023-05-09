#ifndef DRDESTEN_WAV
#define DRDESTEN_WAV

#include <stdbool.h>
#include <stdint.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct {
	int16_t *data;
} WavData;

typedef struct {
	struct WavDescriptorChunk {
		uint8_t  RIFF[4];
		uint32_t fileSize;
		uint8_t  WAVE[4];
	} Descriptor;

	struct WavFormatChunk {
		uint8_t  FMT[4];
		uint32_t formatSize;
		uint16_t formatType;
		uint16_t channels;
		uint32_t sampleRate;
		uint32_t byteRate;
		uint16_t blockSize;
		uint16_t bitsPerSample;
	} Format;

	struct WavDataChunk {
		uint8_t  DATA[4];
		uint32_t dataSize;
		WavData  data;
	} Data;
} WavFile;

typedef enum {
    WAV_SUCCESS,
    WAV_INVALID_DESCRIPTOR,
    WAV_NO_FORMAT,
    WAV_NO_DATA,
} WavError;

static WavError WavFile_readMinimal(WavFile* wavfile, FILE* file) {
    // Read Descriptor
	fread(&wavfile->Descriptor, sizeof(wavfile->Descriptor), 1, file);
	if (wavfile->Descriptor.RIFF[0] != 'R' || wavfile->Descriptor.RIFF[1] != 'I' ||
        wavfile->Descriptor.RIFF[2] != 'F' || wavfile->Descriptor.RIFF[3] != 'F')
		return WAV_INVALID_DESCRIPTOR;
	if (wavfile->Descriptor.WAVE[0] != 'W' || wavfile->Descriptor.WAVE[1] != 'A' ||
        wavfile->Descriptor.WAVE[2] != 'V' || wavfile->Descriptor.WAVE[3] != 'E')
		return WAV_INVALID_DESCRIPTOR;

    // Read Format
	fread(&wavfile->Format, sizeof(wavfile->Format), 1, file);
	if (wavfile->Format.FMT[0] != 'f' || wavfile->Format.FMT[1] != 'm' ||
        wavfile->Format.FMT[2] != 't' || wavfile->Format.FMT[3] != ' ')
		return WAV_NO_FORMAT;

	// Read first Data Chunk
    // Search file, skipping non-data chunks
	char tag[4];
	while (true) {
		fread(&tag, 4, 1, file);
		if (tag[0] == 'd' && tag[1] == 'a' && tag[2] == 't' && tag[3] == 'a')
			break; // Data chunk found

		// Skip chunk
		uint32_t size;
		fread(&size, 4, 1, file);
		fseek(file, size, SEEK_CUR);

        // Exit when reaching end of file
		if (feof(file))
			return WAV_NO_DATA;
	}

	// Set "data" tag
	wavfile->Data.DATA[0] = tag[0];
	wavfile->Data.DATA[1] = tag[1];
	wavfile->Data.DATA[2] = tag[2];
	wavfile->Data.DATA[3] = tag[3];

    // Read Data chunk size
	fread(&wavfile->Data.dataSize, sizeof(wavfile->Data.dataSize), 1, file);

	// Read the Data
	wavfile->Data.data.data = (int16_t*) malloc(wavfile->Data.dataSize);
	fread(wavfile->Data.data.data, wavfile->Data.dataSize, 1, file);

    return WAV_SUCCESS;
}

typedef struct {
    uint8_t  tag[4];
    uint32_t size;
    void*    data;
} WavChunk;

typedef struct {
    uint32_t  length;
    WavChunk* data;
} WavChunks;

static WavError WavFile_read(WavFile* wavfile, WavChunks* chunks, FILE* file) {
    // Read Descriptor
	fread(&wavfile->Descriptor, sizeof(wavfile->Descriptor), 1, file);
	if (wavfile->Descriptor.RIFF[0] != 'R' || wavfile->Descriptor.RIFF[1] != 'I' ||
        wavfile->Descriptor.RIFF[2] != 'F' || wavfile->Descriptor.RIFF[3] != 'F')
		return WAV_INVALID_DESCRIPTOR;
	if (wavfile->Descriptor.WAVE[0] != 'W' || wavfile->Descriptor.WAVE[1] != 'A' ||
        wavfile->Descriptor.WAVE[2] != 'V' || wavfile->Descriptor.WAVE[3] != 'E')
		return WAV_INVALID_DESCRIPTOR;

    // Read Format
	fread(&wavfile->Format, sizeof(wavfile->Format), 1, file);
	if (wavfile->Format.FMT[0] != 'f' || wavfile->Format.FMT[1] != 'm' ||
        wavfile->Format.FMT[2] != 't' || wavfile->Format.FMT[3] != ' ')
		return WAV_NO_FORMAT;

    // Prepare dynamic array
    chunks->length          = 0;
    uint32_t chunksCapacity = 0;

	// Read Chunks
    bool foundData = false;
	while (true) {
	    uint8_t  tag[4];
		uint32_t size;
        void*    data;

        // Read tag and size
		fread(&tag, 4, 1, file);
		fread(&size, 4, 1, file);

        // Exit when reaching end of file
        // We need to check for end-of-file after advancing the file pointer, else we'll get false as the pointer sits on the last byte
		if (feof(file))
            break;

        // Allocate and Read chunk data
        data = malloc(size);
        fread(data, size, 1, file);

        // (only the first) Data chunk is handled separately
		if (!foundData && tag[0] == 'd' && tag[1] == 'a' && tag[2] == 't' && tag[3] == 'a') {
            // Write tag
            wavfile->Data.DATA[0] = tag[0];
            wavfile->Data.DATA[1] = tag[1];
            wavfile->Data.DATA[2] = tag[2];
            wavfile->Data.DATA[3] = tag[3];

            // Write size and data
            wavfile->Data.dataSize = size;
            wavfile->Data.data.data = (int16_t*) data; 

            foundData = true;

        // Handle other tags
        } else {
            // Allocate new chunk
            WavChunk chunk;

            // Write tag
            chunk.tag[0] = tag[0];
            chunk.tag[1] = tag[1];
            chunk.tag[2] = tag[2];
            chunk.tag[3] = tag[3];

            // Write size and data
            chunk.size = size;
            chunk.data = data;

            // Allocate for dynamic array
            if (chunksCapacity <= chunks->length) {
                // Allocate a generous amount the first time around
                if (chunksCapacity == 0) {
                    chunks->data   = (WavChunk*) malloc(sizeof(WavChunk) * 32);
                    chunksCapacity = 32;
                }

                // Double Capacity when insufficent
                chunksCapacity *= 2;
                chunks->data    = (WavChunk*) reallocarray(chunks->data, chunksCapacity, sizeof(WavChunk));
            }

            chunks->data[chunks->length] = chunk;
            chunks->length++;
        }
	}

    // Resize dynamic array to fit
    chunks->data = (WavChunk*) reallocarray(chunks->data, chunks->length, sizeof(WavChunk));

    // Return with the appropiate error code
    return foundData ? WAV_SUCCESS : WAV_NO_DATA;
}


static void WavFile_print(WavFile* wavfile) {
	printf("RIFF:         '%.4s'\n", wavfile->Descriptor.RIFF);
	printf("FileSize:      %d\n",    wavfile->Descriptor.fileSize);
	printf("WAVE:         '%.4s'\n", wavfile->Descriptor.WAVE);
	printf("FMT:          '%.4s'\n", wavfile->Format.FMT);
	printf("FormatSize:    %d\n",    wavfile->Format.formatSize);
	printf("FormatType:    %d\n",    wavfile->Format.formatType);
	printf("Channels:      %d\n",    wavfile->Format.channels);
	printf("SampleRate:    %d\n",    wavfile->Format.sampleRate);
	printf("ByteRate:      %d\n",    wavfile->Format.byteRate);
	printf("BlockSize:     %d\n",    wavfile->Format.blockSize);
	printf("BitsPerSample: %d\n",    wavfile->Format.bitsPerSample);
	printf("DATA:         '%.4s'\n", wavfile->Data.DATA);
	printf("DataSize:      %d\n",    wavfile->Data.dataSize);
}

static void WavChunk_print(WavChunk* chunk) {
    printf("[Chunk '%.4s', Size %u bytes]\n", chunk->tag, chunk->size);
}
static void WavChunks_print(WavChunks* chunks) {
    for (size_t i = 0; i < chunks->length; i++) {
        WavChunk_print(&chunks->data[i]);
    }
}


#ifdef __cplusplus
}
#endif

#endif