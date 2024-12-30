#ifndef TYPES_H
#define TYPES_H

#include <iostream>
#include <sndfile.h>
#include <vector>
#include <string>
#include <cstring>
#include <cmath>
#include <chrono>
#include "types.h"

using namespace std;

struct ReadThreadArgs {
    string inputFile;
    vector<float>* data;
    SF_INFO fileInfo;
    size_t startFrame;
    size_t numFrames;
    size_t channels;
};

struct BandpassThreadData
{
     vector<float> &data;
     vector<float> &filtered;
     float sampleRate;
     float bandwidth;
     size_t startIdx;
     size_t endIdx;

     BandpassThreadData(vector<float> &data, vector<float> &filtered, float sampleRate, float bandwidth, size_t startIdx, size_t endIdx)
         : data(data), filtered(filtered), sampleRate(sampleRate), bandwidth(bandwidth), startIdx(startIdx), endIdx(endIdx) {}
};

struct NotchThreadData
{
     vector<float> &data;
     vector<float> &filtered;
     float sampleRate;
     float notchFreq;
     int order;
     size_t startIdx;
     size_t endIdx;

     NotchThreadData(vector<float> &data, vector<float> &filtered, float sampleRate, float notchFreq, int order, size_t startIdx, size_t endIdx)
         : data(data), filtered(filtered), sampleRate(sampleRate), notchFreq(notchFreq), order(order), startIdx(startIdx), endIdx(endIdx) {}
};

struct FIRThreadData {
    const vector<float>& data;
    const vector<float>& coefficients;
    vector<float>& filtered;
    size_t startIdx;
    size_t endIdx;

    FIRThreadData(const vector<float>& data, const vector<float>& coefficients, vector<float>& filtered, size_t startIdx, size_t endIdx)
        : data(data), coefficients(coefficients), filtered(filtered), startIdx(startIdx), endIdx(endIdx) {}
};

struct IIRThreadData {
    const vector<float> *data;
    const vector<float> *feedforward;
    vector<float> *partialResults;
    size_t start;
    size_t end;
};

#endif