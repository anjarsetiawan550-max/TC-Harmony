#include "HarmonyEngine.h"
#include <cmath>
#include <algorithm>
#include <iterator>

namespace
{
    // Major scale degrees as semitones from the root: 1 2 3 4 5 6 7
    constexpr int kMajorScale[7] = { 0, 2, 4, 5, 7, 9, 11 };

    // Mode table: { scaleStepsA, activeA, scaleStepsB, activeB, fallbackSemiA, fallbackSemiB }
    // Scale steps are diatonic degree offsets (2=3rd, 3=4th, 4=5th, 5=6th; sign = direction).
    struct ModeSpec { int stepsA; bool activeA; int stepsB; bool activeB; int fallbackA; int fallbackB; };
    constexpr ModeSpec kModes[8] =
    {
        /*0 High           */ {  2, true,  0, false,  4, 0 },
        /*1 Higher         */ {  4, true,  0, false,  7, 0 },
        /*2 High & Higher  */ {  2, true,  4, true,   4, 7 },
        /*3 Low            */ { -3, true,  0, false, -5, 0 },
        /*4 Lower          */ { -5, true,  0, false, -9, 0 },
        /*5 Low & Lower    */ { -3, true, -5, true,  -5,-9 },
        /*6 Higher & Lower */ {  4, true, -5, true,   7,-9 },
        /*7 High & Low     */ {  2, true, -3, true,   4,-5 },
    };

    inline int floorDiv (int a, int b) noexcept
    {
        int q = a / b;
        int r = a % b;
        if ((r != 0) && ((r < 0) != (b < 0))) --q;
        return q;
    }
}

void HarmonyEngine::prepare (double sampleRate, int maxBlockSize)
{
    pitchDetector.prepare (sampleRate, 1024);
    referencePitch.prepare (sampleRate, 1024);

    voiceA.prepare (sampleRate, 1024);
    voiceB.prepare (sampleRate, 1024);

    levelGain.reset (sampleRate, 0.02);
    levelGain.setCurrentAndTargetValue (0.6f);

    reverb.setSampleRate (sampleRate);
    updateReverbParamsForType (currentReverbType);

    scratchA.assign (static_cast<size_t> (maxBlockSize), 0.0f);
    scratchB.assign (static_cast<size_t> (maxBlockSize), 0.0f);

    setMode (0);
    reset();
}

void HarmonyEngine::reset()
{
    voiceA.reset();
    voiceB.reset();
    reverb.reset();
    std::fill (std::begin (pitchClassHistogram), std::end (pitchClassHistogram), 0.0f);
    keyEstablished = false;
    keyRootPitchClass = 0;
    samplesSinceKeyUpdate = 0;
}

void HarmonyEngine::setMode (int modeIndex) noexcept
{
    modeIndex = juce::jlimit (0, 7, modeIndex);
    const auto& m = kModes[static_cast<size_t> (modeIndex)];
    voiceAActive = m.activeA;
    voiceBActive = m.activeB;
    scaleStepsA = m.stepsA;
    scaleStepsB = m.stepsB;
    fallbackSemitonesA = m.fallbackA;
    fallbackSemitonesB = m.fallbackB;
}

void HarmonyEngine::setLevel01 (float level01) noexcept
{
    // 0..1 -> 0..~1.3x linear gain on the harmony bus (headroom for 2-voice modes
    // is handled by the mix stage in the processor, not clamped here).
    levelGain.setTargetValue (juce::jlimit (0.0f, 1.3f, level01 * 1.3f));
}

void HarmonyEngine::setReverbType (int type) noexcept
{
    type = juce::jlimit (0, 3, type);
    if (type != currentReverbType)
    {
        currentReverbType = type;
        updateReverbParamsForType (type);
    }
}

void HarmonyEngine::updateReverbParamsForType (int type)
{
    switch (type)
    {
        case 0: // OFF -> fully dry, reverb engine still runs but contributes nothing
            reverbParams.roomSize = 0.2f; reverbParams.damping = 0.9f;
            reverbParams.wetLevel = 0.0f; reverbParams.dryLevel = 1.0f;
            reverbParams.width = 1.0f; reverbParams.freezeMode = 0.0f;
            break;
        case 1: // ROOM - short, intimate
            reverbParams.roomSize = 0.28f; reverbParams.damping = 0.55f;
            reverbParams.wetLevel = 0.22f; reverbParams.dryLevel = 0.80f;
            reverbParams.width = 0.6f; reverbParams.freezeMode = 0.0f;
            break;
        case 2: // CLUB - mid venue
            reverbParams.roomSize = 0.55f; reverbParams.damping = 0.45f;
            reverbParams.wetLevel = 0.30f; reverbParams.dryLevel = 0.72f;
            reverbParams.width = 0.85f; reverbParams.freezeMode = 0.0f;
            break;
        case 3: // HALL - big, dramatic
            reverbParams.roomSize = 0.88f; reverbParams.damping = 0.25f;
            reverbParams.wetLevel = 0.40f; reverbParams.dryLevel = 0.65f;
            reverbParams.width = 1.0f; reverbParams.freezeMode = 0.0f;
            break;
        default: break;
    }
    reverb.setParameters (reverbParams);
}

void HarmonyEngine::updateKeyEstimate (float referenceFreqHz, bool referenceVoiced)
{
    if (referenceVoiced && referenceFreqHz > 0.0f)
    {
        const float midi = 69.0f + 12.0f * std::log2 (referenceFreqHz / 440.0f);
        int pitchClass = ((static_cast<int> (std::lround (midi))) % 12 + 12) % 12;

        // slow exponential forgetting so the key estimate adapts over tens of seconds
        for (float& v : pitchClassHistogram) v *= 0.9995f;
        pitchClassHistogram[static_cast<size_t> (pitchClass)] += 1.0f;
    }

    // Recompute best-fit major key roughly twice a second.
    if (++samplesSinceKeyUpdate >= 22050)
    {
        samplesSinceKeyUpdate = 0;

        float totalEnergy = 0.0f;
        for (float v : pitchClassHistogram) totalEnergy += v;

        if (totalEnergy > 8.0f) // enough evidence accumulated to trust an estimate
        {
            int bestRoot = keyRootPitchClass;
            float bestScore = -1.0f;
            for (int root = 0; root < 12; ++root)
            {
                float score = 0.0f;
                for (int deg : kMajorScale)
                    score += pitchClassHistogram[static_cast<size_t> ((root + deg) % 12)];

                // small bias to keep the previously-established key (hysteresis)
                if (root == keyRootPitchClass) score *= 1.08f;

                if (score > bestScore) { bestScore = score; bestRoot = root; }
            }
            keyRootPitchClass = bestRoot;
            keyEstablished = true;
        }
    }
}

float HarmonyEngine::diatonicSemitoneOffset (float singerFreqHz, int scaleStepsOffset) const
{
    const float midi = 69.0f + 12.0f * std::log2 (singerFreqHz / 440.0f);
    const int pitchClass = ((static_cast<int> (std::lround (midi))) % 12 + 12) % 12;
    const int relative = ((pitchClass - keyRootPitchClass) % 12 + 12) % 12;

    // find nearest major-scale degree to the singer's current pitch class
    int nearestDeg = 0;
    int bestDist = 999;
    for (int i = 0; i < 7; ++i)
    {
        int dist = std::abs (kMajorScale[i] - relative);
        dist = std::min (dist, 12 - dist);
        if (dist < bestDist) { bestDist = dist; nearestDeg = i; }
    }

    const int targetDeg = nearestDeg + scaleStepsOffset;
    const int octaveAdd = floorDiv (targetDeg, 7);
    const int targetDegMod = ((targetDeg % 7) + 7) % 7;
    const int targetRelative = kMajorScale[targetDegMod] + octaveAdd * 12;

    return static_cast<float> (targetRelative - relative);
}

void HarmonyEngine::process (const float* dryVocal, const float* referenceInput, float* harmonyOut, int numSamples)
{
    // --- update pitch trackers & key estimate (block-rate internally hop-driven) ---
    pitchDetector.pushSamples (dryVocal, numSamples);
    referencePitch.pushSamples (referenceInput, numSamples);
    updateKeyEstimate (referencePitch.getFrequencyHz(), referencePitch.isVoiced());

    // --- decide this block's semitone offsets for each active voice ---
    float semiA = static_cast<float> (fallbackSemitonesA);
    float semiB = static_cast<float> (fallbackSemitonesB);

    if (keyEstablished && pitchDetector.isVoiced() && pitchDetector.getFrequencyHz() > 0.0f)
    {
        if (voiceAActive) semiA = diatonicSemitoneOffset (pitchDetector.getFrequencyHz(), scaleStepsA);
        if (voiceBActive) semiB = diatonicSemitoneOffset (pitchDetector.getFrequencyHz(), scaleStepsB);
    }

    voiceA.setPitchShiftSemitones (voiceAActive ? semiA : 0.0f);
    voiceB.setPitchShiftSemitones (voiceBActive ? semiB : 0.0f);

    for (int i = 0; i < numSamples; ++i)
    {
        voiceA.pushDrySample (dryVocal[i]);
        voiceB.pushDrySample (dryVocal[i]);

        const float a = voiceAActive ? voiceA.renderOutputSample() : 0.0f;
        const float b = voiceBActive ? voiceB.renderOutputSample() : 0.0f;

        const float g = levelGain.getNextValue();
        harmonyOut[i] = (a + b) * g;
    }

    if (currentReverbType != 0)
        reverb.processMono (harmonyOut, numSamples);
}
