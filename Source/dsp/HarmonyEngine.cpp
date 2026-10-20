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

void HarmonyEngine::prepare (double newSampleRate, int maxBlockSize)
{
    sampleRate = newSampleRate;

    pitchDetector.prepare (sampleRate, 1024);
    referencePitch.prepare (sampleRate, 1024);

    // jendela YIN 1024 -> periode terpanjang < 512 sampel
    voiceA.prepare (sampleRate, 512);
    voiceB.prepare (sampleRate, 512);
    voiceA.setExtraDelayMs (9.0f);   // sedikit "telat" dari suara asli, seperti penyanyi kedua
    voiceB.setExtraDelayMs (15.0f);

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
    noteValid = false;
    driftA = driftB = driftTargetA = driftTargetB = 0.0f;
    driftCounter = 0;
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
    lastSemiA = static_cast<float> (m.fallbackA);
    lastSemiB = static_cast<float> (m.fallbackB);
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

void HarmonyEngine::updateKeyEstimate (float referenceFreqHz, bool referenceVoiced,
                                       float singerFreqHz, bool singerVoiced, int numSamples)
{
    // Lupa pelan (konstanta waktu ~tiap 512 sampel) supaya kunci bisa berpindah dalam puluhan detik.
    const float decay = std::pow (0.9995f, static_cast<float> (numSamples) / 512.0f);
    for (float& v : pitchClassHistogram) v *= decay;

    auto addEvidence = [this] (float freqHz, float weight)
    {
        const float midi = 69.0f + 12.0f * std::log2 (freqHz / 440.0f);
        const int pitchClass = ((static_cast<int> (std::lround (midi))) % 12 + 12) % 12;
        pitchClassHistogram[static_cast<size_t> (pitchClass)] += weight;
    };

    // Gitar/referensi = bukti utama; melodi penyanyi sendiri = bukti tambahan
    // (supaya tetap ada kunci walau input 2 kosong).
    if (referenceVoiced && referenceFreqHz > 0.0f) addEvidence (referenceFreqHz, 1.0f);
    if (singerVoiced && singerFreqHz > 0.0f)       addEvidence (singerFreqHz, 0.35f);

    // Hitung ulang kunci terbaik kira-kira dua kali per detik.
    // (Perbaikan: sebelumnya penghitung ini naik 1 per BLOK bukan per sampel, sehingga
    //  kunci baru terdeteksi setelah beberapa menit -> harmoni selalu jatuh ke interval tetap.)
    samplesSinceKeyUpdate += numSamples;
    if (samplesSinceKeyUpdate >= static_cast<int> (sampleRate * 0.5))
    {
        samplesSinceKeyUpdate = 0;

        float totalEnergy = 0.0f;
        for (float v : pitchClassHistogram) totalEnergy += v;

        if (totalEnergy > 8.0f) // cukup bukti untuk dipercaya
        {
            int bestRoot = keyRootPitchClass;
            float bestScore = -1.0f;
            for (int root = 0; root < 12; ++root)
            {
                float score = 0.0f;
                for (int deg : kMajorScale)
                    score += pitchClassHistogram[static_cast<size_t> ((root + deg) % 12)];

                // sedikit bias untuk mempertahankan kunci sebelumnya (hysteresis)
                if (root == keyRootPitchClass) score *= 1.08f;

                if (score > bestScore) { bestScore = score; bestRoot = root; }
            }
            keyRootPitchClass = bestRoot;
            keyEstablished = true;
        }
    }
}

float HarmonyEngine::diatonicSemitoneOffset (int singerMidiNote, int scaleStepsOffset) const
{
    const int pitchClass = ((singerMidiNote % 12) + 12) % 12;
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
    // --- update pitch trackers & key estimate ---
    pitchDetector.pushSamples (dryVocal, numSamples);
    referencePitch.pushSamples (referenceInput, numSamples);

    const float singerHz = pitchDetector.getFrequencyHz();
    const bool singerVoiced = pitchDetector.isVoiced() && singerHz > 0.0f;

    updateKeyEstimate (referencePitch.getFrequencyHz(), referencePitch.isVoiced(),
                       singerHz, singerVoiced, numSamples);

    // --- nada penyanyi (dengan hysteresis) -> offset semitone tiap suara ---
    if (singerVoiced)
    {
        const float midi = 69.0f + 12.0f * std::log2 (singerHz / 440.0f);
        if (! noteValid || std::abs (midi - static_cast<float> (trackedNote)) > 0.65f)
        {
            trackedNote = static_cast<int> (std::lround (midi));
            noteValid = true;
        }

        if (keyEstablished)
        {
            if (voiceAActive) lastSemiA = diatonicSemitoneOffset (trackedNote, scaleStepsA);
            if (voiceBActive) lastSemiB = diatonicSemitoneOffset (trackedNote, scaleStepsB);
        }
        else
        {
            lastSemiA = static_cast<float> (fallbackSemitonesA);
            lastSemiB = static_cast<float> (fallbackSemitonesB);
        }
    }
    // saat tak bernada offset terakhir dipertahankan (bagian itu memang tidak digeser)

    voiceA.setPitchShiftSemitones (voiceAActive ? lastSemiA : 0.0f);
    voiceB.setPitchShiftSemitones (voiceBActive ? lastSemiB : 0.0f);

    // --- detune halus yang melayang pelan: dua "penyanyi" tidak pernah persis sama ---
    driftCounter += numSamples;
    if (driftCounter >= static_cast<int> (sampleRate * 0.15))
    {
        driftCounter = 0;
        driftTargetA = (rng.nextFloat() * 2.0f - 1.0f) * 4.0f;   // +-4 sen
        driftTargetB = (rng.nextFloat() * 2.0f - 1.0f) * 4.0f;
    }
    const float driftCoeff = 1.0f - std::exp (-static_cast<float> (numSamples) / (0.25f * static_cast<float> (sampleRate)));
    driftA += driftCoeff * (driftTargetA - driftA);
    driftB += driftCoeff * (driftTargetB - driftB);
    voiceA.setDetuneCents (+6.0f + driftA);
    voiceB.setDetuneCents (-6.0f + driftB);

    // --- render ---
    // Kedua suara selalu didorong & dirender (supaya penghitung waktunya tetap sinkron);
    // yang tidak aktif cukup tidak diikutkan ke bus.
    const float period = singerVoiced ? static_cast<float> (sampleRate / static_cast<double> (singerHz)) : 0.0f;
    const float ampA = voiceAActive ? 1.0f : 0.0f;
    const float ampB = voiceBActive ? 1.0f : 0.0f;
    const float voiceNorm = (voiceAActive && voiceBActive) ? 0.72f : 1.0f; // dua suara -> turunkan sedikit

    for (int i = 0; i < numSamples; ++i)
    {
        voiceA.pushDrySample (dryVocal[i], period);
        voiceB.pushDrySample (dryVocal[i], period);

        const float a = voiceA.renderOutputSample() * ampA;
        const float b = voiceB.renderOutputSample() * ampB;

        const float g = levelGain.getNextValue();
        harmonyOut[i] = (a + b) * g * voiceNorm;
    }

    if (currentReverbType != 0)
        reverb.processMono (harmonyOut, numSamples);
}
