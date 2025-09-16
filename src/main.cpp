#include "stdio.h"
#include "stdlib.h"
#include "getopt.h"
#include "signal.h"
#include "stdint.h"

#include "math.h"

#include <string>
#include <atomic>
#include <vector>
#include <filesystem>
#include <algorithm>

//#include "nlohmann/json.hpp"

#include "AudioManipulator.hpp"
#include "WaveLoader.hpp"
#include "MatrixFader.hpp"

class GaplessLooper : public WaveFile {
    public:
        GaplessLooper(std::string fileName, bool verbose=false): WaveFile(fileName, "r", verbose) {}
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

std::atomic<bool> KeyboardInterrupt;
GaplessLooper* curWF = nullptr;
void kbiHandler(int signo) {
    KeyboardInterrupt.store(true);
    if (curWF) {
        curWF->abortRequest();
    }
}

template <typename DTYPE>
void printRatBar(DTYPE fillLength, DTYPE maxLength,
                 int charLength=10, bool withPercent=false,
                 char fillChar='#', char emptyChar='_',
                 bool colored=false, bool revcolor=false) {
    float fillRatio = 0.0;
    int fillCount = 0;
    if (fillLength > 0) {
        fillRatio = (float)fillLength / (float)maxLength;
    }
    fillCount = (int)(fillRatio * (float)charLength);
    bool halfMarked = false;
    bool fourFifthMarked = false;
    bool almostFullMarked = false;
    for (int ctr = 0; ctr < fillCount; ctr++) {
        if (colored) {
            if ((ctr < (charLength/2)) && !halfMarked) {
                if (revcolor) {
                    printf("\x1b[042m\x1b[097m");
                } else {
                    printf("\x1b[041m\x1b[097m");
                } 
                halfMarked = true;
            }
            if (((charLength/2) <= ctr) && (ctr < ((4*charLength)/5)) && !fourFifthMarked) {
                printf("\x1b[0m");
                printf("\x1b[043m\x1b[097m");
                fourFifthMarked = true;
            }
            if ( (((4*charLength)/5) <= ctr) && !almostFullMarked ) {
                printf("\x1b[0m");
                if (revcolor) {
                    printf("\x1b[041m\x1b[097m");
                } else {
                    printf("\x1b[042m\x1b[097m");
                }
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
                 char fillChar='#', char emptyChar='_',
                 bool colored=false, bool revcolor=false) {
    double fillRatio = 0.0;
    int fillCount = 0;
    if (fillLength > 0) {
        fillRatio = fillLength / maxLength;
    }
    fillCount = (int)(fillRatio * (double)charLength);
    bool halfMarked = false;
    bool fourFifthMarked = false;
    bool almostFullMarked = false;
    for (int ctr = 0; ctr < fillCount; ctr++) {
        if (colored) {
            if ((ctr < (charLength/2)) && !halfMarked) {
                if (revcolor) {
                    printf("\x1b[042m\x1b[097m");
                } else {
                    printf("\x1b[041m\x1b[097m");
                } 
                halfMarked = true;
            }
            if (((charLength/2) <= ctr) && (ctr < ((4*charLength)/5)) && !fourFifthMarked) {
                printf("\x1b[0m");
                printf("\x1b[043m\x1b[097m");
                fourFifthMarked = true;
            }
            if ( (((4*charLength)/5) <= ctr) && !almostFullMarked ) {
                printf("\x1b[0m");
                if (revcolor) {
                    printf("\x1b[041m\x1b[097m");
                } else {
                    printf("\x1b[042m\x1b[097m");
                }
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

void showHelp() {
    printf("args:\n--help, --list-devices, --loadonly, --verbose, --noloop,\n"
           "--output-device, --chunklength, --rblength, --file, --directory\n\n");
    printf("--help                        : Show this help\n"
           "--list-devices                : Show sound devices and exit.\n"
           "--loadonly                    : Only load wave file (and exit without playing).\n"
           "--noloop                      : Don't loop file if set.\n"
           "--verbose                     : Show additional information.\n"
           "--output-device <index: int>  : Set sound output device to device No.<index>.\n"
           "                                index can be retrieved with function --list-devices.\n"
           "--chunklength <length: int>   : Set chunk length to <length>.\n"
           "                                chunk length is the sample length which application reads from file in once.\n"
           "                                detail: chunklength = (the number of sample) * (the number of audio channel)\n"
           "--rblength <length: int>      : Set ring buffer length to <length>.\n"
           "                                at least (chunklength * 2) shuld be set.\n"
           "--file <filename: str>        : Set file name to load.\n"
           "--directory <directory: str>  : Set directory to load.\n"
           );
}

void displayInformation(AudioManipulator& aOut, GaplessLooper& wf,
                        int readLength, int barLength, float wPeak) {
    float dbwPeak = 0.0;
    float dbPos = 0.0;
    constexpr float dbMin = -24.0;
    puts("\r\033[3A");
    printRatBar(aOut.getRbStoredChunkLength(), aOut.getRbChunkLength(), barLength, true, '*', ' ', true);
    printf("|%6d|%6lu|%9lu|%9lu|\n", readLength, aOut.getTxCbFrameCount(), aOut.getRbStoredLength(), aOut.getRbLength());
    // print read position
    printRatBar(wf.getPosition(), wf.getDataSize(), barLength, false, '-', ' ');
    printf("|%6.1f / %6.1f\n", wf.getPositionInSeconds(), wf.getLengthInSeconds());
    // print peak
    dbwPeak = 20*log10(wPeak);
    if (wPeak > 0) {
        dbPos = dbMin - dbwPeak;
        dbPos /= dbMin;
    }

    printRatBar(dbPos, 1.0f, barLength, false, '>', ' ', true, true);
    printf("|%6.1f", dbwPeak);
    fflush(stdout);
}

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
        {"verbose", no_argument, 0, 8001},
        {"directory", required_argument, 0, 9001},
        {0, 0, 0, 0}
    };
    int getoptStatus = 0;
    int optionIndex = 0;
    bool loadonly = false;
    bool noLoop = false;
    bool dirMode = false;
    bool verbose = false;
    std::string fileName;
    std::string dirName;
    uint32_t oDeviceIndex = 0;
    uint32_t ioChunkLength = 1024;
    uint32_t ioRBLength = ioChunkLength*8;
    do {
        getoptStatus = getopt_long(argc, argv, "", long_options, &optionIndex);
        switch (getoptStatus) {
            case '?':
                return -1;
            case 'h':
                showHelp();
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
            case 8001:
                verbose = true;
                break;
            case 9001:
                dirName.assign(optarg);
                dirMode = true;
                break;
            default:
                break;
        }
    } while (getoptStatus != -1);

    std::vector<std::string> paths;
    if (dirMode) {
        for (const std::filesystem::directory_entry& dirinfo : std::filesystem::directory_iterator(dirName)) {
            std::string path(dirinfo.path().c_str());
            std::string lcpath((std::string::size_type)path.length(), 0);
            for (std::string::size_type sidx=0; sidx < path.length(); sidx++) {
                lcpath.at(sidx) = std::tolower(path.at(sidx));
            }
            if (lcpath.find(".wav") != std::string::npos) {
                paths.push_back(path.c_str());
            }
        }
        std::sort(paths.begin(), paths.end());
        curWF = new GaplessLooper(paths.at(0), verbose);
    } else {
        curWF = new GaplessLooper(fileName, verbose);
    }

    if (!curWF->isFileOpened()) {
        if (dirMode){
            printf("Cannot open file: %s\n", paths.at(0).c_str());
        } else {
            printf("Cannot open file: %s\n", fileName.c_str());
        }
        return -1;
    }
    if (!(curWF->isWaveFile())) {
        printf("Error: It's not WAVE file.\n");
        return -1;
    }
    if (loadonly) {
        return 0;
    }

    AudioManipulator aOut(oDeviceIndex, "o",
                          (double)curWF->getSampleFreq(), "f32", 2,
                          ioRBLength, ioChunkLength);

    if (!aOut.isDeviceAvailable()) {
        printf("Device not available.\n");
        return -1;
    }

    AudioData* aData = nullptr;
    aData = new AudioData[ioChunkLength*curWF->getChannels()];
    if (!aData) {
        printf("Cannot allocate read buffer\n");
        return -1;
    }
    for (uint32_t ctr=0; ctr<(ioChunkLength*curWF->getChannels()); ctr++) {
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
    if (curWF->getChannels() < 2) {
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
    float wABS = 0;
    std::size_t playedFileCount = 0;
    if (dirMode) { //ファイル名の表示: 下の '\033[3A'で3行分上書きされるため改行を追加
        printf("File: %s\n\n\n\n", paths.at(0).c_str());
    } else {
        printf("File: %s\n\n\n\n", fileName.c_str());
    }
    while (!KeyboardInterrupt.load()) {
        wPeak = 0;
        if (dirMode) {
            readLength = curWF->prepareFrame(&(aData[0].f32), ioChunkLength, true);
        } else {
            readLength = curWF->prepareFrame(&(aData[0].f32), ioChunkLength, noLoop);
        }
        if (readLength < ioChunkLength) {
            if (dirMode) {
                playedFileCount++;
                if (playedFileCount < paths.size()) {
                    GaplessLooper* prevWF = nullptr;
                    prevWF = curWF;
                    printf("\nFile: %s\n\n\n\n", paths.at(playedFileCount).c_str());
                    curWF = new GaplessLooper(paths.at(playedFileCount), verbose);
                    curWF->prepareFrame(&(aData[readLength*prevWF->getChannels()].f32), ioChunkLength-readLength, true);
                    readLength = ioChunkLength;
                    delete prevWF;
                } else {
                    memset((float*)&(aData[readLength*curWF->getChannels()].f32),
                        0,
                        sizeof(float)*(ioChunkLength-readLength)*curWF->getChannels());
                    if (!noLoop) {
                        readLength = ioChunkLength;
                    }
                }
            } else {
                memset((float*)&(aData[readLength*curWF->getChannels()].f32),
                        0,
                        sizeof(float)*(ioChunkLength-readLength)*curWF->getChannels());
            }
        }
        //AudioManipulator::deinterleave(aData, deint, ioChunkLength);
        //AudioManipulator::interleave(deint, aData, ioChunkLength);
        // get peak
        for (uint32_t ctr=0; ctr<(ioChunkLength*aOut.getChannelCount()); ctr++) {
            wABS = aData[ctr].f32;
            if (wABS < 0) {
                wABS *= -1;
            }
            if (wPeak < wABS) {
                wPeak = wABS;
            }
        }

        // print information
        displayInformation(aOut, *curWF, readLength, barLength, wPeak);
        // write audio data to audio output
        aOut.blockingWrite(aData, readLength, 1000);

        if (readLength < ioChunkLength) {
            break;
        }
        if (dirMode && (playedFileCount >= paths.size())) {
            if (noLoop) {
                break;
            } else {
                delete curWF;
                playedFileCount = 0;
                curWF = new GaplessLooper(paths.at(playedFileCount), verbose);
                printf("\nFile: %s\n\n\n\n", paths.at(playedFileCount).c_str());
            }
        }
    }
    KeyboardInterrupt.store(false);
    while (aOut.wait(50) != 0) {
        displayInformation(aOut, *curWF, readLength, barLength, wPeak);
        if (KeyboardInterrupt.load()) {
            break;
        }
    }
    displayInformation(aOut, *curWF, readLength, barLength, wPeak);
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
    if (curWF) {
        delete curWF;
    }

    printf("Audio output closing...\n");
    aOut.close();
    printf("Audio output closed\n");

    aOut.terminate();
    printf("Audio output terminated.\n");
    printf("Exit.\n");

    return 0;
}
