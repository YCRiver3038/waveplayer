#ifndef WAVE_LOADER_H_INCLUDED
#define WAVE_LOADER_H_INCLUDED

#include "stdint.h"
#include "stdlib.h"
#include "stdio.h"

#include <string>

typedef enum {
    SIGNED_8 = 0,
    SIGNED_16,
    SIGNED_24,
    SIGNED_32,
    FLOAT_32,
    EXTENSIBLE=65534
} WF_Format;

typedef union {
    int8_t s8[4];
    int16_t s16[2];
    int32_t s32;
    float f32;
} WaveData;

class WaveFile {
    private:
        FILE* wFile = nullptr;
        bool isClosed = false;
        long dataChunkPos = 0;
        uint32_t dataChunkSize = 0;
        union  {
            char raw[4] = {};
            uint32_t value;
        } fSize;
        union {
            char raw[4] = {};
            uint32_t data;
        } chunkSize;
        union {
            char raw[2] = {};
            uint16_t data;
        } wFormat;
        union {
            char raw[2] = {};
            uint16_t data;
        } nChannels;
        union {
            char raw[4] = {};
            uint32_t data;
        } nSPS;
        union {
            char raw[4] = {};
            uint32_t data;
        } nBPS;
        union {
            char raw[2] = {};
            uint16_t data;
        } nBlockAlign;
        union {
            char raw[2] = {};
            uint32_t data;
        } nBitsPerSample;
        union {
            char raw[2];
            uint16_t data;
        } cbSize;
        union {
            char raw[2];
            uint16_t data;
        } Samples;
        union {
            char raw[4];
            uint32_t data;
        } dwChannelMask;
        uint8_t guid[16] = {};
        union {
            char raw[4];
            uint32_t data;
        } subfmt;

        uint32_t nSingleSampleSize = 0;
        uint32_t nBytesPerSample = 0;
        bool isUnsupported = true;
        bool isWaveDataEnd = false;
        WF_Format wfmt;
        uint32_t readSizeCount = 0;
        char* tempRawData = nullptr;

    public:
        WaveFile(){}
        WaveFile(std::string fileName, std::string mode, bool verbose=false) {
            bool isReadMode = true;
            std::string rwmode("rb");
            if (mode.find('r') != std::string::npos) {
                rwmode.clear();
                rwmode.assign("rb");
            } else if (mode.find('w') != std::string::npos) {
                rwmode.clear();
                rwmode.assign("wb");
                isReadMode = false;
            } else {
                rwmode.clear();
                rwmode.assign("rb");
            }
            wFile = fopen(fileName.c_str(), rwmode.c_str());
            if (!wFile) {
                return;
            }
            if (isReadMode) {
                // RIFF indicator check
                std::string fHeader;
                char wData[12] = {};
                fread(wData, 1, 12, wFile);
                fHeader.assign(wData, 4);
                if (verbose) {
                    printf("RIFF Header check: %s\n", fHeader.c_str());
                }
                memcpy(fSize.raw, &(wData[4]), 4);
                if (verbose) {
                    printf("File size: %d\n", fSize.value);
                }
                // WAVE indicator check
                std::string fID;
                fID.clear();
                fID.assign(&(wData[8]), 4);
                if (verbose) {
                    printf("WAVE ID check: %s\n", fID.c_str());
                }
                char rawChunkID[4] = {};
                //size_t readSize = 0;
                while (true) {
                    //readSize = fread(rawChunkID, 1, 4, wFile);
                    fread(rawChunkID, 1, 4, wFile);
                    if (feof(wFile)) {
                        if (verbose) {
                            printf("End of file.\n");
                        }
                        break;
                    }
                    std::string chunkID(rawChunkID, 4);
                    union {
                        char raw[4] = {};
                        uint32_t data;
                    } chunkSize;
                    fread(chunkSize.raw, 1 ,4, wFile);
                    if (verbose) {
                        printf("Chunk ID: %s, Chunk size: %d\n", chunkID.c_str(), chunkSize.data);
                    }
                    
                    if (chunkID.find("fmt") != std::string::npos) {
                        //printf("Format chunk found.\n");
                        char* chunkData = nullptr;
                        chunkData = new char[chunkSize.data];
                        fread(chunkData, 1, chunkSize.data, wFile);
                        if (feof(wFile)) {
                            if (verbose) {
                                printf("End of file.\n");
                            }
                            delete[] chunkData;
                            break;
                        }
                        memcpy(wFormat.raw, chunkData, 2);
                        memcpy(nChannels.raw, &(chunkData[2]), 2);
                        memcpy(nSPS.raw, &(chunkData[4]), 4);
                        memcpy(nBPS.raw, &(chunkData[8]), 4);
                        nBytesPerSample = nBPS.data /  nSPS.data;
                        
                        switch (wFormat.data) {
                            case 1:
                                if (verbose) {
                                    printf("Format: %d - Signed int (WAVE_FORMAT_PCM)\n", wFormat.data);
                                }
                                isUnsupported = false;
                                break;
                            case 3:
                                if (verbose) {
                                    printf("Format: %d - Float (WAVE_FORMAT_IEEE_FLOAT)\n", wFormat.data);
                                }
                                isUnsupported = false;
                                wfmt = FLOAT_32;
                                break;
                            case 7:
                                if (verbose) {
                                    printf("Format: %d - μ-law (WAVE_FORMAT_MULAW)\n", wFormat.data);
                                }
                                break;
                            case 65534:
                                if (verbose) {
                                    printf("Format: %d - WAVEFORMATEXTENSIBLE\n", wFormat.data);
                                }
                                isUnsupported = false;
                                break;
                            default:
                                if (verbose) {
                                    printf("Format: %d - Unknown\n", wFormat.data);
                                }
                                break;
                        }
                        if (isUnsupported) {
                            printf("Loader Warning: Unsupported data type\n");
                        }
                        if (verbose) {
                            printf("Channels: %d\n", nChannels.data);
                            printf("fs: %d\n", nSPS.data);
                            printf("Bitrate: %dBytes/sec, %9.3fkbits/s\n", nBPS.data, (float)(nBPS.data*8)/1000.0);
                            printf("         %dBytes/(sample*ch)\n", nBytesPerSample);
                        }
                        if (chunkSize.data >= 16) {
                            memcpy(nBlockAlign.raw, &(chunkData[12]), 2);
                            memcpy(nBitsPerSample.raw, &(chunkData[14]), 2);
                            if (verbose) {
                                printf("         %dbits/sample\n", nBitsPerSample.data);
                                printf("Block Align: %d\n", nBlockAlign.data);
                            }
                        }
                        if (chunkSize.data >= 18) {
                            memcpy(cbSize.raw, &(chunkData[16]), 2);
                            if (verbose) {
                                printf("cbSize: %d\n", cbSize.data);
                            }
                        }
                        if (chunkSize.data >= 20) {
                            memcpy(Samples.raw, &(chunkData[18]), 2);
                            if (verbose) {
                                printf("Samples: %d\n", Samples.data);
                            }
                        }
                        if (chunkSize.data >= 24) {
                            memcpy(dwChannelMask.raw, &(chunkData[20]), 4);
                            if (verbose) {
                                printf("dwChannelMask: 0x%08X\n", dwChannelMask.data);
                            }
                        }
                        if (chunkSize.data >= 40) {
                            memcpy(guid, &(chunkData[24]), 16);
                            if (verbose) {
                                printf("Rest data (possibly subformat GUID):\n");
                                for (int tempctr=0; tempctr<16; tempctr++) {
                                    printf("%02X ", guid[tempctr]);
                                }
                            }
                            memcpy(subfmt.raw, guid, 4);
                            printf("\n");
                        }
                        if (subfmt.data == 3) {
                            wfmt = FLOAT_32;
                        }
                        nSingleSampleSize = nBytesPerSample / nChannels.data;
                        if (wfmt != FLOAT_32) {
                            if (verbose) {
                                printf("Data type: ");
                            }
                            switch (nSingleSampleSize) {
                                case 1:
                                    wfmt = SIGNED_8;
                                    if (verbose) {
                                        printf("Signed 8bit\n");
                                    }
                                    break;
                                case 2:
                                    wfmt = SIGNED_16;
                                    if (verbose) {
                                        printf("Signed 16bit\n");
                                    }
                                    break;
                                case 3:
                                    wfmt = SIGNED_24;
                                    if (verbose) {
                                        printf("Signed 24bit\n");
                                    }
                                    break;
                                case 4:
                                    wfmt = SIGNED_32;
                                    if (verbose) {
                                        printf("Signed 32bit\n");
                                    }
                                    break;
                                default:
                                    isUnsupported = true;
                                    if (verbose) {
                                        printf("Unsupported\n");
                                    }
                                    break;
                            };
                            if (verbose) {
                                printf("\n");
                            }
                        } else {
                            if (verbose) {
                                printf("Data type: Float 32bit\n");
                            }
                        }
                        delete[] chunkData;
                        continue;
                    }
                    if (chunkID.find("data") != std::string::npos) {
                        dataChunkPos = ftell(wFile);
                        fseek(wFile, chunkSize.data, SEEK_CUR);
                        dataChunkSize  = chunkSize.data;
                        if (verbose) {
                            printf("Data chunk found - ");
                            printf("Position: %ld\n", dataChunkPos);
                        }
                        continue;
                    }
                    fseek(wFile, chunkSize.data, SEEK_CUR);
                }
                fseek(wFile, dataChunkPos, SEEK_SET);
                tempRawData = new char[nBytesPerSample];
                //printf("DEBUG:\n  tempRawData: %p\n", tempRawData);
            }
        }
        virtual ~WaveFile() {
            if (!isClosed && wFile) {
                //printf("DEBUG (to close):\n  wFile: %p\n", wFile);
                fclose(wFile);
                isClosed = true;
            }
            if (tempRawData) {
                //printf("DEBUG (to free):\n  tempRawData: %p\n", tempRawData);
                delete[] tempRawData;
            }
        }
        uint32_t getSampleFreq() {
            return (int)nSPS.data;
        }
        int getFormat() {
            return (int)wfmt;
        }
        int getChannels() {
            return (int)nChannels.data;
        }
        uint32_t read(float* dest, uint32_t length) {
            if (!wFile) {
                return 0;
            }
            WaveData wData;
            size_t readCount = 0;
            size_t readSize = 0;
            float tempData = 0;
            if (!tempRawData) {
                return 0;
            }
            for (uint32_t ctr=0; (ctr<length) && !isEndOfFile(); ctr++) {
                if (readSizeCount >= dataChunkSize) {
                    isWaveDataEnd = true;
                    break;
                }
                readSize = fread(tempRawData, 1, nBytesPerSample, wFile);
                if (readSize == 0) {
                    break;
                }
                readSizeCount += readSize;
                for (int chCount=0; chCount<nChannels.data; chCount++) {
                    memcpy(wData.s8, &(tempRawData[chCount*nSingleSampleSize]), nSingleSampleSize);
                    switch (wfmt) {
                        case SIGNED_8:
                        tempData = (float)wData.s8[0] / 128.0;
                            break;
                        case SIGNED_16:
                            tempData = (float)wData.s16[0] / 32768.0;
                            break;
                        case SIGNED_24:
                            tempData = (float)(wData.s32 << 8) / 2147483648.0;
                            break;
                        case SIGNED_32:
                            tempData = (float)(wData.s32) / 2147483648.0;
                            break;
                        case FLOAT_32:
                            tempData = wData.f32;
                            break;
                        default:
                            tempData = 0.0;
                            break;
                    }
                    dest[(ctr*nChannels.data)+chCount] = tempData;
                }
                readCount++;
            }
            return readCount;
        }
        uint32_t write(float* src, uint32_t length) {
            if (!wFile) {
                return 0;
            }
            return fwrite(src, nBytesPerSample, length, wFile);
        }
        uint32_t getDataSize() {
            return dataChunkSize; 
        }
        uint32_t getPosition() {
            return readSizeCount;
        }
        float getPositionInSeconds(){
            return (float)(readSizeCount / (nBytesPerSample)) / nSPS.data;
        }
        float getLengthInSeconds() {
            return (float)(dataChunkSize / (nBytesPerSample)) / nSPS.data;
        }
        bool isFileOpened() {
            if (!wFile) {
                return false;
            }
            return true;
        }
        bool isEndOfFile() {
            if (feof(wFile) != 0) {
                return true;
            }
            return false;
        }
        bool isEndOfData() {
            return isWaveDataEnd;
        }
        void rewind() {
            fseek(wFile, dataChunkPos, SEEK_SET);
            readSizeCount = 0;
            isWaveDataEnd = false;
        }
};

#endif
