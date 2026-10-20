#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "gui/FilmstripLookAndFeel.h"
#include "gui/ImageStateButton.h"
#include "gui/PedalLayout.h"

class HarmonySingerAudioProcessorEditor : public juce::AudioProcessorEditor,
                                           private juce::Timer
{
public:
    explicit HarmonySingerAudioProcessorEditor (HarmonySingerAudioProcessor&);
    ~HarmonySingerAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshToggleVisuals();

    HarmonySingerAudioProcessor& processor;

    juce::Image backgroundImage;

    // --- rotary knobs ---
    FilmstripLookAndFeel harmonyLnf, levelLnf, reverbLnf, mixInLLnf;
    juce::Slider harmonySlider, levelSlider, reverbSlider, mixInLSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> harmonyAttachment, levelAttachment, reverbAttachment, mixInLAttachment;

    // --- buttons / switches / LEDs (state-driven from photographed assets) ---
    ImageStateButton toneButton;    // states: 0 off, 1 on, 2 off-pressed, 3 on-pressed
    ImageStateButton refOutButton;  // states: 0 up(off), 1 down(on)
    ImageStateButton footswitch;    // states: 0 up, 1 down
    ImageStateButton ledStatusFx;   // states: 0 off, 1 on
    ImageStateButton ledPower;      // states: 0 off, 1 on (we always show on)
    ImageStateButton ledRefOut;     // states: 0 off, 1 on (mengikuti tombol REF OUT)

    // pesan singkat di panel (mis. "TONE: LESS BRIGHT" setelah Ctrl + klik footswitch)
    void showOverlay (const juce::String& text);
    void showKeyMenu();
    static juce::String describeContext (int displayCode);
    int lastDisplayCode = -1;
    juce::String overlayText;
    double overlayUntilMs = 0.0;

    bool toneButtonPressed = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HarmonySingerAudioProcessorEditor)
};
