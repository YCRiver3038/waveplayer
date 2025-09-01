#ifndef MTX_H_INCLUDED
#define MTX_H_INCLUDED

#include "stdint.h"
#include "stdlib.h"
#include "stdio.h"
#include "math.h"

#include <vector>

constexpr float inf = std::numeric_limits<float>::infinity();
constexpr double infd = std::numeric_limits<double>::infinity();

class MatrixFader {
    private:
        uint32_t numInputs = 0;
        uint32_t numOutputs = 0;
        float* inputBuf = nullptr;
        float* outputBuf = nullptr;
        float** cpGains = nullptr;
        float* inputGains = nullptr;
        float* outputGains = nullptr;

    public:
        MatrixFader(uint32_t inputs, uint32_t outputs) {
            numInputs = inputs;
            numOutputs = outputs;
            if ((numInputs == 0) | (numOutputs == 0)) {
                return;
            }
            // allocate inputGains[inputCH]
            inputGains = new float[numInputs];
            if (!inputGains) {
                return;
            }
            // allocate outputGains[outputCH]
            outputGains = new float[numOutputs];
            if (!outputGains) {
                return;
            }
            // allocate cpGains[inputCH][outputCH]
            cpGains = new float*[numInputs];
            if (!cpGains) {
                return;
            }
            bool allocErr = false;
            for (uint32_t ctr=0; ctr<numInputs; ctr++) {
                cpGains[ctr] = new float[numOutputs];
                if (!cpGains[ctr]) {
                    allocErr = true;
                    break;
                }
            }
            if (allocErr) {
                for (uint32_t ctr=0; ctr<numInputs; ctr++) {
                    if (cpGains) {
                        delete[] cpGains[ctr];
                    }
                }
                delete[] cpGains;
            }
            for (uint32_t ictr=0; ictr<numInputs; ictr++) {
                for (uint32_t octr=0; octr<numOutputs; octr++) {
                    cpGains[ictr][octr] = -inf;
                }
            }
        }
        ~MatrixFader() {
            if (inputGains) {
                delete[] inputGains;
            }
            if (outputGains) {
                delete[] outputGains;
            }
            if (inputBuf) {
                delete[] inputBuf;
            }
            if (outputBuf) {
                delete[] outputBuf;
            }
            if (cpGains) {
                for (uint32_t ictr=0; ictr<numInputs; ictr++) {
                    delete[] cpGains[ictr];
                }
                delete[] cpGains;
            }
        }

        void setCrossPointGain(uint32_t idxIn, uint32_t idxOut, float gainDB) {
            if (cpGains) {
                cpGains[idxIn][idxOut] = gainDB;
            }
        }

        void setInputGain(uint32_t idxIn, float gainDB) {
            if (idxIn > numInputs) {
                return;
            }
            inputGains[idxIn] = powf(10, gainDB/20.0);
        }

        void setOutputGain(uint32_t idxOut, float gainDB) {
            if (idxOut > numInputs) {
                return;
            }
            outputGains[idxOut] = powf(10, gainDB/20.0);
        }

        void mix(float** inputDataArr, uint32_t inputDataLength,
                     float** outputDataArr, uint32_t outputDataLength) {
            if (!cpGains) {
                return;
            }
            if (!inputDataArr) {
                return;
            }
            if (!outputDataArr) {
                return;
            }
            for (uint32_t octr=0; octr < numOutputs; octr++) {
                for (uint32_t odctr=0; odctr<outputDataLength; odctr++) {
                    outputDataArr[octr][odctr] = 0;
                    // add input data to output
                    for (uint32_t ictr=0; ictr<numInputs; ictr++) {
                        if (odctr >= inputDataLength) {
                            break;
                        }
                        outputDataArr[octr][odctr] += (inputDataArr[ictr][odctr]*inputGains[ictr]);
                    }
                    // apply output volume
                    outputDataArr[octr][odctr] *= outputGains[octr];
                }
            }
        }

};

#endif
