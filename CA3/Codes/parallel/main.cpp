#include <iostream>
#include <sndfile.h>
#include <vector>
#include <string>
#include <cstring>
#include <cmath>
#include <chrono>
#include "types.h"

using namespace std;

#include <sndfile.h>
#include <pthread.h>
#include <vector>
#include <string>
#include <iostream>
#include <chrono>

using namespace std;

void *readChunk(void *args)
{
     ReadThreadArgs *threadArgs = static_cast<ReadThreadArgs *>(args);

     SNDFILE *inFile = sf_open(threadArgs->inputFile.c_str(), SFM_READ, &threadArgs->fileInfo);
     if (!inFile)
     {
          cerr << "Error opening input file in thread: " << sf_strerror(NULL) << endl;
          pthread_exit(nullptr);
     }

     sf_seek(inFile, threadArgs->startFrame, SEEK_SET);

     sf_count_t framesRead = sf_readf_float(inFile, threadArgs->data->data() + threadArgs->startFrame * threadArgs->channels, threadArgs->numFrames);

     if (framesRead != threadArgs->numFrames)
     {
          cerr << "Error or EOF reached while reading in thread." << endl;
     }

     sf_close(inFile);
     pthread_exit(nullptr);
}

void readWavFile(const string &inputFile, vector<float> &data, SF_INFO &fileInfo)
{
     size_t numThreads = 4;
     auto start = chrono::high_resolution_clock::now();

     SNDFILE *inFile = sf_open(inputFile.c_str(), SFM_READ, &fileInfo);
     if (!inFile)
     {
          cerr << "Error opening input file: " << sf_strerror(NULL) << endl;
          exit(1);
     }

     size_t totalFrames = fileInfo.frames;
     size_t channels = fileInfo.channels;

     data.resize(totalFrames * channels);

     sf_close(inFile);

     vector<pthread_t> threads(numThreads);
     vector<ReadThreadArgs> threadArgs(numThreads);

     size_t framesPerThread = totalFrames / numThreads;

     for (size_t i = 0; i < numThreads; ++i)
     {
          threadArgs[i].inputFile = inputFile;
          threadArgs[i].data = &data;
          threadArgs[i].fileInfo = fileInfo;
          threadArgs[i].startFrame = i * framesPerThread;
          threadArgs[i].numFrames = (i == numThreads - 1) ? (totalFrames - i * framesPerThread) : framesPerThread;
          threadArgs[i].channels = channels;

          pthread_create(&threads[i], nullptr, readChunk, &threadArgs[i]);
     }

     for (size_t i = 0; i < numThreads; ++i)
     {
          pthread_join(threads[i], nullptr);
     }

     auto end = chrono::high_resolution_clock::now();
     cout << "Successfully read " << totalFrames << " frames from " << inputFile << endl;
     cout << "    Reading time (multithreaded): "
          << chrono::duration_cast<chrono::microseconds>(end - start).count()
          << " microseconds\n"
          << endl;
}

void writeWavFile(const string &outputFile, const vector<float> &data, SF_INFO &fileInfo)
{
     auto start = chrono::high_resolution_clock::now();

     sf_count_t originalFrames = fileInfo.frames;
     SNDFILE *outFile = sf_open(outputFile.c_str(), SFM_WRITE, &fileInfo);
     if (!outFile)
     {
          cerr << "Error opening output file: " << sf_strerror(NULL) << endl;
          exit(1);
     }
     sf_count_t numFrames = sf_writef_float(outFile, data.data(), originalFrames);
     if (numFrames != originalFrames)
     {
          cerr << "Error writing frames to file." << endl;
          sf_close(outFile);
          exit(1);
     }

     sf_close(outFile);
     auto end = chrono::high_resolution_clock::now();

     cout << "    Successfully wrote " << numFrames << " frames to " << outputFile << endl;
     cout << "    Writing time: "
          << chrono::duration_cast<chrono::microseconds>(end - start).count()
          << " microseconds\n"
          << endl;
}

void *processBandpassFilterChunk(void *arg)
{
     BandpassThreadData *threadData = (BandpassThreadData *)arg;
     vector<float> &data = threadData->data;
     vector<float> &filtered = threadData->filtered;
     float sampleRate = threadData->sampleRate;
     float bandwidth = threadData->bandwidth;

     for (size_t i = threadData->startIdx; i < threadData->endIdx; ++i)
     {
          float t = static_cast<float>(i) / sampleRate;
          float freq = (t > 0) ? 1.0f / t : 0.0f;
          float deltaF = bandwidth;
          float h = (freq * freq) / (freq * freq + deltaF * deltaF);
          filtered[i] = h * data[i];
     }

     return nullptr;
}

void applyBandPassFilter(vector<float> &data, float sampleRate, float bandwidth)
{
     auto start = chrono::high_resolution_clock::now();

     auto step1_start = chrono::high_resolution_clock::now();
     vector<float> filtered(data.size());
     auto step1_end = chrono::high_resolution_clock::now();

     int numThreads = 4;
     pthread_t threads[numThreads];
     BandpassThreadData *threadData[numThreads];

     size_t chunkSize = data.size() / numThreads;

     auto step2_start = chrono::high_resolution_clock::now();

     for (int i = 0; i < numThreads; ++i)
     {
          threadData[i] = new BandpassThreadData(data, filtered, sampleRate, bandwidth, i * chunkSize, (i == numThreads - 1) ? data.size() : (i + 1) * chunkSize);
          pthread_create(&threads[i], nullptr, processBandpassFilterChunk, (void *)threadData[i]);
     }

     for (int i = 0; i < numThreads; ++i)
     {
          pthread_join(threads[i], nullptr);
          delete threadData[i];
     }

     auto step2_end = chrono::high_resolution_clock::now();

     auto step3_start = chrono::high_resolution_clock::now();
     data = filtered;
     auto step3_end = chrono::high_resolution_clock::now();

     auto end = chrono::high_resolution_clock::now();

     cout << "Bandpass Filtering time: " << endl;
     cout << "    Step 1 (Initialization): "
          << chrono::duration_cast<chrono::microseconds>(step1_end - step1_start).count()
          << " microseconds" << endl;

     cout << "    Step 2 (Processing): "
          << chrono::duration_cast<chrono::microseconds>(step2_end - step2_start).count()
          << " microseconds" << endl;

     cout << "    Step 3 (Copy Data Back): "
          << chrono::duration_cast<chrono::microseconds>(step3_end - step3_start).count()
          << " microseconds" << endl;

     cout << "    Total Filtering Time: "
          << chrono::duration_cast<chrono::microseconds>(end - start).count()
          << " microseconds" << endl;
}

void *processNotchChunk(void *arg)
{
     NotchThreadData *threadData = (NotchThreadData *)arg;
     vector<float> &data = threadData->data;
     vector<float> &filtered = threadData->filtered;
     float sampleRate = threadData->sampleRate;
     float notchFreq = threadData->notchFreq;
     int order = threadData->order;

     for (size_t i = threadData->startIdx; i < threadData->endIdx; ++i)
     {
          float t = static_cast<float>(i) / sampleRate;
          float freq = (t > 0) ? 1.0f / t : 0.0f;
          float h = 1.0f / (pow(freq / notchFreq, 2 * order) + 1);
          filtered[i] = h * data[i];
     }

     return nullptr;
}

void applyNotchFilter(vector<float> &data, float sampleRate, float notchFreq, int order)
{
     auto start = chrono::high_resolution_clock::now();

     auto step1_start = chrono::high_resolution_clock::now();
     vector<float> filtered(data.size());
     auto step1_end = chrono::high_resolution_clock::now();

     int numThreads = 4;
     pthread_t threads[numThreads];
     NotchThreadData *threadData[numThreads];

     size_t chunkSize = data.size() / numThreads;

     auto step2_start = chrono::high_resolution_clock::now();
     for (int i = 0; i < numThreads; ++i)
     {
          threadData[i] = new NotchThreadData(data, filtered, sampleRate, notchFreq, order, i * chunkSize, (i == numThreads - 1) ? data.size() : (i + 1) * chunkSize);
          pthread_create(&threads[i], nullptr, processNotchChunk, (void *)threadData[i]);
     }

     for (int i = 0; i < numThreads; ++i)
     {
          pthread_join(threads[i], nullptr);
          delete threadData[i];
     }

     auto step2_end = chrono::high_resolution_clock::now();

     auto step3_start = chrono::high_resolution_clock::now();
     data = filtered;
     auto step3_end = chrono::high_resolution_clock::now();

     auto end = chrono::high_resolution_clock::now();

     cout << "Notch Filtering time: " << endl;
     cout << "    Step 1 (Initialization): "
          << chrono::duration_cast<chrono::microseconds>(step1_end - step1_start).count()
          << " microseconds" << endl;

     cout << "    Step 2 (Processing): "
          << chrono::duration_cast<chrono::microseconds>(step2_end - step2_start).count()
          << " microseconds" << endl;

     cout << "    Step 3 (Copy Data Back): "
          << chrono::duration_cast<chrono::microseconds>(step3_end - step3_start).count()
          << " microseconds" << endl;

     cout << "    Total Filtering Time: "
          << chrono::duration_cast<chrono::microseconds>(end - start).count()
          << " microseconds" << endl;
}

void *processFIRChunk(void *arg)
{
     FIRThreadData *threadData = (FIRThreadData *)arg;
     const vector<float> &data = threadData->data;
     const vector<float> &coefficients = threadData->coefficients;
     vector<float> &filtered = threadData->filtered;
     size_t filterLength = coefficients.size();

     for (size_t i = threadData->startIdx; i < threadData->endIdx; ++i)
     {
          for (size_t j = 0; j < filterLength; ++j)
          {
               if (i >= j)
               {
                    filtered[i] += coefficients[j] * data[i - j];
               }
          }
     }

     return nullptr;
}

void applyFIRFilter(vector<float> &data, const vector<float> &coefficients)
{
     auto start = chrono::high_resolution_clock::now();

     auto step1_start = chrono::high_resolution_clock::now();
     vector<float> filtered(data.size(), 0.0f);
     auto step1_end = chrono::high_resolution_clock::now();

     int numThreads = 4;
     pthread_t threads[numThreads];
     FIRThreadData *threadData[numThreads];

     size_t chunkSize = data.size() / numThreads;

     auto step2_start = chrono::high_resolution_clock::now();

     for (int i = 0; i < numThreads; ++i)
     {
          threadData[i] = new FIRThreadData(data, coefficients, filtered, i * chunkSize,
                                            (i == numThreads - 1) ? data.size() : (i + 1) * chunkSize);
          pthread_create(&threads[i], nullptr, processFIRChunk, (void *)threadData[i]);
     }

     for (int i = 0; i < numThreads; ++i)
     {
          pthread_join(threads[i], nullptr);
          delete threadData[i];
     }

     auto step2_end = chrono::high_resolution_clock::now();

     auto step3_start = chrono::high_resolution_clock::now();
     data = filtered;
     auto step3_end = chrono::high_resolution_clock::now();

     auto end = chrono::high_resolution_clock::now();

     cout << "FIR Filtering time: " << endl;
     cout << "    Step 1 (Initialization): "
          << chrono::duration_cast<chrono::microseconds>(step1_end - step1_start).count()
          << " microseconds" << endl;

     cout << "    Step 2 (Convolution Loop): "
          << chrono::duration_cast<chrono::microseconds>(step2_end - step2_start).count()
          << " microseconds" << endl;

     cout << "    Step 3 (Copy Data Back): "
          << chrono::duration_cast<chrono::microseconds>(step3_end - step3_start).count()
          << " microseconds" << endl;

     cout << "    Total Filtering Time: "
          << chrono::duration_cast<chrono::microseconds>(end - start).count()
          << " microseconds" << endl;
}

void *computeFeedforward(void *arg)
{
     IIRThreadData *args = (IIRThreadData *)arg;

     for (size_t i = args->start; i < args->end; ++i)
     {
          for (size_t j = 0; j < args->feedforward->size(); ++j)
          {
               if (i >= j)
               {
                    (*args->partialResults)[i] += (*args->feedforward)[j] * (*args->data)[i - j];
               }
          }
     }

     pthread_exit(nullptr);
}

void applyIIRFilter(vector<float> &data, const vector<float> &feedforward, const vector<float> &feedback)
{
     auto start = chrono::high_resolution_clock::now();

     auto step1_start = chrono::high_resolution_clock::now();
     vector<float> filtered(data.size(), 0.0f);
     auto step1_end = chrono::high_resolution_clock::now();

     auto step2a_start = chrono::high_resolution_clock::now();

     size_t numThreads = 4; 
     vector<pthread_t> threads(numThreads);
     vector<IIRThreadData> threadArgs(numThreads);

     size_t chunkSize = data.size() / numThreads;

     for (size_t i = 0; i < numThreads; ++i)
     {
          size_t startIdx = i * chunkSize;
          size_t endIdx = (i == numThreads - 1) ? data.size() : startIdx + chunkSize;

          threadArgs[i] = {&data, &feedforward, &filtered, startIdx, endIdx};
          if (pthread_create(&threads[i], nullptr, computeFeedforward, &threadArgs[i]) != 0)
          {
               cerr << "Error creating thread " << i << endl;
               exit(1);
          }
     }

     for (size_t i = 0; i < numThreads; ++i)
     {
          pthread_join(threads[i], nullptr);
     }

     auto step2a_end = chrono::high_resolution_clock::now();

     auto step2b_start = chrono::high_resolution_clock::now();

     for (size_t i = 0; i < data.size(); ++i)
     {
          for (size_t j = 1; j < feedback.size(); ++j)
          {
               if (i >= j)
               {
                    filtered[i] -= feedback[j] * filtered[i - j];
               }
          }
     }

     auto step2b_end = chrono::high_resolution_clock::now();

     auto step3_start = chrono::high_resolution_clock::now();
     data = filtered;
     auto step3_end = chrono::high_resolution_clock::now();

     auto end = chrono::high_resolution_clock::now();

     cout << "IIR Filtering Time: " << endl;

     cout << "    Step 1 (Initialization): "
          << chrono::duration_cast<chrono::microseconds>(step1_end - step1_start).count()
          << " microseconds" << endl;

     cout << "    Step 2a (Feedforward computation): "
          << chrono::duration_cast<chrono::microseconds>(step2a_end - step2a_start).count()
          << " microseconds" << endl;

     cout << "    Step 2b (Feedback computation): "
          << chrono::duration_cast<chrono::microseconds>(step2b_end - step2b_start).count()
          << " microseconds" << endl;

     cout << "    Step 3 (Copy Data Back): "
          << chrono::duration_cast<chrono::microseconds>(step3_end - step3_start).count()
          << " microseconds" << endl;

     cout << "    Total Filtering Time: "
          << chrono::duration_cast<chrono::microseconds>(end - start).count()
          << " microseconds" << endl;
}

int main(int argc, char *argv[])
{
     if (argc != 2)
     {
          cerr << "Usage: " << argv[0] << " <input_file>" << endl;
          return 1;
     }

     string inputFile = argv[1];

     string bandpassFilterOutputFile = "output_bandpass_filtered.wav";
     string notchFilterOutputFile = "output_notch_filtered.wav";
     string firFilterOutputFile = "output_fir_filtered.wav";
     string iirFilterOutputFile = "output_iir_filtered.wav";

     SF_INFO bandpassFilterFileInfo, notchFilterFileInfo, FIRFilterFileInfo, IIRFilterFileInfo;
     vector<float> audioData;

     memset(&bandpassFilterFileInfo, 0, sizeof(bandpassFilterFileInfo));
     readWavFile(inputFile, audioData, bandpassFilterFileInfo);
     vector<float> audioDataBandpass = audioData;
     float bandwidth = 100.0f;
     applyBandPassFilter(audioDataBandpass, bandpassFilterFileInfo.samplerate, bandwidth);
     writeWavFile(bandpassFilterOutputFile, audioDataBandpass, bandpassFilterFileInfo);

     memset(&notchFilterFileInfo, 0, sizeof(notchFilterFileInfo));
     readWavFile(inputFile, audioData, notchFilterFileInfo);
     vector<float> audioDataNotch = audioData;
     float notchFreq = 1000.0f;
     int order = 2;
     applyNotchFilter(audioDataNotch, notchFilterFileInfo.samplerate, notchFreq, order);
     writeWavFile(notchFilterOutputFile, audioDataNotch, notchFilterFileInfo);

     memset(&FIRFilterFileInfo, 0, sizeof(FIRFilterFileInfo));
     readWavFile(inputFile, audioData, FIRFilterFileInfo);
     vector<float> audioDataFIR = audioData;
     vector<float> firCoefficients = {0.1, 0.15, 0.5, 0.15, 0.1};
     applyFIRFilter(audioData, firCoefficients);
     writeWavFile(firFilterOutputFile, audioData, FIRFilterFileInfo);

     memset(&IIRFilterFileInfo, 0, sizeof(IIRFilterFileInfo));
     readWavFile(inputFile, audioData, IIRFilterFileInfo);
     vector<float> iirFeedforward = {0.5, 0.25};
     vector<float> iirFeedback = {1.0, -0.75};
     applyIIRFilter(audioData, iirFeedforward, iirFeedback);
     writeWavFile(iirFilterOutputFile, audioData, IIRFilterFileInfo);

     return 0;
}
