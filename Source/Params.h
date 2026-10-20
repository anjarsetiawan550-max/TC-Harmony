#pragma once
#include <JuceHeader.h>

namespace ParamIDs
{
    inline constexpr const char* harmonyMode = "harmonyMode";
    inline constexpr const char* level = "level";
    inline constexpr const char* reverbType = "reverbType";
    inline constexpr const char* mixInL = "mixInL";
    inline constexpr const char* toneEnabled = "toneEnabled";
    inline constexpr const char* toneVoicing = "toneVoicing";
    inline constexpr const char* fxEnabled = "fxEnabled";
    inline constexpr const char* refOut = "refOut";
    inline constexpr const char* keyRoot = "keyRoot";
    inline constexpr const char* keyScale = "keyScale";
}

inline juce::StringArray harmonyModeNames()
{
    return { "High", "Higher", "High & Higher", "Low", "Lower",
             "Low & Lower", "Higher & Lower", "High & Low" };
}

inline juce::StringArray reverbTypeNames()
{
    return { "Off", "Room", "Club", "Hall" };
}

inline juce::StringArray keyRootNames()
{
    return { "Auto", "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
}

inline juce::StringArray keyScaleNames()
{
    return { "Major", "Minor" };
}

inline juce::StringArray toneVoicingNames()
{
    return { "Normal", "Less Bright" };
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::harmonyMode, 1 }, "Harmony Mode", harmonyModeNames(), 0));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::level, 1 }, "Level",
        juce::NormalisableRange<float> (0.0f, 1.0f), 0.6f));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::reverbType, 1 }, "Reverb", reverbTypeNames(), 1));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::mixInL, 1 }, "Mix In 1/L",
        juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIDs::toneEnabled, 1 }, "Adaptive Tone", true));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::toneVoicing, 1 }, "Tone Voicing", toneVoicingNames(), 0));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIDs::fxEnabled, 1 }, "Harmony FX (Footswitch)", true));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIDs::refOut, 1 }, "Ref Out", true));

    // Key manual: "Auto" = ikuti akor/key dari input 2 (seperti TC-Helicon)
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::keyRoot, 1 }, "Key", keyRootNames(), 0));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::keyScale, 1 }, "Scale", keyScaleNames(), 0));

    return { params.begin(), params.end() };
}
