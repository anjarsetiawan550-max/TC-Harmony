#include "AdaptiveTone.h"
#include <cmath>

void AdaptiveTone::prepare (double newSampleRate, int maxBlockSize, int numChannels)
{
    sampleRate = newSampleRate;

    juce::dsp::ProcessSpec spec { sampleRate, static_cast<juce::uint32> (maxBlockSize), static_cast<juce::uint32> (numChannels) };

    hpf.prepare (spec);
    lowCutMud.prepare (spec);
    presence.prepare (spec);
    airShelf.prepare (spec);
    deEssLowBand.prepare (spec);
    deEssHighBand.prepare (spec);
    deEssDetector.prepare (spec);

    compressor.prepare (spec);
    compressor.setRatio (3.0f);
    compressor.setAttack (8.0f);
    compressor.setRelease (120.0f);
    compressor.setThreshold (-18.0f);

    // Gate / de-esser envelope coefficients (one-pole, time-constant based)
    auto coeffFor = [this] (float ms)
    {
        return std::exp (-1.0f / (0.001f * ms * static_cast<float> (sampleRate)));
    };
    gateAttackCoeff  = coeffFor (3.0f);
    gateReleaseCoeff = coeffFor (120.0f);
    deEssAttackCoeff  = coeffFor (2.0f);
    deEssReleaseCoeff = coeffFor (60.0f);

    updateFilters();
    reset();
}

void AdaptiveTone::reset()
{
    hpf.reset(); lowCutMud.reset(); presence.reset(); airShelf.reset();
    deEssLowBand.reset(); deEssHighBand.reset(); deEssDetector.reset();
    compressor.reset();
    gateEnvelope = 0.0f;
    gateGain = 0.0f;
    deEssEnvelope = 0.0f;
}

void AdaptiveTone::setVoicing (Voicing v)
{
    if (v != voicing)
    {
        voicing = v;
        updateFilters();
    }
}

void AdaptiveTone::updateFilters()
{
    const auto sr = sampleRate;

    *hpf.coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, 90.0f, 0.707f);
    *lowCutMud.coefficients = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 300.0f, 1.0f, juce::Decibels::decibelsToGain (-2.5f));

    if (voicing == Voicing::Normal)
    {
        *presence.coefficients = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 3200.0f, 1.1f, juce::Decibels::decibelsToGain (3.0f));
        *airShelf.coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 9000.0f, 0.8f, juce::Decibels::decibelsToGain (3.5f));
        deEssThreshold = 0.06f;
        deEssFloor = 0.35f;
    }
    else // LessBright
    {
        // jelas lebih gelap/lembut dari Normal (selisih ~8 dB di atas 5 kHz) supaya perubahan terdengar
        *presence.coefficients = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 2500.0f, 1.0f, juce::Decibels::decibelsToGain (0.0f));
        *airShelf.coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 5500.0f, 0.7f, juce::Decibels::decibelsToGain (-4.5f));
        deEssThreshold = 0.035f; // more sensitive -> catches sibilance earlier
        deEssFloor = 0.22f;      // allowed to cut harder
    }

    const float splitHz = 6500.0f;
    *deEssLowBand.coefficients  = *juce::dsp::IIR::Coefficients<float>::makeLowPass  (sr, splitHz, 0.707f);
    *deEssHighBand.coefficients = *juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, splitHz, 0.707f);
    *deEssDetector.coefficients = *juce::dsp::IIR::Coefficients<float>::makeBandPass (sr, 7000.0f, 1.2f);
}

void AdaptiveTone::process (float* samples, int numSamples)
{
    if (! enabled)
        return;

    for (int i = 0; i < numSamples; ++i)
    {
        float x = samples[i];

        // --- Noise gate ---
        const float rectified = std::abs (x);
        gateEnvelope = rectified > gateEnvelope
                           ? gateAttackCoeff * gateEnvelope + (1.0f - gateAttackCoeff) * rectified
                           : gateReleaseCoeff * gateEnvelope + (1.0f - gateReleaseCoeff) * rectified;

        const float targetGateGain = gateEnvelope > gateThresholdLinear ? 1.0f : 0.0f;
        // smooth the gate's own on/off to avoid clicks
        gateGain += 0.05f * (targetGateGain - gateGain);
        x *= gateGain;

        // --- Adaptive EQ ---
        x = hpf.processSample (x);
        x = lowCutMud.processSample (x);
        x = presence.processSample (x);
        x = airShelf.processSample (x);

        // --- De-esser (split-band dynamic attenuation) ---
        const float sense = std::abs (deEssDetector.processSample (x));
        deEssEnvelope = sense > deEssEnvelope
                             ? deEssAttackCoeff * deEssEnvelope + (1.0f - deEssAttackCoeff) * sense
                             : deEssReleaseCoeff * deEssEnvelope + (1.0f - deEssReleaseCoeff) * sense;

        float reduction = 1.0f;
        if (deEssEnvelope > deEssThreshold)
            reduction = juce::jmax (deEssFloor, deEssThreshold / deEssEnvelope);

        const float low  = deEssLowBand.processSample (x);
        const float high = deEssHighBand.processSample (x) * reduction;
        x = low + high;

        samples[i] = x;
    }

    // --- Compressor (block-based via dsp::Compressor, needs an AudioBlock) ---
    juce::dsp::AudioBlock<float> block (&samples, 1, static_cast<size_t> (numSamples));
    juce::dsp::ProcessContextReplacing<float> ctx (block);
    compressor.process (ctx);
}
