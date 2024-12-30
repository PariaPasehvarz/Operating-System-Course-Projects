#include <iostream>
#include <sndfile.h>
#include <vector>
#include <string>
#include <cstring>
#include <cmath>
#include <chrono>

using namespace std;

void readWavFile(const string &inputFile, vector<float> &data, SF_INFO &fileInfo)
{
     auto start = chrono::high_resolution_clock::now();

     SNDFILE *inFile = sf_open(inputFile.c_str(), SFM_READ, &fileInfo);
     if (!inFile)
     {
          cerr << "Error opening input file: " << sf_strerror(NULL) << endl;
          exit(1);
     }

     data.resize(fileInfo.frames * fileInfo.channels);
     sf_count_t numFrames = sf_readf_float(inFile, data.data(), fileInfo.frames);
     if (numFrames != fileInfo.frames)
     {
          cerr << "Error reading frames from file." << endl;
          sf_close(inFile);
          exit(1);
     }

     sf_close(inFile);
     auto end = chrono::high_resolution_clock::now();

     cout << "Successfully read " << numFrames << " frames from " << inputFile << endl;
     cout << "    Reading time: "
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

void applyBandPassFilter(vector<float> &data, float sampleRate, float bandwidth)
{
     auto start = chrono::high_resolution_clock::now();

     auto step1_start = chrono::high_resolution_clock::now();
     vector<float> filtered(data.size());
     auto step1_end = chrono::high_resolution_clock::now();

     long long total_time_2a = 0, total_time_2b = 0, total_time_2c = 0;
     long long max_time_2a = 0, max_time_2b = 0, max_time_2c = 0;

     auto step2_start = chrono::high_resolution_clock::now();

     for (size_t i = 0; i < data.size(); ++i)
     {
          auto step2a_start = chrono::high_resolution_clock::now();
          float t = static_cast<float>(i) / sampleRate;
          float freq = (t > 0) ? 1.0f / t : 0.0f;
          auto step2a_end = chrono::high_resolution_clock::now();
          long long step2a_time = chrono::duration_cast<chrono::microseconds>(step2a_end - step2a_start).count();

          total_time_2a += step2a_time;
          max_time_2a = max(max_time_2a, step2a_time);

          auto step2b_start = chrono::high_resolution_clock::now();
          float deltaF = bandwidth;
          float h = (freq * freq) / (freq * freq + deltaF * deltaF);
          auto step2b_end = chrono::high_resolution_clock::now();
          long long step2b_time = chrono::duration_cast<chrono::microseconds>(step2b_end - step2b_start).count();

          total_time_2b += step2b_time;
          max_time_2b = max(max_time_2b, step2b_time);

          auto step2c_start = chrono::high_resolution_clock::now();
          filtered[i] = h * data[i];
          auto step2c_end = chrono::high_resolution_clock::now();
          long long step2c_time = chrono::duration_cast<chrono::microseconds>(step2c_end - step2c_start).count();

          total_time_2c += step2c_time;
          max_time_2c = max(max_time_2c, step2c_time);
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

     cout << "    Step 2a (Calculate time and frequency) - Total Time: "
          << total_time_2a << " microseconds, Max Time: "
          << max_time_2a << " microseconds" << endl;

     cout << "    Step 2b (Calculate filter response) - Total Time: "
          << total_time_2b << " microseconds, Max Time: "
          << max_time_2b << " microseconds" << endl;

     cout << "    Step 2c (Apply filter) - Total Time: "
          << total_time_2c << " microseconds, Max Time: "
          << max_time_2c << " microseconds" << endl;

     cout << "    Step 3 (Copy Data Back): "
          << chrono::duration_cast<chrono::microseconds>(step3_end - step3_start).count()
          << " microseconds" << endl;

     cout << "    Total Filtering Time: "
          << chrono::duration_cast<chrono::microseconds>(end - start).count()
          << " microseconds" << endl;
}

void applyNotchFilter(vector<float> &data, float sampleRate, float notchFreq, int order)
{
     auto start = chrono::high_resolution_clock::now();

     auto step1_start = chrono::high_resolution_clock::now();
     vector<float> filtered(data.size());
     auto step1_end = chrono::high_resolution_clock::now();

     long long total_time_2a = 0, total_time_2b = 0;
     long long max_time_2a = 0, max_time_2b = 0;

     auto step2_start = chrono::high_resolution_clock::now();

     for (size_t i = 0; i < data.size(); ++i)
     {
          auto step2a_start = chrono::high_resolution_clock::now();
          float t = static_cast<float>(i) / sampleRate;
          float freq = (t > 0) ? 1.0f / t : 0.0f;
          auto step2a_end = chrono::high_resolution_clock::now();
          long long step2a_time = chrono::duration_cast<chrono::microseconds>(step2a_end - step2a_start).count();

          total_time_2a += step2a_time;
          max_time_2a = max(max_time_2a, step2a_time);

          auto step2b_start = chrono::high_resolution_clock::now();
          float h = 1.0f / (pow(freq / notchFreq, 2 * order) + 1);
          auto step2b_end = chrono::high_resolution_clock::now();
          long long step2b_time = chrono::duration_cast<chrono::microseconds>(step2b_end - step2b_start).count();

          total_time_2b += step2b_time;
          max_time_2b = max(max_time_2b, step2b_time);

          filtered[i] = h * data[i];
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

     cout << "    Step 2a (Calculate time and frequency) - Total Time: "
          << total_time_2a << " microseconds, Max Time: "
          << max_time_2a << " microseconds" << endl;

     cout << "    Step 2b (Apply notch filter) - Total Time: "
          << total_time_2b << " microseconds, Max Time: "
          << max_time_2b << " microseconds" << endl;

     cout << "    Step 3 (Copy Data Back): "
          << chrono::duration_cast<chrono::microseconds>(step3_end - step3_start).count()
          << " microseconds" << endl;

     cout << "    Total Filtering Time: "
          << chrono::duration_cast<chrono::microseconds>(end - start).count()
          << " microseconds" << endl;
}

void applyFIRFilter(vector<float> &data, const vector<float> &coefficients)
{
     auto start = chrono::high_resolution_clock::now();

     auto step1_start = chrono::high_resolution_clock::now();
     vector<float> filtered(data.size(), 0.0f);
     size_t filterLength = coefficients.size();
     auto step1_end = chrono::high_resolution_clock::now();

     long long total_time_inner_loop = 0;
     long long max_time_inner_loop = 0;

     auto step2_start = chrono::high_resolution_clock::now();

     for (size_t i = 0; i < data.size(); ++i)
     {
          auto inner_loop_start = chrono::high_resolution_clock::now();

          for (size_t j = 0; j < filterLength; ++j)
          {
               if (i >= j)
               {
                    filtered[i] += coefficients[j] * data[i - j];
               }
          }

          auto inner_loop_end = chrono::high_resolution_clock::now();
          long long inner_loop_time = chrono::duration_cast<chrono::microseconds>(inner_loop_end - inner_loop_start).count();

          total_time_inner_loop += inner_loop_time;
          max_time_inner_loop = max(max_time_inner_loop, inner_loop_time);
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

     cout << "        Inner Loop - Total Time: "
          << total_time_inner_loop << " microseconds, Max Time: "
          << max_time_inner_loop << " microseconds" << endl;

     cout << "    Step 3 (Copy Data Back): "
          << chrono::duration_cast<chrono::microseconds>(step3_end - step3_start).count()
          << " microseconds" << endl;

     cout << "    Total Filtering Time: "
          << chrono::duration_cast<chrono::microseconds>(end - start).count()
          << " microseconds" << endl;
}

void applyIIRFilter(vector<float> &data, const vector<float> &feedforward, const vector<float> &feedback)
{
    auto start = chrono::high_resolution_clock::now();

    auto step1_start = chrono::high_resolution_clock::now();
    vector<float> filtered(data.size(), 0.0f);
    auto step1_end = chrono::high_resolution_clock::now();

    long long total_time_2a = 0, total_time_2b = 0;
    long long max_time_2a = 0, max_time_2b = 0;

    auto step2_start = chrono::high_resolution_clock::now();

    for (size_t i = 0; i < data.size(); ++i)
    {
        auto step2a_start = chrono::high_resolution_clock::now();
        for (size_t j = 0; j < feedforward.size(); ++j)
        {
            if (i >= j)
            {
                filtered[i] += feedforward[j] * data[i - j];
            }
        }
        auto step2a_end = chrono::high_resolution_clock::now();
        long long step2a_time = chrono::duration_cast<chrono::microseconds>(step2a_end - step2a_start).count();

        total_time_2a += step2a_time;
        max_time_2a = max(max_time_2a, step2a_time);

        auto step2b_start = chrono::high_resolution_clock::now();
        for (size_t j = 1; j < feedback.size(); ++j)
        {
            if (i >= j)
            {
                filtered[i] -= feedback[j] * filtered[i - j];
            }
        }
        auto step2b_end = chrono::high_resolution_clock::now();
        long long step2b_time = chrono::duration_cast<chrono::microseconds>(step2b_end - step2b_start).count();

        total_time_2b += step2b_time;
        max_time_2b = max(max_time_2b, step2b_time);
    }

    auto step2_end = chrono::high_resolution_clock::now();

    auto step3_start = chrono::high_resolution_clock::now();
    data = filtered;
    auto step3_end = chrono::high_resolution_clock::now();

    auto end = chrono::high_resolution_clock::now();

    cout << "IIR Filtering time: " << endl;

    cout << "    Step 1 (Initialization): "
         << chrono::duration_cast<chrono::microseconds>(step1_end - step1_start).count()
         << " microseconds" << endl;

    cout << "    Step 2 (Processing): "
         << chrono::duration_cast<chrono::microseconds>(step2_end - step2_start).count()
         << " microseconds" << endl;

    cout << "    Step 2a (Feedforward computation) - Total Time: "
         << total_time_2a << " microseconds, Max Time: "
         << max_time_2a << " microseconds" << endl;

    cout << "    Step 2b (Feedback computation) - Total Time: "
         << total_time_2b << " microseconds, Max Time: "
         << max_time_2b << " microseconds" << endl;

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
