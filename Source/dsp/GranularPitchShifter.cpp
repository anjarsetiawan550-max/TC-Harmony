#include "GranularPitchShifter.h"
#include <cmath>
#include <algorithm>

namespace
{
    constexpr float PI_F = 3.14159265358979323846f;

    inline int wrapIndex (int i, int len) noexcept
    {
        i %= len;
        if (i < 0) i += len;
        return i;
    }

    inline float wrapFloat (float v, float len) noexcept
    {
        while (v < 0.0f)  v += len;
        while (v >= len)  v -= len;
        return v;
    }
}

float GranularPitchShifter::hannWindow (float phase01) noexcept
{
    return 0.5f - 0.5f * std::cos (2.0f * PI_F * phase01);
}

void GranularPitchShifter::prepare (double newSampleRate, int grainSizeSamplesIn)
{
    sampleRate = newSampleRate;
    grainSizeSamples = grainSizeSamplesIn;

    // Generous margin above 2x grain length so an upward-shifted grain
    // (faster read speed) never catches up with the write pointer.
    inputBufferLen = grainSizeSamples * 6;
    inputBuffer.assign (static_cast<size_t> (inputBufferLen), 0.0f);

    reset();
}

void GranularPitchShifter::reset()
{
    std::fill (inputBuffer.begin(), inputBuffer.end(), 0.0f);
    inputWriteIndex = 0;
    validSamplesWritten = 0;

    age0 = 0;
    age1 = grainSizeSamples / 2;

    readPos0 = wrapFloat (static_cast<float> (inputWriteIndex - grainSizeSamples), static_cast<float> (inputBufferLen));
    readPos1 = wrapFloat (static_cast<float> (inputWriteIndex - grainSizeSamples / 2), static_cast<float> (inputBufferLen));

    currentSemitones = targetSemitones;
    ratio = std::pow (2.0f, currentSemitones / 12.0f);
}

void GranularPitchShifter::restartGrain (float& readPos) noexcept
{
    readPos = wrapFloat (static_cast<float> (inputWriteIndex - grainSizeSamples), static_cast<float> (inputBufferLen));
}

float GranularPitchShifter::readInterpolated (float fractionalPos) const noexcept
{
    const float pos = wrapFloat (fractionalPos, static_cast<float> (inputBufferLen));
    const int i0 = static_cast<int> (pos);
    const int i1 = wrapIndex (i0 + 1, inputBufferLen);
    const float frac = pos - static_cast<float> (i0);
    return inputBuffer[static_cast<size_t> (i0)] * (1.0f - frac) + inputBuffer[static_cast<size_t> (i1)] * frac;
}

void GranularPitchShifter::pushDrySample (float sample) noexcept
{
    inputBuffer[static_cast<size_t> (inputWriteIndex)] = sample;
    inputWriteIndex = wrapIndex (inputWriteIndex + 1, inputBufferLen);
    if (validSamplesWritten < inputBufferLen)
        ++validSamplesWritten;
}

float GranularPitchShifter::renderOutputSample() noexcept
{
    // Smooth semitone glides (~a few ms) so switching harmony mode doesn't zipper.
    currentSemitones += 0.01f * (targetSemitones - currentSemitones);
    ratio = std::pow (2.0f, currentSemitones / 12.0f);

    const float phase0 = static_cast<float> (age0) / static_cast<float> (grainSizeSamples);
    const float phase1 = static_cast<float> (age1) / static_cast<float> (grainSizeSamples);

    const float s0 = readInterpolated (readPos0) * hannWindow (phase0);
    const float s1 = readInterpolated (readPos1) * hannWindow (phase1);

    float out = s0 + s1;

    // Until the buffer has filled with one full grain's worth of real
    // history, fade the output in to avoid a cold-start click/thump.
    if (validSamplesWritten < grainSizeSamples)
        out *= static_cast<float> (validSamplesWritten) / static_cast<float> (grainSizeSamples);

    // Advance grain 0
    readPos0 = wrapFloat (readPos0 + ratio, static_cast<float> (inputBufferLen));
    if (++age0 >= grainSizeSamples)
    {
        age0 = 0;
        restartGrain (readPos0);
    }

    // Advance grain 1
    readPos1 = wrapFloat (readPos1 + ratio, static_cast<float> (inputBufferLen));
    if (++age1 >= grainSizeSamples)
    {
        age1 = 0;
        restartGrain (readPos1);
    }

    return out;
}
