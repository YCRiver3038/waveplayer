#ifndef AUDIO_MANIPULATOR_H_INCLUDED
#define AUDIO_MANIPULATOR_H_INCLUDED

#include "portaudio.h"
#include "buffers.hpp"
#include <cmath>
#include "time.h"
#include <vector>

typedef union {
    int8_t s8[4];
    int16_t s16[2];
    int32_t s32;
    float f32;
} AudioData;

typedef union {
    int32_t s32[2];
    float f32[2];
} ChunkData2x32;

int rxCallback( const void *input,
                void *output,
                unsigned long frameCount,
                const PaStreamCallbackTimeInfo* timeInfo,
                PaStreamCallbackFlags statusFlags,
                void *userData );
int txCallback( const void *input,
                void *output,
                unsigned long frameCount,
                const PaStreamCallbackTimeInfo* timeInfo,
                PaStreamCallbackFlags statusFlags,
                void *userData );

class AudioManipulator {
    private:
        PaError initStatus = 0;
        PaError openStatus = 0;
        PaError streamStatus = 0;
        PaError stopStatus = paNoError;
        PaError closeStatus = paNoError;
        const PaDeviceInfo* devInfo = nullptr;
        PaStreamParameters parameters = {0};
        double fs = 44100;
        bool output = false;
        int nCH = 0;
        unsigned long rbLen = 0;
        PaStream* aStream = nullptr;
        ring_buffer<AudioData>* dataBuf = nullptr;
        bool writeReady = false;
        std::vector<int> inputList;
        std::vector<int> outputList;
        bool isPaused = true;
        bool isStopped = true;
        bool isClosed = false;
        bool isTerminated = false;
        bool isOpened = false;
        unsigned long txCbFrameCount = 0;
        unsigned long rxCbFrameCount = 0;
        unsigned int lengthFactor = 1;
        AudioData* zerodata = nullptr;
        unsigned long zdlength = 0;

    public:
        unsigned long iFrameCount = 0;
        unsigned long oFrameCount = 0;
        unsigned int sampleBytesSize = 0;

        void storeTxCbFrameCount(unsigned long fc) {
            txCbFrameCount = fc;
        }
        void storeRxCbFrameCount(unsigned long fc) {
            rxCbFrameCount = fc;
        }
        unsigned long getTxCbFrameCount() {
            return txCbFrameCount;
        }
        unsigned long getRxCbFrameCount(){
            return rxCbFrameCount;
        }
        void initPortAudio() {
            parameters.hostApiSpecificStreamInfo = nullptr;
            //printf("PortAudio - Initializing...\n");
            initStatus = Pa_Initialize();
            printf("\nPortAudio - Init status: %s (%d)\n", Pa_GetErrorText(initStatus), initStatus);
        }
        AudioManipulator() { // Only initialize PortAudio
            initPortAudio();
        }
        AudioManipulator(const int index, std::string dir,
                         const double fSample, const std::string format,
                         const int channels, const unsigned long ringBufLength, const unsigned long chunkLength) {
            initPortAudio();

            parameters.device = index;
            devInfo = Pa_GetDeviceInfo(index);
            fs = fSample;
            
            zdlength = chunkLength*channels;
            zerodata = new AudioData[zdlength];
            for (uint32_t ctr=0; ctr<zdlength; ctr++) {
                zerodata[ctr].s32 = 0;
            }

            if ((dir.compare("o") == 0) || (dir.compare("O") == 0)) {
                output = true;
            } else {
                output = false;
            }
            if (format.compare("8") == 0) {
                parameters.sampleFormat = paInt8;
                sampleBytesSize = 1;
            } else if (format.compare("16") == 0) {
                parameters.sampleFormat = paInt16;
                sampleBytesSize = 2;
            } else if (format.compare("32") == 0) {
                parameters.sampleFormat = paInt32;
                sampleBytesSize = 4;
            } else if (format.compare("f32") == 0) {
                parameters.sampleFormat = paFloat32;
                sampleBytesSize = 4;
            } else {
                parameters.sampleFormat = paFloat32;
                sampleBytesSize = 4;
            }
            lengthFactor = 4 / sampleBytesSize;

            if (output) {
                parameters.suggestedLatency = (double)chunkLength / fSample;
                nCH = channels;
                if (devInfo->maxOutputChannels < channels) {
                    nCH = devInfo->maxOutputChannels;
                }
                parameters.channelCount = nCH;
                openStatus = Pa_OpenStream(&aStream, nullptr, &parameters, fs,
                            0, paNoFlag, txCallback, this);
            } else {
                parameters.suggestedLatency = (double)chunkLength / fSample;
                nCH = channels;
                if (devInfo->maxInputChannels < channels) {
                    nCH = devInfo->maxInputChannels;
                    printf("Warning: Available input channel is less than specified: Changed to %d Channels\n", nCH);
                }
                parameters.channelCount = nCH;
                openStatus = Pa_OpenStream(&aStream, &parameters, nullptr, fs,
                            0, paNoFlag, rxCallback, this);
            }
            //printf("DEBUG:\n  aStream: %p\n", aStream);
            if (openStatus != 0) {
                fprintf(stderr, "PortAudio - opening stream failed: %s ( %d )\n", Pa_GetErrorText(openStatus), openStatus);
                initStatus = openStatus;
                return;
            }
            rbLen = ringBufLength*nCH;
            dataBuf = new ring_buffer<AudioData>(rbLen);
            //printf("DEBUG:\n  dataBuf: %p\n", dataBuf);
            isOpened = true;
        }
        ~AudioManipulator(){
            //wait(10000);
            isOpened = false;
            if (!aStream) {
                return;
            }
            close();
            terminate();
            if (dataBuf) {
                //printf("DEBUG(to free):\n  dataBuf:%p\n", dataBuf);
                delete dataBuf;
                dataBuf = nullptr;
            }
            if (zerodata) {
                delete[] zerodata;
            }
        }

        bool isDeviceAvailable() {
            return isOpened;
        }

        void close() {
            if (isClosed) {
                return;
            }
            if (initStatus == paNoError) {
                //printf("PortAudio - Terminatig...\n");
                stop();
                closeStatus = Pa_CloseStream(aStream);
            }
            isClosed = true;
        }
        void terminate() {
            if (isTerminated) {
                return;
            }
            if (initStatus == paNoError) {
                initStatus = Pa_Terminate();
                printf("PortAudio - Terminate status: %s (%d)\n",
                       Pa_GetErrorText(initStatus), initStatus);
            }
            isTerminated = true;
        }
        void start() {
            if (initStatus != paNoError) {
                return;
            }
            if (dataBuf) {
                dataBuf->init_buffer();
            }
            streamStatus = Pa_StartStream(aStream);
            if (streamStatus != paNoError) {
                fprintf(stderr, "PortAudio - Failed to start stream: %s ( %d )\n",
                       Pa_GetErrorText(streamStatus), streamStatus);
                isStopped = true;
                return;
            }
            isPaused = false;
            isStopped = false;
        }

        void stop() {
            isPaused = true;
            if (isStopped) {
                return;
            }
            if (!aStream) {
                return;
            }
            stopStatus = Pa_StopStream(aStream);
            if (stopStatus != paNoError) {
                fprintf(stderr, "PortAudio - Failed to stop stream: %s ( %d )\n",
                       Pa_GetErrorText(stopStatus), stopStatus);
            }
            isStopped = true;
        }

        void pause() {
            isPaused = true;
        }
        void resume() {
            isPaused = false;
        }
        int wait(int timeout=10000) {
            timespec sleepTime = {};
            sleepTime.tv_nsec = 1000000; //1msec
            int timeoutCount = 0;
            while (getRbStoredChunkLength() > 0) {
                nanosleep(&sleepTime, nullptr);
                timeoutCount++;
                if (timeoutCount > timeout) {
                    return -1;
                }
            }
            return 0;
        }

        void setWriteReady() {
            writeReady = true;
        }
        void setWriteNotReady() {
            writeReady = false;
        }

        int write(AudioData* src, uint32_t length) {
            uint32_t remain = 0;
            remain = getRbChunkLength() - getRbStoredChunkLength();
            if (openStatus != paNoError) {
                return -1;
            }
            if (!dataBuf) {
                return -1;
            }
            if (remain > length) {
                dataBuf->put_data_memcpy(src, length*nCH/lengthFactor);
                //dataBuf->put_data_arr_queue(src, length*nCH*lengthFactor);
                return 0;
            }
            if (remain > 0) {
                dataBuf->put_data_memcpy(src, remain*nCH/lengthFactor);
                return 0;
            }
            return 0;
        }

        int blockingWrite(AudioData* src, uint32_t length, long timeout=100) {
            if (openStatus != paNoError) {
                return -1;
            }
            timespec sleepTime = {};
            sleepTime.tv_nsec = 1000000; //1msec
            long timeoutCount = 0;
            while (getRbStoredChunkLength() >= (getRbChunkLength()-length)) {
                nanosleep(&sleepTime, nullptr);
                timeoutCount += 1;
                if (timeoutCount > timeout) {
                    return -1;
                }
            }
            write(src, length);
            return 0;
        }

        int read(AudioData* dest, uint32_t length, bool zeros=false) {
            uint32_t remain = 0;
            remain = getRbStoredChunkLength();
            if (openStatus != paNoError) {
                return -1;
            }
            if (!dataBuf) {
                return -1;
            }
            if (zeros) {
                for (uint32_t ctr=0; ctr<(length*nCH); ctr++) {
                    dest[ctr].s32 = 0;
                }
                return 0;
            }
            if (remain >= length) {
                memcpy(dest, dataBuf->get_data_memcpy(length*nCH/lengthFactor),
                             length*nCH*sizeof(AudioData)/lengthFactor);
                //memcpy(dest, dataBuf->get_data_nelm_queue(length*nCH), length*nCH*sizeof(AudioData));
                return 0;
            }
            if (remain != 0) {
                memcpy(dest,
                       dataBuf->get_data_memcpy(remain*nCH/lengthFactor),
                       remain*nCH*sizeof(AudioData)/lengthFactor);
                return 0;
            }
            if ((length*nCH) < zdlength) {
                memcpy(dest, zerodata,
                       length*nCH*sizeof(AudioData)/lengthFactor);
                return 0;
            }
            memcpy(dest, zerodata,
                   zdlength*sizeof(AudioData)/lengthFactor);

            return 0;
        }

        int getInitStatus() {
            return static_cast<int>(initStatus);
        }

        int getChannelCount() {
            return nCH;
        }

        bool isStreamPaused() {
            return isPaused;
        }

        unsigned long getRbLengthInBytes() {
            return sizeof(AudioData)*rbLen;
        }
        
        unsigned long getRbLengthInTotalSamples() {
            return rbLen;
        }

        unsigned long getRbLength() {
            return dataBuf->get_buf_length();
        }

        unsigned long getRbChunkLength() {
            if (!dataBuf) {
                return -1;
            }
            return dataBuf->get_buf_length() / nCH;
        }

        unsigned long getRbStoredLength() {
            if (!dataBuf) {
                return -1;
            }
            return dataBuf->get_stored_length();
        }
        unsigned long getRbStoredChunkLength() {
            if (!dataBuf) {
                return -1;
            }
            return dataBuf->get_stored_length() / nCH;
        }
        
        void listInputDevices() {
            int deviceCount = 0;
            deviceCount = Pa_GetDeviceCount();
            for (int ctr = 0; ctr < deviceCount; ctr++) {
                devInfo = Pa_GetDeviceInfo(ctr);
                if (devInfo->maxInputChannels > 0) {
                    inputList.push_back(ctr);
                }
            }
            const PaHostApiInfo* hostAPIInfo = nullptr;
            printf("\n--- Input List ---\n");
            for (std::vector<int>::size_type ctr=0; ctr<inputList.size(); ctr++) {
                devInfo = Pa_GetDeviceInfo(inputList.at(ctr));
                hostAPIInfo = Pa_GetHostApiInfo(devInfo->hostApi);
                printf("Index: %d, API:%s, Name: %s\nfs default: %8.0lf[Hz]\n",
                        inputList.at(ctr), hostAPIInfo->name, devInfo->name, devInfo->defaultSampleRate);
                printf("max input channels: %d\n", devInfo->maxInputChannels);
                printf("Input low latency default:  %6.2lf[msec]\n",
                        (devInfo->defaultLowInputLatency != -1 ?
                         devInfo->defaultLowInputLatency*1000 : std::nan("1")));
                printf("Input high latency default: %6.2lf[msec]\n\n",
                        (devInfo->defaultHighInputLatency != -1 ?
                         devInfo->defaultHighInputLatency*1000 : std::nan("1")));
            }
        }

        void listOutputDevices() {
            int deviceCount = 0;
            deviceCount = Pa_GetDeviceCount();
            for (int ctr = 0; ctr < deviceCount; ctr++) {
                devInfo = Pa_GetDeviceInfo(ctr);
                if (devInfo->maxOutputChannels > 0) {
                    outputList.push_back(ctr);
                }
            }
            const PaHostApiInfo* hostAPIInfo = nullptr;
            printf("\n--- Output List ---\n");
            for (std::vector<int>::size_type ctr=0; ctr<outputList.size(); ctr++) {
                devInfo = Pa_GetDeviceInfo(outputList.at(ctr));
                hostAPIInfo = Pa_GetHostApiInfo(devInfo->hostApi);
                printf("Index: %d, API: %s, Name: %s\nfs default: %8.0lf[Hz]\n",
                        outputList.at(ctr), hostAPIInfo->name, devInfo->name, devInfo->defaultSampleRate);
                printf("max output channels: %d\n", devInfo->maxOutputChannels);
                printf("Output low latency default:  %6.2lf[msec]\n",
                        (devInfo->defaultLowInputLatency != -1 ?
                         devInfo->defaultLowInputLatency*1000 : std::nan("1")));
                printf("Output high latency default: %6.2lf[msec]\n\n",
                        (devInfo->defaultHighInputLatency != -1 ?
                         devInfo->defaultHighInputLatency*1000 : std::nan("1")));
            }  
        }
    
        void listDevices() {
            int deviceCount = 0;
            deviceCount = Pa_GetDeviceCount();
            for (int ctr = 0; ctr < deviceCount; ctr++) {
                devInfo = Pa_GetDeviceInfo(ctr);
                if (devInfo->maxOutputChannels > 0) {
                    outputList.push_back(ctr);
                }
                if (devInfo->maxInputChannels > 0) {
                    inputList.push_back(ctr);
                }
            }
            const PaHostApiInfo* hostAPIInfo = nullptr;
            printf("\n--- Input List ---\n");
            for (std::vector<int>::size_type ctr=0; ctr<inputList.size(); ctr++) {
                devInfo = Pa_GetDeviceInfo(inputList.at(ctr));
                hostAPIInfo = Pa_GetHostApiInfo(devInfo->hostApi);
                printf("Index: %d, API:%s, Name: %s\nfs default: %8.0lf[Hz]\n",
                        inputList.at(ctr), hostAPIInfo->name, devInfo->name, devInfo->defaultSampleRate);
                printf("max input channels: %d\n", devInfo->maxInputChannels);
                printf("Input low latency default:  %6.2lf[msec]\n",
                        (devInfo->defaultLowInputLatency != -1 ?
                         devInfo->defaultLowInputLatency*1000 : std::nan("1")));
                printf("Input high latency default: %6.2lf[msec]\n\n",
                        (devInfo->defaultHighInputLatency != -1 ?
                         devInfo->defaultHighInputLatency*1000 : std::nan("1")));
            }
            printf("\n--- Output List ---\n");
            for (std::vector<int>::size_type ctr=0; ctr<outputList.size(); ctr++) {
                devInfo = Pa_GetDeviceInfo(outputList.at(ctr));
                hostAPIInfo = Pa_GetHostApiInfo(devInfo->hostApi);
                printf("Index: %d, API: %s, Name: %s\nfs default: %8.0lf[Hz]\n",
                        outputList.at(ctr), hostAPIInfo->name, devInfo->name, devInfo->defaultSampleRate);
                printf("max output channels: %d\n", devInfo->maxOutputChannels);
                printf("Output low latency default:  %6.2lf[msec]\n",
                        (devInfo->defaultLowInputLatency != -1 ?
                         devInfo->defaultLowInputLatency*1000 : std::nan("1")));
                printf("Output high latency default: %6.2lf[msec]\n\n",
                        (devInfo->defaultHighInputLatency != -1 ?
                         devInfo->defaultHighInputLatency*1000 : std::nan("1")));
            }
        }
        void getPaVersion() {
            const PaVersionInfo* vInfo;
            vInfo = Pa_GetVersionInfo();
            printf("Version: %s\n", vInfo->versionText);
        }

// static member
        static void deinterleave(AudioData* interleaved, AudioData** deinterleaved,
                                 uint32_t chunkLength, uint32_t channels=2) {
            uint32_t ictr=0;
            for (uint32_t cctr=0; cctr<chunkLength; cctr++) {
                for (uint32_t ch=0; ch<channels; ch++) {
                    deinterleaved[ch][cctr] = interleaved[ictr];
                    ictr++;
                }
            }
            return;
        }

        static void interleave(AudioData** source, AudioData* interleaved,
                               uint32_t chunkLength, uint32_t channels=2) {
            uint32_t ictr=0;
            for (uint32_t cctr=0; cctr<chunkLength; cctr++) {
                for (uint32_t ch=0; ch<channels; ch++) {
                    interleaved[ictr] = source[ch][cctr];
                    ictr++;
                }
            }
            return;
        }
};

int rxCallback( const void *input,
                void *output,
                unsigned long frameCount,
                const PaStreamCallbackTimeInfo* timeInfo,
                PaStreamCallbackFlags statusFlags,
                void *userData ) {
    reinterpret_cast<AudioManipulator*>(userData)->storeRxCbFrameCount(frameCount);
    if (reinterpret_cast<AudioManipulator*>(userData)->isStreamPaused()) {
        return 0;
    }
    reinterpret_cast<AudioManipulator*>(userData)->write((AudioData*)input, frameCount);
    return 0;
}

int txCallback( const void *input,
                void *output,
                unsigned long frameCount,
                const PaStreamCallbackTimeInfo* timeInfo,
                PaStreamCallbackFlags statusFlags,
                void *userData ) {
    reinterpret_cast<AudioManipulator*>(userData)->storeTxCbFrameCount(frameCount);
    if (reinterpret_cast<AudioManipulator*>(userData)->isStreamPaused()) {
        reinterpret_cast<AudioManipulator*>(userData)->read((AudioData*)output, frameCount, true);
        return 0;
    }
    reinterpret_cast<AudioManipulator*>(userData)->read((AudioData*)output, frameCount);
    return 0;
}

#endif