#pragma once
#include <vector>
#include <cstddef>

/**
    YinPitchDetector
    -----------------
    Real-time fundamental-frequency estimator based on the YIN algorithm
    (de Cheveigne & Kawahara, 2002). Tuned here for short-latency vocal
    pitch tracking: a ~1024-sample analysis window (~23 ms @ 44.1 kHz)
    gives a good trade-off between low latency and tracking accuracy
    for a singing voice (roughly E2-C6).

    Call `process()` once per audio block with the latest dry mic
    samples. It internally keeps a rolling analysis buffer so it does
    not require the block size to match the analysis window.
*/
class YinPitchDetector
{
public:
    YinPitchDetector() = default;

    void prepare (double sampleRate, int analysisWindowSize = 1024);

    // Push new samples (mono, dry vocal) into the rolling buffer.
    // Returns true if a new pitch estimate became available this call.
    bool pushSamples (const float* samples, int numSamples);

    // Latest estimate. 0.0f == unvoiced / no pitch detected.
    float getFrequencyHz() const noexcept { return smoothedFrequencyHz; }
    bool  isVoiced()       const noexcept { return voiced; }

private:
    void analyse();

    double sampleRate = 44100.0;
    int windowSize = 1024;
    float yinThreshold = 0.15f; // standard YIN absolute threshold

    std::vector<float> ringBuffer;
    std::vector<float> analysisFrame;
    std::vector<float> yinBuffer;
    int writePos = 0;
    int samplesSinceLastAnalysis = 0;
    int hopSize = 256; // ~5.8 ms hop @ 44.1kHz -> responsive tracking

    float rawFrequencyHz = 0.0f;
    float smoothedFrequencyHz = 0.0f;
    bool voiced = false;

    // one-pole smoothing to avoid warbling the pitch-shifter ratio
    float smoothingCoeff = 0.35f;
};
