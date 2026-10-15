#pragma once
#include <JuceHeader.h>
#include "YinPitchDetector.h"
#include "GranularPitchShifter.h"

/**
    HarmonyEngine
    --------------
    Owns the pitch tracker, up to two pitch-shifted harmony voices, and
    the harmony-bus reverb. Mirrors the HARMONY knob's 8 modes exactly
    as specified:

        0 High            : +maj3 above
        1 Higher          : +P5  above
        2 High & Higher   : +maj3 and +P5 above
        3 Low             : -P4  below
        4 Lower           : -maj6 below
        5 Low & Lower     : -P4 and -maj6 below
        6 Higher & Lower  : +P5 above, -maj6 below
        7 High & Low      : +maj3 above, -P4 below

    "Tepat kuncinya" (in the correct key): rather than always applying a
    fixed chromatic interval, the engine also listens to the reference
    input (guitar/instrument on input 2) and builds a rolling pitch-class
    histogram from it to estimate the song's current major key centre.
    The harmony interval (3rd / 4th / 5th / 6th) is then taken as a
    *diatonic* scale step in that key relative to the singer's own
    detected note, so e.g. a "3rd above" lands on a major 3rd or minor
    3rd automatically depending on where the melody sits in the key -
    not a fixed +4 semitones every time. If no reliable reference/key
    information is available yet, it falls back to fixed equal-tempered
    intervals assuming C major, so the effect is still always usable.
    (There is no KEY knob on the supplied panel, so this estimation is
    automatic rather than user-selectable - a deliberate, documented
    simplification versus TC-Helicon's own guitar-controlled harmony.)
*/
class HarmonyEngine
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    void setMode (int modeIndex) noexcept;           // 0..7, see table above
    void setLevel01 (float level01) noexcept;          // 0..1 -> harmony output gain
    void setReverbType (int type) noexcept;            // 0=OFF 1=ROOM 2=CLUB 3=HALL

    // dryVocal: mono singer signal (post Adaptive Tone). referenceInput: mono
    // guitar/reference signal (input 2), used only for key estimation - it is
    // never itself added to the harmony bus. harmonyOut: mono buffer that
    // receives the (reverb-wet) harmony voices, pre-mix.
    void process (const float* dryVocal, const float* referenceInput, float* harmonyOut, int numSamples);

    int getLatencySamples() const noexcept { return voiceA.getLatencySamples(); }

    float getDetectedFrequencyHz() const noexcept { return pitchDetector.getFrequencyHz(); }
    bool  isVoiced() const noexcept { return pitchDetector.isVoiced(); }

private:
    void updateReverbParamsForType (int type);
    void updateKeyEstimate (float referenceFreqHz, bool referenceVoiced);
    float diatonicSemitoneOffset (float singerFreqHz, int scaleStepsOffset) const;

    YinPitchDetector pitchDetector;    // tracks the singer (dry vocal)
    YinPitchDetector referencePitch;   // tracks the guitar/reference input
    GranularPitchShifter voiceA, voiceB;

    bool voiceAActive = false;
    bool voiceBActive = false;
    int  scaleStepsA = 0;   // diatonic scale-step offsets for the active mode
    int  scaleStepsB = 0;
    int  fallbackSemitonesA = 0; // used until a key has been established
    int  fallbackSemitonesB = 0;

    juce::LinearSmoothedValue<float> levelGain { 1.0f };

    juce::Reverb reverb;
    juce::Reverb::Parameters reverbParams;
    int currentReverbType = 1; // default ROOM

    std::vector<float> scratchA, scratchB;

    // --- rolling key (key-centre) estimation from the reference input ---
    float pitchClassHistogram[12] = { 0.0f };
    int   keyRootPitchClass = 0;     // 0=C .. 11=B, meaningful only if keyEstablished
    bool  keyEstablished = false;
    int   samplesSinceKeyUpdate = 0;
};
