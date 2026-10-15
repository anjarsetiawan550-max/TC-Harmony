#pragma once
#include <JuceHeader.h>

/**
    AdaptiveTone
    -------------
    The chain behind the TONE button's red LED. Applies ONLY to the dry
    lead vocal (channel 1 / mic), never to the generated harmony voices,
    per the spec. Chain order (as in most vocal-channel processors):

        Noise Gate -> Adaptive EQ -> Compressor -> De-esser

    Two voicings, switchable with Ctrl + footswitch per the spec:
        Normal       - fuller low-mid, a bit more top-end air
        Less Bright  - gentler high shelf, softer de-esser threshold,
                       for harsh mics / bright rooms / sibilant voices
*/
class AdaptiveTone
{
public:
    enum class Voicing { Normal, LessBright };

    void prepare (double sampleRate, int maxBlockSize, int numChannels = 1);
    void reset();

    void setEnabled (bool shouldBeEnabled) noexcept { enabled = shouldBeEnabled; }
    void setVoicing (Voicing v);

    // in-place mono processing
    void process (float* samples, int numSamples);

private:
    void updateFilters();

    bool enabled = true;
    Voicing voicing = Voicing::Normal;
    double sampleRate = 44100.0;

    // --- noise gate (simple RMS envelope follower + gain) ---
    float gateEnvelope = 0.0f;
    float gateThresholdLinear = 0.012f; // ~ -38 dBFS
    float gateAttackCoeff = 0.0f, gateReleaseCoeff = 0.0f;
    float gateGain = 0.0f;

    // --- adaptive EQ (3-band: low cut, presence bump, air shelf) ---
    juce::dsp::IIR::Filter<float> hpf;      // rumble / mic handling noise
    juce::dsp::IIR::Filter<float> lowCutMud; // gentle low-mid scoop to reduce "keruh"
    juce::dsp::IIR::Filter<float> presence;  // clarity bump ~3 kHz
    juce::dsp::IIR::Filter<float> airShelf;  // high shelf

    // --- compressor ---
    juce::dsp::Compressor<float> compressor;

    // --- de-esser: split-band dynamic attenuation of the sibilant (~5-9kHz) range ---
    juce::dsp::IIR::Filter<float> deEssLowBand;   // everything below the split
    juce::dsp::IIR::Filter<float> deEssHighBand;  // the sibilant band being ducked
    juce::dsp::IIR::Filter<float> deEssDetector;  // narrower bandpass driving the envelope
    float deEssEnvelope = 0.0f;
    float deEssAttackCoeff = 0.0f, deEssReleaseCoeff = 0.0f;
    float deEssThreshold = 0.05f;
    float deEssFloor = 0.3f; // max attenuation applied to the high band (~ -10 dB)
};
