#pragma once
#include <JuceHeader.h>
#include "Params.h"
#include "dsp/HarmonyEngine.h"
#include "dsp/AdaptiveTone.h"

/**
    TC HELICON by LR - Audio Processor
    ------------------------------------
    Routing (fixed, per spec):
        Input 1 (channel 0)  = mic / lead vocal  -> processed -> Output 1 / L only
        Input 2 (channel 1)  = reference/guitar   -> (gated by Ref Out) -> Output 2 / R only

    Signal flow on the vocal (L) side:
        mic -> Adaptive Tone (optional) -> [analysed for pitch] -> HarmonyEngine (PSOLA)
             -> crossfade asli <-> harmoni (Mix In 1/L; 100% = tanpa suara asli) -> Output L
        (HarmonyEngine itself also listens to the reference input purely to
         estimate the song's key, never adding it to the audible signal.)

    Latency: the plugin reports PDC equal to the pitch-shifter's latency
    (PsolaPitchShifter::getLatencySamples(), 1168 samples ~ 26 ms @ 44.1 kHz) so hosts keep the
    dry-mixed vocal, the harmony voices and the reference/R channel all
    sample-aligned. That window size is deliberately kept small (rather
    than a larger, higher-fidelity PSOLA/phase-vocoder window) to keep
    the "real-time feel" short for live singing, while staying long
    enough for the YIN pitch tracker to lock reliably.
*/
class HarmonySingerAudioProcessor : public juce::AudioProcessor
{
public:
    HarmonySingerAudioProcessor();
    ~HarmonySingerAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.5; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // UI-facing live status (read by the editor each repaint, not realtime-safe
    // critical since it's only ever written by the audio thread as plain floats)
    std::atomic<float> detectedFrequencyHz { 0.0f };
    std::atomic<bool>  detectedVoiced { false };

    // Konteks harmoni yang sedang dipakai (akor terdeteksi / key / key manual), untuk tampilan panel.
    int getHarmonyDisplayCode() const noexcept { return harmonyEngine.getDisplayCode(); }

    // Footswitch (harmony FX) and Tone button both expose simple toggle methods
    // for the editor so clicks don't need to know parameter plumbing details.
    void toggleFxEnabled();
    void toggleToneEnabled();
    void toggleToneVoicing(); // Ctrl + footswitch combo in the UI

private:
    HarmonyEngine harmonyEngine;
    AdaptiveTone adaptiveTone;

    juce::dsp::DelayLine<float> dryDelayForAlignment { 8192 };
    juce::dsp::DelayLine<float> refDelayForAlignment { 8192 };

    juce::AudioParameterChoice* harmonyModeParam = nullptr;
    juce::AudioParameterFloat*  levelParam = nullptr;
    juce::AudioParameterChoice* reverbTypeParam = nullptr;
    juce::AudioParameterFloat*  mixInLParam = nullptr;
    juce::AudioParameterBool*   toneEnabledParam = nullptr;
    juce::AudioParameterChoice* toneVoicingParam = nullptr;
    juce::AudioParameterBool*   fxEnabledParam = nullptr;
    juce::AudioParameterBool*   refOutParam = nullptr;
    juce::AudioParameterChoice* keyRootParam = nullptr;
    juce::AudioParameterChoice* keyScaleParam = nullptr;

    juce::LinearSmoothedValue<float> refOutGain { 1.0f };
    juce::LinearSmoothedValue<float> fxMixGain { 1.0f }; // smooths footswitch on/off clicks

    std::vector<float> dryVocalScratch, referenceScratch, harmonyScratch;

    int reportedLatencySamples = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HarmonySingerAudioProcessor)
};
