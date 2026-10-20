#include "PluginProcessor.h"
#include "PluginEditor.h"

HarmonySingerAudioProcessor::HarmonySingerAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
                                 .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                 .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    harmonyModeParam = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (ParamIDs::harmonyMode));
    levelParam       = dynamic_cast<juce::AudioParameterFloat*>  (apvts.getParameter (ParamIDs::level));
    reverbTypeParam  = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (ParamIDs::reverbType));
    mixInLParam      = dynamic_cast<juce::AudioParameterFloat*>  (apvts.getParameter (ParamIDs::mixInL));
    toneEnabledParam = dynamic_cast<juce::AudioParameterBool*>   (apvts.getParameter (ParamIDs::toneEnabled));
    toneVoicingParam = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (ParamIDs::toneVoicing));
    fxEnabledParam   = dynamic_cast<juce::AudioParameterBool*>   (apvts.getParameter (ParamIDs::fxEnabled));
    refOutParam      = dynamic_cast<juce::AudioParameterBool*>   (apvts.getParameter (ParamIDs::refOut));
    keyRootParam     = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (ParamIDs::keyRoot));
    keyScaleParam    = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (ParamIDs::keyScale));
}

HarmonySingerAudioProcessor::~HarmonySingerAudioProcessor() = default;

void HarmonySingerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    harmonyEngine.prepare (sampleRate, samplesPerBlock);
    adaptiveTone.prepare (sampleRate, samplesPerBlock, 1);

    juce::dsp::ProcessSpec spec { sampleRate, static_cast<juce::uint32> (samplesPerBlock), 1 };
    dryDelayForAlignment.prepare (spec);
    refDelayForAlignment.prepare (spec);

    reportedLatencySamples = harmonyEngine.getLatencySamples();
    dryDelayForAlignment.setDelay (static_cast<float> (reportedLatencySamples));
    refDelayForAlignment.setDelay (static_cast<float> (reportedLatencySamples));
    setLatencySamples (reportedLatencySamples);

    refOutGain.reset (sampleRate, 0.02);
    fxMixGain.reset (sampleRate, 0.03);
    refOutGain.setCurrentAndTargetValue (refOutParam != nullptr && refOutParam->get() ? 1.0f : 0.0f);
    fxMixGain.setCurrentAndTargetValue (fxEnabledParam != nullptr && fxEnabledParam->get() ? 1.0f : 0.0f);

    dryVocalScratch.assign (static_cast<size_t> (samplesPerBlock), 0.0f);
    referenceScratch.assign (static_cast<size_t> (samplesPerBlock), 0.0f);
    harmonyScratch.assign (static_cast<size_t> (samplesPerBlock), 0.0f);
}

void HarmonySingerAudioProcessor::releaseResources()
{
}

bool HarmonySingerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void HarmonySingerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || numSamples == 0)
        return;

    // --- read inputs before anything overwrites the buffer ---
    const float* micIn = buffer.getReadPointer (0);
    const float* refIn = buffer.getReadPointer (1);
    for (int i = 0; i < numSamples; ++i)
    {
        dryVocalScratch[static_cast<size_t> (i)]  = micIn[i];
        referenceScratch[static_cast<size_t> (i)] = refIn[i];
    }

    // --- Adaptive Tone (vocal only) ---
    const bool toneOn = toneEnabledParam != nullptr && toneEnabledParam->get();
    adaptiveTone.setEnabled (toneOn);
    adaptiveTone.setVoicing (toneVoicingParam != nullptr && toneVoicingParam->getIndex() == 1
                                  ? AdaptiveTone::Voicing::LessBright
                                  : AdaptiveTone::Voicing::Normal);
    adaptiveTone.process (dryVocalScratch.data(), numSamples);

    // --- Harmony engine (reads dry vocal + reference, writes wet harmony bus) ---
    harmonyEngine.setMode (harmonyModeParam != nullptr ? harmonyModeParam->getIndex() : 0);
    harmonyEngine.setManualKey (keyRootParam != nullptr ? keyRootParam->getIndex() - 1 : -1,   // 0 = Auto -> -1
                                keyScaleParam != nullptr && keyScaleParam->getIndex() == 1);
    harmonyEngine.setLevel01 (levelParam != nullptr ? levelParam->get() : 0.6f);
    harmonyEngine.setReverbType (reverbTypeParam != nullptr ? reverbTypeParam->getIndex() : 1);
    harmonyEngine.process (dryVocalScratch.data(), referenceScratch.data(), harmonyScratch.data(), numSamples);

    detectedFrequencyHz.store (harmonyEngine.getDetectedFrequencyHz(), std::memory_order_relaxed);
    detectedVoiced.store (harmonyEngine.isVoiced(), std::memory_order_relaxed);

    const bool refOutOn = refOutParam != nullptr && refOutParam->get();
    refOutGain.setTargetValue (refOutOn ? 1.0f : 0.0f);

    const bool fxOn = fxEnabledParam != nullptr && fxEnabledParam->get();
    fxMixGain.setTargetValue (fxOn ? 1.0f : 0.0f);

    const float mix = mixInLParam != nullptr ? mixInLParam->get() : 1.0f;

    float* outL = buffer.getWritePointer (0);
    float* outR = buffer.getWritePointer (1);

    for (int i = 0; i < numSamples; ++i)
    {
        // delay the dry/reference paths by the harmony engine's latency so
        // everything lands sample-aligned at the output (PDC-safe mixing)
        const float delayedDry = dryDelayForAlignment.popSample (0, dryDelayForAlignment.getDelay(), true);
        dryDelayForAlignment.pushSample (0, dryVocalScratch[static_cast<size_t> (i)]);

        const float delayedRef = refDelayForAlignment.popSample (0, refDelayForAlignment.getDelay(), true);
        refDelayForAlignment.pushSample (0, referenceScratch[static_cast<size_t> (i)]);

        // MIX IN 1/L = crossfade murni: 0% = hanya suara asli mic (DO), 100% = hanya
        // suara harmoni (MI / SOL ...) tanpa suara asli. Footswitch OFF -> m = 0 -> bypass
        // (suara asli saja), jadi output tidak pernah senyap.
        const float fxGain = fxMixGain.getNextValue();
        const float m = mix * fxGain;
        outL[i] = delayedDry * (1.0f - m) + harmonyScratch[static_cast<size_t> (i)] * m;

        outR[i] = delayedRef * refOutGain.getNextValue();
    }

    // any extra channels beyond the stereo pair (shouldn't happen given the
    // fixed stereo bus layout, but keep things well-defined if hosted oddly)
    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);
}

juce::AudioProcessorEditor* HarmonySingerAudioProcessor::createEditor()
{
    return new HarmonySingerAudioProcessorEditor (*this);
}

void HarmonySingerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); true)
    {
        std::unique_ptr<juce::XmlElement> xml (state.createXml());
        copyXmlToBinary (*xml, destData);
    }
}

void HarmonySingerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

void HarmonySingerAudioProcessor::toggleFxEnabled()
{
    if (fxEnabledParam != nullptr)
        fxEnabledParam->setValueNotifyingHost (fxEnabledParam->get() ? 0.0f : 1.0f);
}

void HarmonySingerAudioProcessor::toggleToneEnabled()
{
    if (toneEnabledParam != nullptr)
        toneEnabledParam->setValueNotifyingHost (toneEnabledParam->get() ? 0.0f : 1.0f);
}

void HarmonySingerAudioProcessor::toggleToneVoicing()
{
    if (toneVoicingParam != nullptr)
    {
        const int next = (toneVoicingParam->getIndex() + 1) % 2;
        toneVoicingParam->setValueNotifyingHost (toneVoicingParam->convertTo0to1 (static_cast<float> (next)));
    }
}

// This creates the global instance expected by all JUCE plugin wrappers.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HarmonySingerAudioProcessor();
}
