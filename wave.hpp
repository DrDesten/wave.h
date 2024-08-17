#ifndef DRDESTEN_WAV_HPP
#define DRDESTEN_WAV_HPP

#include <stdbool.h>
#include <stdint.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum WavError : uint8_t {
    SUCCESS = 0,
    INVALID_DESCRIPTOR,
    NO_FORMAT,
    NO_DATA,
};

enum WavFormatType : uint8_t {
    PCM   = 1,
    FLOAT = 3,
};

enum WavChannelLayout : uint8_t {
    INTERLIEVED,
    INLINE,
    SPLIT,
};

template <typename T> struct WavData {
    uint32_t channels;
    uint32_t samples;
    T**      data;

    void free() {
        for (uint32_t i = 0; i < channels; i++)
            ::free(data[i]);
        ::free(data);
    }
};

struct WavFile {
  public:
    struct Descriptor {
        uint8_t  RIFF[4];
        uint32_t fileSize;
        uint8_t  WAVE[4];
    } Descriptor;

    struct Format {
        uint8_t  FMT[4];
        uint32_t formatSize;
        uint16_t formatType;
        uint16_t channels;
        uint32_t sampleRate;
        uint32_t byteRate;
        uint16_t blockSize;
        uint16_t bitsPerSample;
    } Format;

    struct Data {
        uint8_t  DATA[4];
        uint32_t dataSize;
        void*    data;
    } Data;

    struct Chunk {
        uint8_t  tag[4];
        uint32_t size;
        void*    data;
        void print() { printf("[Chunk '%.4s', Size %u bytes]\n", tag, size); }
    };

    struct Chunks {
        uint32_t length;
        Chunk*   data;

        void print() {
            for (int64_t i = 0; i < length; i++)
                data[i].print();
        }
    } Chunks;

  private:
    static WavError readHeader(WavFile* wavfile, FILE* file) {
        // Read Descriptor
        fread(&wavfile->Descriptor, sizeof(wavfile->Descriptor), 1, file);
        if (wavfile->Descriptor.RIFF[0] != 'R' ||
            wavfile->Descriptor.RIFF[1] != 'I' ||
            wavfile->Descriptor.RIFF[2] != 'F' ||
            wavfile->Descriptor.RIFF[3] != 'F')
            return INVALID_DESCRIPTOR;
        if (wavfile->Descriptor.WAVE[0] != 'W' ||
            wavfile->Descriptor.WAVE[1] != 'A' ||
            wavfile->Descriptor.WAVE[2] != 'V' ||
            wavfile->Descriptor.WAVE[3] != 'E')
            return INVALID_DESCRIPTOR;

        // Read Format
        fread(&wavfile->Format, sizeof(wavfile->Format), 1, file);
        if (wavfile->Format.FMT[0] != 'f' || wavfile->Format.FMT[1] != 'm' ||
            wavfile->Format.FMT[2] != 't' || wavfile->Format.FMT[3] != ' ')
            return NO_FORMAT;

        // Seek forward to the data chunk if format is longer
        if (wavfile->Format.formatSize > 16) {
            fseek(file, wavfile->Format.formatSize - 16, SEEK_CUR);
        }

        return SUCCESS;
    }

    static Chunk readChunk(FILE* file) {
        // Read Chunk Header
        Chunk chunk;
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

  public:
    WavError readMinimal(FILE* file) {
        // Read Header
        WavError error = WavFile::readHeader(this, file);
        if (error)
            return error;

        // Read first Data Chunk
        // Search file, skipping non-data chunks
        char tag[4];
        while (true) {
            fread(&tag, 4, 1, file);
            if (tag[0] == 'd' && tag[1] == 'a' && tag[2] == 't' &&
                tag[3] == 'a')
                break; // Data chunk found

            // Skip chunk
            uint32_t size;
            fread(&size, 4, 1, file);
            fseek(file, size, SEEK_CUR);

            // Exit when reaching end of file
            if (feof(file))
                return NO_DATA;
        }

        // Set "data" tag
        Data.DATA[0] = tag[0];
        Data.DATA[1] = tag[1];
        Data.DATA[2] = tag[2];
        Data.DATA[3] = tag[3];

        // Read Data chunk size
        fread(&Data.dataSize, sizeof(Data.dataSize), 1, file);

        // Read the Data
        Data.data = malloc(Data.dataSize);
        fread(Data.data, Data.dataSize, 1, file);

        return SUCCESS;
    }

    WavError read(FILE* file) {
        // Read Header
        WavError error = WavFile::readHeader(this, file);
        if (error)
            return error;

        // Prepare dynamic array and allocate a generous initial capacity
        uint32_t chunksCapacity = 32;
        Chunks.length           = 0;
        Chunks.data = (Chunk*)malloc(sizeof(Chunk) * chunksCapacity);

        // Read Chunks
        bool foundData = false;
        while (true) {

            // Read Chunk and check for eof or error
            Chunk chunk = WavFile::readChunk(file);
            if (chunk.data == NULL)
                break;

            // (only the first) Data chunk is handled separately
            if (!foundData && chunk.tag[0] == 'd' && chunk.tag[1] == 'a' &&
                chunk.tag[2] == 't' && chunk.tag[3] == 'a') {
                // Write tag
                Data.DATA[0] = chunk.tag[0];
                Data.DATA[1] = chunk.tag[1];
                Data.DATA[2] = chunk.tag[2];
                Data.DATA[3] = chunk.tag[3];

                // Write size and data
                Data.dataSize = chunk.size;
                Data.data     = chunk.data;

                foundData = true;

                // Handle other tags
            } else {
                // Reallocate dynamic array
                if (chunksCapacity <= Chunks.length) {
                    // Double Capacity when insufficent
                    chunksCapacity *= 2;
                    Chunks.data     = (Chunk*)reallocarray(
                        Chunks.data, chunksCapacity, sizeof(Chunk));
                }

                Chunks.data[Chunks.length] = chunk;
                Chunks.length++;
            }
        }

        // Resize dynamic array to fit
        Chunks.data =
            (Chunk*)reallocarray(Chunks.data, Chunks.length, sizeof(Chunk));

        // Return with the appropiate error code
        return foundData ? SUCCESS : NO_DATA;
    }

    void write(FILE* file) {
        // Write Header
        fwrite(&Descriptor, sizeof(Descriptor), 1, file);
        fwrite(&Format, sizeof(Format), 1, file);

        // Write Data
        fwrite(&Data, sizeof(Data) - sizeof(void*), 1, file);
        fwrite(Data.data, Data.dataSize, 1, file);

        // Write Chunks
        for (int64_t i = 0; i < Chunks.length; i++) {
            fwrite(&Chunks.data[i], sizeof(Chunk) - sizeof(void*), 1, file);
            fwrite(Chunks.data[i].data, Chunks.data[i].size, 1, file);
        }
    }

    void* getRawData(WavChannelLayout channelLayout) {
        void* raw      = Data.data;
        int   channels = Format.channels;
        int   bits     = Format.bitsPerSample;
        int   samples  = Data.dataSize / Format.blockSize;

        if (channels <= 1)
            return raw;

        if (channelLayout == INTERLIEVED)
            return raw;

        if (bits == 8) {
            int8_t* data = (int8_t*)malloc(samples * channels * sizeof(int8_t));
            int8_t** channelData = (int8_t**)malloc(channels * sizeof(int8_t*));

            for (int i = 0; i < channels; i++)
                channelData[i] = data + i * samples;

            for (int i = 0; i < samples; i++)
                for (int j = 0; j < channels; j++)
                    channelData[j][i] = ((int8_t*)raw)[i * channels + j];

            if (channelLayout == INLINE)
                return data;
            if (channelLayout == SPLIT)
                return channelData;
            return NULL;
        }
        if (bits == 16) {
            int16_t* data =
                (int16_t*)malloc(samples * channels * sizeof(int16_t));
            int16_t** channelData =
                (int16_t**)malloc(channels * sizeof(int16_t*));

            for (int i = 0; i < channels; i++)
                channelData[i] = data + i * samples;

            for (int i = 0; i < samples; i++)
                for (int j = 0; j < channels; j++)
                    channelData[j][i] = ((int16_t*)raw)[i * channels + j];

            if (channelLayout == INLINE)
                return data;
            if (channelLayout == SPLIT)
                return channelData;
            return NULL;
        }
        if (bits == 32) {
            int32_t* data =
                (int32_t*)malloc(samples * channels * sizeof(int32_t));
            int32_t** channelData =
                (int32_t**)malloc(channels * sizeof(int32_t*));

            for (int i = 0; i < channels; i++)
                channelData[i] = data + i * samples;

            for (int i = 0; i < samples; i++)
                for (int j = 0; j < channels; j++)
                    channelData[j][i] = ((int32_t*)raw)[i * channels + j];

            if (channelLayout == INLINE)
                return data;
            if (channelLayout == SPLIT)
                return channelData;
            return NULL;
        }
        if (bits == 64) {
            int64_t* data =
                (int64_t*)malloc(samples * channels * sizeof(int64_t));
            int64_t** channelData =
                (int64_t**)malloc(channels * sizeof(int64_t*));

            for (int i = 0; i < channels; i++)
                channelData[i] = data + i * samples;

            for (int i = 0; i < samples; i++)
                for (int j = 0; j < channels; j++)
                    channelData[j][i] = ((int64_t*)raw)[i * channels + j];

            if (channelLayout == INLINE)
                return data;
            if (channelLayout == SPLIT)
                return channelData;
            return NULL;
        }

        // Unsupported bit depth
        return NULL;
    }

  public:
#define READ_SAMPLES(type, scale)                                              \
    for (uint32_t i = 0; i < samples; i++) {                                   \
        for (uint32_t c = 0; c < channels; c++) {                              \
            type sample = ((type*)Data.data)[i * channels + c];                \
            data[c][i]  = sample * (scale);                                    \
        }                                                                      \
    }
    WavData<float> getData() {
        // Not Supported
        if (!(Format.bitsPerSample == 8 || Format.bitsPerSample == 16 ||
              Format.bitsPerSample == 32)) {
            return {0, 0, nullptr};
        }

        // Not Implemented
        if (Format.formatType != WavFormatType::PCM) {
            return {0, 0, nullptr};
        }

        // Allocate space for data
        uint32_t samples  = Data.dataSize / Format.blockSize;
        uint32_t channels = Format.channels;
        float**  data     = (float**)malloc(channels * sizeof(float*));
        for (uint32_t i = 0; i < channels; i++) {
            data[i] = (float*)malloc(samples * sizeof(float));
        }

        switch (Format.bitsPerSample) {
        case 8: {
            READ_SAMPLES(int8_t, 1 / 128.f);
        } break;
        case 16: {
            READ_SAMPLES(int16_t, 1 / 32768.f);
        } break;
        case 32: {
            READ_SAMPLES(int32_t, 1 / 2147483648.f);
        } break;
        }

        return {channels, samples, data};
    }
#undef READ_SAMPLES

#define WRITE_SAMPLES(type, scale)                                             \
    for (uint32_t i = 0; i < data.samples; i++) {                              \
        for (uint32_t c = 0; c < data.channels; c++) {                         \
            float sample = data.data[c][i];                                    \
            sample       = fmax(fmin(sample, 1.0f - (scale)), -1.0f);          \
            ((type*)Data.data)[i * data.channels + c] = sample / (scale);      \
        }                                                                      \
    }
    void setData(WavData<float> data) {
        if (data.channels != Format.channels) {
            printf("Error: Channel count mismatch\n");
            return;
        }
        if (data.samples != Data.dataSize / Format.blockSize) {
            printf("Error: Sample count mismatch\n");
            return;
        }

        // Not Supported
        if (!(Format.bitsPerSample == 8 || Format.bitsPerSample == 16 ||
              Format.bitsPerSample == 32)) {
            return;
        }

        // Not Implemented
        if (Format.formatType != WavFormatType::PCM) {
            return;
        }

        // Write data
        switch (Format.bitsPerSample) {
        case 8: {
            WRITE_SAMPLES(int8_t, 1 / 128.f);
        } break;
        case 16: {
            WRITE_SAMPLES(int16_t, 1 / 32768.f);
        } break;
        case 32: {
            WRITE_SAMPLES(int32_t, 1 / 2147483648.f);
        } break;
        }
    }

    void print() {
        printf("RIFF:         '%.4s'\n", Descriptor.RIFF);
        printf("FileSize:      %d\n", Descriptor.fileSize);
        printf("WAVE:         '%.4s'\n", Descriptor.WAVE);
        printf("FMT:          '%.4s'\n", Format.FMT);
        printf("FormatSize:    %d\n", Format.formatSize);
        printf("FormatType:    %d\n", Format.formatType);
        printf("Channels:      %d\n", Format.channels);
        printf("SampleRate:    %d\n", Format.sampleRate);
        printf("ByteRate:      %d\n", Format.byteRate);
        printf("BlockSize:     %d\n", Format.blockSize);
        printf("BitsPerSample: %d\n", Format.bitsPerSample);
        printf("DATA:         '%.4s'\n", Data.DATA);
        printf("DataSize:      %d\n", Data.dataSize);
        Chunks.print();
    }
    static void print(WavFile& wavfile) { wavfile.print(); }
};

#endif