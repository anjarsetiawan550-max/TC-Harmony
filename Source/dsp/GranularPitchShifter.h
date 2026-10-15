#pragma once
#include <vector>

/**
    GranularPitchShifter
    ---------------------
    A compact real-time pitch shifter using two overlapping (50%) grains
    read back at a variable speed - classic granular/overlap-add pitch
    shifting. It is not as pristine as a full PSOLA/phase-vocoder
    implementation, but it is cheap, glitch-resistant on monophonic
    vocal material, and - most importantly for a live harmony pedal -
    keeps latency short and strictly bounded to one grain length.

    Latency = grainSizeSamples (report via getLatencySamples(), roughly
    20-25 ms at a 1024-sample grain / 44.1 kHz, well inside "feels
    real-time" territory for a harmony/doubling effect).

    Usage: call prepare() once, pushDrySample() continuously with the
    *dry* vocal signal, and renderOutputSample() once per sample to pull
    the time-aligned, pitch-shifted output.
*/
class GranularPitchShifter
{
public:
    void prepare (double sampleRate, int grainSizeSamplesIn = 1024);

    // Semitone offset, positive = up, negative = down. Smoothed internally.
    void setPitchShiftSemitones (float semitones) noexcept { targetSemitones = semitones; }

    void pushDrySample (float sample) noexcept;
    float renderOutputSample() noexcept;

    int getLatencySamples() const noexcept { return grainSizeSamples; }

    void reset();

private:
    static float hannWindow (float phase01) noexcept; // 0..1 -> Hann gain
    float readInterpolated (float fractionalPos) const noexcept;
    void restartGrain (float& readPos) noexcept;

    std::vector<float> inputBuffer;
    int inputWriteIndex = 0;
    int inputBufferLen = 0;
    int validSamplesWritten = 0; // guards against reading uninitialised history at start

    double sampleRate = 44100.0;
    int grainSizeSamples = 1024;

    // two grains, 50% out of phase, crossfaded with complementary Hann windows
    int   age0 = 0;
    int   age1 = 0;
    float readPos0 = 0.0f;
    float readPos1 = 0.0f;

    float targetSemitones = 0.0f;
    float currentSemitones = 0.0f;
    float ratio = 1.0f;
};
