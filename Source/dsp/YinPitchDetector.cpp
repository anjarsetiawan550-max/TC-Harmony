#include "YinPitchDetector.h"
#include <cmath>
#include <algorithm>

void YinPitchDetector::prepare (double newSampleRate, int analysisWindowSize)
{
    sampleRate = newSampleRate;
    windowSize = analysisWindowSize;
    hopSize = windowSize / 4;

    // Ring buffer needs to hold at least windowSize*2 so we always have a
    // full contiguous analysis frame available without reallocating.
    ringBuffer.assign (static_cast<size_t> (windowSize) * 2, 0.0f);
    analysisFrame.assign (static_cast<size_t> (windowSize), 0.0f);
    yinBuffer.assign (static_cast<size_t> (windowSize) / 2, 0.0f);

    writePos = 0;
    samplesSinceLastAnalysis = 0;
    rawFrequencyHz = 0.0f;
    smoothedFrequencyHz = 0.0f;
    voiced = false;
}

bool YinPitchDetector::pushSamples (const float* samples, int numSamples)
{
    bool producedNewEstimate = false;
    const int bufLen = static_cast<int> (ringBuffer.size());

    for (int i = 0; i < numSamples; ++i)
    {
        ringBuffer[static_cast<size_t> (writePos)] = samples[i];
        writePos = (writePos + 1) % bufLen;
        ++samplesSinceLastAnalysis;

        if (samplesSinceLastAnalysis >= hopSize)
        {
            samplesSinceLastAnalysis = 0;

            // Copy out the most recent `windowSize` samples in order.
            int readStart = (writePos - windowSize + bufLen) % bufLen;
            for (int n = 0; n < windowSize; ++n)
                analysisFrame[static_cast<size_t> (n)] = ringBuffer[static_cast<size_t> ((readStart + n) % bufLen)];

            analyse();
            producedNewEstimate = true;
        }
    }

    return producedNewEstimate;
}

void YinPitchDetector::analyse()
{
    const int halfWindow = windowSize / 2;

    // --- Step 1+2: difference function & cumulative mean normalised difference ---
    yinBuffer[0] = 1.0f;
    float runningSum = 0.0f;

    for (int tau = 1; tau < halfWindow; ++tau)
    {
        float sum = 0.0f;
        for (int j = 0; j < halfWindow; ++j)
        {
            const float diff = analysisFrame[static_cast<size_t> (j)] - analysisFrame[static_cast<size_t> (j + tau)];
            sum += diff * diff;
        }

        runningSum += sum;
        yinBuffer[static_cast<size_t> (tau)] = runningSum == 0.0f ? 1.0f : sum * tau / runningSum;
    }

    // --- Step 3: absolute threshold ---
    int tauEstimate = -1;
    for (int tau = 2; tau < halfWindow; ++tau)
    {
        if (yinBuffer[static_cast<size_t> (tau)] < yinThreshold)
        {
            while (tau + 1 < halfWindow && yinBuffer[static_cast<size_t> (tau + 1)] < yinBuffer[static_cast<size_t> (tau)])
                ++tau;
            tauEstimate = tau;
            break;
        }
    }

    if (tauEstimate == -1)
    {
        // No clear periodicity found -> treat as unvoiced (breath, silence, consonant)
        voiced = false;
        return;
    }

    // --- Step 4: parabolic interpolation around the minimum for sub-sample accuracy ---
    float betterTau = static_cast<float> (tauEstimate);
    {
        const int x0 = tauEstimate < 1 ? tauEstimate : tauEstimate - 1;
        const int x2 = tauEstimate + 1 < halfWindow ? tauEstimate + 1 : tauEstimate;

        if (x0 != tauEstimate && x2 != tauEstimate)
        {
            const float s0 = yinBuffer[static_cast<size_t> (x0)];
            const float s1 = yinBuffer[static_cast<size_t> (tauEstimate)];
            const float s2 = yinBuffer[static_cast<size_t> (x2)];
            const float denom = 2.0f * (2.0f * s1 - s2 - s0);
            if (std::abs (denom) > 1.0e-9f)
                betterTau = static_cast<float> (tauEstimate) + (s2 - s0) / denom;
        }
    }

    if (betterTau <= 0.0f)
    {
        voiced = false;
        return;
    }

    const float freq = static_cast<float> (sampleRate) / betterTau;

    // Reject implausible values for a singing voice (keeps octave errors / noise out)
    if (freq < 60.0f || freq > 1200.0f)
    {
        voiced = false;
        return;
    }

    rawFrequencyHz = freq;
    voiced = true;

    // One-pole smoothing so the pitch shifter ratio doesn't zipper
    if (smoothedFrequencyHz <= 0.0f)
        smoothedFrequencyHz = rawFrequencyHz;
    else
        smoothedFrequencyHz += smoothingCoeff * (rawFrequencyHz - smoothedFrequencyHz);
}
