#include "stdio.h"
#include "stdlib.h"
#include "getopt.h"
#include "signal.h"
#include "stdint.h"

#include "math.h"

#include <string>
#include <atomic>

//#include "nlohmann/json.hpp"

#include "AudioManipulator.hpp"
#include "WaveLoader.hpp"
#include "MatrixFader.hpp"

std::atomic<bool> KeyboardInterrupt;
void kbiHandler(int signo) {
    KeyboardInterrupt.store(true);
}

template <typename DTYPE>
void printRatBar(DTYPE fillLength, DTYPE maxLength,
                 int charLength=10, bool withPercent=false,
                 char fillChar='#', char emptyChar='_', bool colored=false) {
    float fillRatio = 0.0;
    int fillCount = 0;
    fillRatio = (float)fillLength / (float)maxLength;
    fillCount = (int)(fillRatio * (float)charLength);
    bool halfMarked = false;
    bool fourFifthMarked = false;
    bool almostFullMarked = false;
    for (int ctr = 0; ctr < fillCount; ctr++) {
        if (colored) {
            if ((ctr < (charLength/2)) && !halfMarked) {
                printf("\x1b[041m\x1b[097m");
                halfMarked = true;
            }
            if (((charLength/2) <= ctr) && (ctr < ((4*charLength)/5)) && !fourFifthMarked) {
                printf("\x1b[0m");
                printf("\x1b[043m\x1b[097m");
                fourFifthMarked = true;
            }
            if ( (((4*charLength)/5) <= ctr) && !almostFullMarked ) {
                printf("\x1b[0m");
                printf("\x1b[042m\x1b[097m");
                almostFullMarked = true;
            }
        }
        putchar(fillChar);
    }
    if (colored) {
        printf("\x1b[0m");
    }
    for (int ctr = 0; ctr < (charLength - fillCount); ctr++) {
        putchar(emptyChar);
    }
    if (withPercent) {
        printf("|%5.1f%%", fillRatio*100.0);
    }
}

void printRatBar(float fillLength, float maxLength,
                 int charLength=10, bool withPercent=false,
                 char fillChar='#', char emptyChar='_', bool colored=false) {
    float fillRatio = 0.0;
    int fillCount = 0;
    fillRatio = fillLength / maxLength;
    fillCount = (int)(fillRatio * charLength);
    bool halfMarked = false;
    bool fourFifthMarked = false;
    bool almostFullMarked = false;
    for (int ctr = 0; ctr < fillCount; ctr++) {
        if (colored) {
            if ((ctr < (charLength/2)) && !halfMarked) {
                printf("\x1b[041m\x1b[097m");
                halfMarked = true;
            }
            if (((charLength/2) <= ctr) && (ctr < ((4*charLength)/5)) && !fourFifthMarked) {
                printf("\x1b[0m");
                printf("\x1b[043m\x1b[097m");
                fourFifthMarked = true;
            }
            if ( (((4*charLength)/5) <= ctr) && !almostFullMarked ) {
                printf("\x1b[0m");
                printf("\x1b[042m\x1b[097m");
                almostFullMarked = true;
            }
        }
        putchar(fillChar);
    }
    if (colored) {
        printf("\x1b[0m");
    }
    for (int ctr = 0; ctr < (charLength - fillCount); ctr++) {
        putchar(emptyChar);
    }
    if (withPercent) {
        printf("|%5.1f%%", fillRatio*100.0);
    }
}

void printRatBar(double fillLength, double maxLength,
                 int charLength=10, bool withPercent=false,
                 char fillChar='#', char emptyChar='_', bool colored=false) {
    float fillRatio = 0.0;
    int fillCount = 0;
    fillRatio = fillLength / maxLength;
    fillCount = (int)(fillRatio * charLength);
    bool halfMarked = false;
    bool fourFifthMarked = false;
    bool almostFullMarked = false;
    for (int ctr = 0; ctr < fillCount; ctr++) {
        if (colored) {
            if ((ctr < (charLength/2)) && !halfMarked) {
                printf("\x1b[041m\x1b[097m");
                halfMarked = true;
            }
            if (((charLength/2) <= ctr) && (ctr < ((4*charLength)/5)) && !fourFifthMarked) {
                printf("\x1b[0m");
                printf("\x1b[043m\x1b[097m");
                fourFifthMarked = true;
            }
            if ( (((4*charLength)/5) <= ctr) && !almostFullMarked ) {
                printf("\x1b[0m");
                printf("\x1b[042m\x1b[097m");
                almostFullMarked = true;
            }
        }
        putchar(fillChar);
    }
    if (colored) {
        printf("\x1b[0m");
    }
    for (int ctr = 0; ctr < (charLength - fillCount); ctr++) {
        putchar(emptyChar);
    }
    if (withPercent) {
        printf("|%5.1f%%", fillRatio*100.0);
    }
}

class GaplessLooper : public WaveFile {
    public:
        GaplessLooper(std::string fileName): WaveFile(fileName, "r") {}
        uint32_t prepareFrame(float* dest, uint32_t chunkLength, bool noloop=false) {
            if (!isFileOpened()) {
                return 0;
            }
            uint32_t readLength = 0;
            readLength = read(dest, chunkLength);
            if (noloop) {
                return readLength;
            }
            if (readLength < chunkLength) {
                rewind();
                readLength = read(&(dest[readLength*getChannels()]), chunkLength-readLength);
            }
            return chunkLength;
        }
};

int main(int argc, char* argv[]) {
#if defined(__linux__) || defined(__APPLE__)
    struct sigaction sa = {};
    sa.sa_handler = kbiHandler;
    sigaction(SIGINT, &sa, nullptr);
#else
    signal(SIGINT, kbiHandler);
#endif

    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"list-devices", no_argument, 0, 1000},
        {"loadonly", no_argument, 0, 1001},
        {"noloop", no_argument, 0, 1002},
        {"output-device", required_argument, 0, 2000},
        {"chunklength", required_argument, 0, 2001},
        {"file", required_argument, 0, 2002},
        {"rblength", required_argument, 0, 2003},
        {0, 0, 0, 0}
    };
    int getoptStatus = 0;
    int optionIndex = 0;
    bool loadonly = false;
    bool noLoop = false;
    std::string fileName;
    uint32_t oDeviceIndex = 0;
    uint32_t ioChunkLength = 1024;
    uint32_t ioRBLength = ioChunkLength*8;
    do {
        getoptStatus = getopt_long(argc, argv, "", long_options, &optionIndex);
        switch (getoptStatus) {
            case 'h':
                printf("args: --help, --list-devices, --loadonly --output-device, --chunklength, --rblength, --file\n");
                return 0;
            case 1000: {
                AudioManipulator initOnly;
                initOnly.listDevices();
                return 0;
            }
            case 1001:
                loadonly = true;
                break;
            case 1002:
                noLoop = true;
                break;
            case 2000:
                try {
                    oDeviceIndex = std::stoi(std::string(optarg));
                } catch (const std::invalid_argument& e) {
                    printf("Output: Invalid index ( %s )\n", optarg);
                    return -1;
                }
                break;
            case 2001:
                try {
                    ioChunkLength = std::stoi(std::string(optarg));
                } catch (const std::invalid_argument& e) {
                    printf("Invalid length ( %s )\n", optarg);
                    return -1;
                }
                break;
            case 2002:
                fileName.assign(optarg);
                break;
            case 2003:
                try {
                    ioRBLength = std::stoi(std::string(optarg));
                } catch (const std::invalid_argument& e) {
                    printf("Invalid length ( %s )\n", optarg);
                    return -1;
                }
                break;
            default:
                break;
        }
    } while (getoptStatus != -1);

    GaplessLooper wf1(fileName);
    if (!wf1.isFileOpened()) {
        printf("Cannot open file: %s\n", fileName.c_str());
        return -1;
    }

    if (loadonly) {
        return 0;
    }

    AudioManipulator aOut(oDeviceIndex, "o",
                          (double)wf1.getSampleFreq(), "f32", 2,
                          ioRBLength, ioChunkLength);

    if (!aOut.isDeviceAvailable()) {
        printf("Device not available.\n");
        return -1;
    }

    AudioData* aData = nullptr;
    aData = new AudioData[ioChunkLength*wf1.getChannels()];
    if (!aData) {
        printf("Cannot allocate read buffer\n");
        return -1;
    }
    for (uint32_t ctr=0; ctr<(ioChunkLength*wf1.getChannels()); ctr++) {
        aData[ctr].f32 = 0.0;
    }

    putc('\n', stdout);
    uint32_t readLength = 0;
    aOut.start();
    aOut.blockingWrite(aData, ioChunkLength, 1000);
    int barLength = 50;
    uint32_t mfInputs = 16;
    uint32_t mfOutputs = 16;
    MatrixFader mf1(mfInputs, mfOutputs);
    if (wf1.getChannels() < 2) {
        mf1.setCrossPointGain(0, 0, 0.0);
        mf1.setCrossPointGain(0, 1, 0.0);
    }

    int interleaveCH = 2;
    AudioData** deint = nullptr;
    deint = (AudioData**)calloc(interleaveCH, sizeof(AudioData*));
    for (int ctr=0; ctr < interleaveCH; ctr++) {
        deint[ctr] = new AudioData[ioChunkLength];
    }

    float wPeak = 0;
    float dbwPeak = 0;
    float wABS = 0;
    while (!KeyboardInterrupt.load()) {
        puts("\r\033[3A");
        wPeak = 0;
        readLength = wf1.prepareFrame(&(aData[0].f32), ioChunkLength, noLoop);
        AudioManipulator::deinterleave(aData, deint, ioChunkLength);
        AudioManipulator::interleave(deint, aData, ioChunkLength);
        // get peak
        for (uint32_t ctr=0; ctr<(ioChunkLength*wf1.getChannels()); ctr++) {
            wABS = aData[ctr].f32;
            if (wABS < 0) {
                wABS *= -1;
            }
            if (wPeak < wABS) {
                wPeak = wABS;
            }
        }
        if (noLoop && (readLength < ioChunkLength)) {
            memset((float*)&(aData[readLength*wf1.getChannels()].f32),
                   0,
                   sizeof(float)*(ioChunkLength-readLength)*wf1.getChannels());
        }
        // print buffer status
        printRatBar(aOut.getRbStoredChunkLength(), aOut.getRbChunkLength(), barLength, true, '*', ' ', true);
        printf("|%6d|%6lu|%9lu|%9lu|\n", readLength, aOut.getTxCbFrameCount(), aOut.getRbStoredLength(), aOut.getRbLength());
        // print read position
        printRatBar(wf1.getPosition(), wf1.getDataSize(), barLength, false, '-', ' ');
        printf("|%6.1f / %6.1f\n", wf1.getPositionInSeconds(), wf1.getLengthInSeconds());
        // print peak
        dbwPeak = 20*log10(wPeak);
        printRatBar(wPeak, 1.0f, barLength, false, '>', ' ');
        printf("|%6.1f", dbwPeak);
        fflush(stdout);
        aOut.blockingWrite(aData, readLength, 1000);
        if (noLoop && (readLength < ioChunkLength)) {
            break;
        }
    }
    KeyboardInterrupt.store(false);
    while (aOut.wait(50) != 0) {
        puts("\r\033[3A");
        printRatBar(aOut.getRbStoredChunkLength(), aOut.getRbChunkLength(), barLength, true, '*', ' ', true);
        printf("|%6d|%6lu|%9lu|%9lu|\n", readLength, aOut.getTxCbFrameCount(), aOut.getRbStoredLength(), aOut.getRbLength());
        printRatBar(wf1.getPosition(), wf1.getDataSize(), barLength, false, '-', ' ');
        printf("|%6.1f / %6.1f\n", wf1.getPositionInSeconds(), wf1.getLengthInSeconds());
        fflush(stdout);
        if (KeyboardInterrupt.load()) {
            break;
        }
    }
    puts("\r\033[3A");
    printRatBar(aOut.getRbStoredChunkLength(), aOut.getRbChunkLength(), barLength, true, '*', ' ', true);
    printf("|%6d|%6lu|%9lu|%9lu|\n", readLength, aOut.getTxCbFrameCount(), aOut.getRbStoredLength(), aOut.getRbLength());
    printRatBar(wf1.getPosition(), wf1.getDataSize(), barLength, false, '-', ' ');
    printf("|%6.1f / %6.1f", wf1.getPositionInSeconds(), wf1.getLengthInSeconds());
    puts("\n");
    if (KeyboardInterrupt.load()) {
        printf("\nKeyboardInterrupt.\n");
    }

    printf("Stopping audio output...\n");
    aOut.stop();
    printf("Audio output stopped.\n");

    // delete deinterleaved data
    for (int ctr=0; ctr < interleaveCH; ctr++) {
        delete[] deint[ctr];
    }
    free(deint);
    //if (aData) {
        delete[] aData;
    //}

    printf("Audio output closing...\n");
    aOut.close();
    printf("Audio output closed\n");

    aOut.terminate();
    printf("Audio output terminated.\n");
    printf("Exit.\n");

    return 0;
}
