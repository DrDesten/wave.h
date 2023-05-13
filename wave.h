#ifndef DRDESTEN_WAV
#define DRDESTEN_WAV

#include <stdbool.h>
#include <stdint.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WAV_SUCCESS = 0,
    WAV_INVALID_DESCRIPTOR,
    WAV_NO_FORMAT,
    WAV_NO_DATA,
} WavError;

typedef enum {
    WAV_PCM = 1,
    WAV_FLOAT = 3,
} WavFormatType;

typedef enum {
    WAV_CHANNEL_INTERLIEVED,
    WAV_CHANNEL_INLINE,
    WAV_CHANNEL_SPLIT,
} WavChannelLayout;

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
		void*    data;
	} Data;
} WavFile;

typedef struct {
    uint8_t  tag[4];
    uint32_t size;
    void*    data;
} WavChunk;

typedef struct {
    uint32_t  length;
    WavChunk* data;
} WavChunks;


static WavChunk Wav_readChunk(FILE* file) {
    // Read Chunk Header
    WavChunk chunk;
    fread(&chunk.tag, 4, 1, file);
    fread(&chunk.size, 4, 1, file);

    // Check for end-of-file or errors
    if (feof(file) || ferror(file)) {
        chunk.data = NULL;
        return chunk;
    }

    // Read Chunk Data
    chunk.data = malloc(chunk.size);
    fread(chunk.data, chunk.size, 1, file);

    return chunk;
}

static WavError WavFile_readHeader(WavFile* wavfile, FILE* file) {
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
    
    // Seek forward to the data chunk if format is longer
    if (wavfile->Format.formatSize > 16) {
        fseek(file, wavfile->Format.formatSize - 16, SEEK_CUR);
    }

    return WAV_SUCCESS;
}

static WavError WavFile_readMinimal(WavFile* wavfile, FILE* file) {
    // Read Header
    WavError error = WavFile_readHeader(wavfile, file);
    if (error) return error;

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
	wavfile->Data.data = malloc(wavfile->Data.dataSize);
	fread(wavfile->Data.data, wavfile->Data.dataSize, 1, file);

    return WAV_SUCCESS;
}


static WavError WavFile_read(WavFile* wavfile, WavChunks* chunks, FILE* file) {
    // Read Header
    WavError error = WavFile_readHeader(wavfile, file);
    if (error) return error;

    // Prepare dynamic array and allocate a generous initial capacity
    uint32_t chunksCapacity = 32;
    chunks->length          = 0;
    chunks->data            = (WavChunk*) malloc(sizeof(WavChunk) * chunksCapacity);

	// Read Chunks
    bool foundData = false;
	while (true) {

        // Read Chunk and check for eof or error
        WavChunk chunk = Wav_readChunk(file);
        if (chunk.data == NULL)
            break;

        // (only the first) Data chunk is handled separately
		if (!foundData && chunk.tag[0] == 'd' && chunk.tag[1] == 'a' && chunk.tag[2] == 't' && chunk.tag[3] == 'a') {
            // Write tag
            wavfile->Data.DATA[0] = chunk.tag[0];
            wavfile->Data.DATA[1] = chunk.tag[1];
            wavfile->Data.DATA[2] = chunk.tag[2];
            wavfile->Data.DATA[3] = chunk.tag[3];

            // Write size and data
            wavfile->Data.dataSize = chunk.size;
            wavfile->Data.data = chunk.data; 

            foundData = true;

        // Handle other tags
        } else {
            // Reallocate dynamic array
            if (chunksCapacity <= chunks->length) {
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

static void* WavFile_getData(WavFile* wavfile, WavChannelLayout channelLayout) {
    void* raw      = wavfile->Data.data;
    int   channels = wavfile->Format.channels;
    int   bits     = wavfile->Format.bitsPerSample;
    int   samples  = wavfile->Data.dataSize / wavfile->Format.blockSize;

    if (channels <= 1)
        return raw;

    if (channelLayout == WAV_CHANNEL_INTERLIEVED)
        return raw;

    if (bits == 8) {
        int8_t*  data        = (int8_t*)  malloc(samples * channels * sizeof(int8_t));
        int8_t** channelData = (int8_t**) malloc(channels * sizeof(int8_t*));

        for (int i = 0; i < channels; i++)
            channelData[i] = data + i * samples;

        for (int i = 0; i < samples; i++)
            for (int j = 0; j < channels; j++)
                channelData[j][i] = ((int8_t*) raw)[i * channels + j];

        if (channelLayout == WAV_CHANNEL_INLINE)
            return data;
        if (channelLayout == WAV_CHANNEL_SPLIT)
            return channelData;
        return NULL;
    }
    if (bits == 16) {
        int16_t*  data        = (int16_t*)  malloc(samples * channels * sizeof(int16_t));
        int16_t** channelData = (int16_t**) malloc(channels * sizeof(int16_t*));

        for (int i = 0; i < channels; i++)
            channelData[i] = data + i * samples;

        for (int i = 0; i < samples; i++)
            for (int j = 0; j < channels; j++)
                channelData[j][i] = ((int16_t*) raw)[i * channels + j];

        if (channelLayout == WAV_CHANNEL_INLINE)
            return data;
        if (channelLayout == WAV_CHANNEL_SPLIT)
            return channelData;
        return NULL;
    }
    if (bits == 32) {
        int32_t*  data        = (int32_t*)  malloc(samples * channels * sizeof(int32_t));
        int32_t** channelData = (int32_t**) malloc(channels * sizeof(int32_t*));

        for (int i = 0; i < channels; i++)
            channelData[i] = data + i * samples;

        for (int i = 0; i < samples; i++)
            for (int j = 0; j < channels; j++)
                channelData[j][i] = ((int32_t*) raw)[i * channels + j];

        if (channelLayout == WAV_CHANNEL_INLINE)
            return data;
        if (channelLayout == WAV_CHANNEL_SPLIT)
            return channelData;
        return NULL;
    }
    if (bits == 64) {
        int64_t*  data        = (int64_t*)  malloc(samples * channels * sizeof(int64_t));
        int64_t** channelData = (int64_t**) malloc(channels * sizeof(int64_t*));

        for (int i = 0; i < channels; i++)
            channelData[i] = data + i * samples;

        for (int i = 0; i < samples; i++)
            for (int j = 0; j < channels; j++)
                channelData[j][i] = ((int64_t*) raw)[i * channels + j];

        if (channelLayout == WAV_CHANNEL_INLINE)
            return data;
        if (channelLayout == WAV_CHANNEL_SPLIT)
            return channelData;
        return NULL;
    }

    // Unsupported bit depth
    return NULL;
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