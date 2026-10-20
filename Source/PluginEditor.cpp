#include "PluginEditor.h"

namespace
{
    juce::Image loadImg (const void* data, int size)
    {
        return juce::ImageCache::getFromMemory (data, size);
    }

    juce::Rectangle<int> scaled (const PedalLayout::Rect& r, double k)
    {
        const int x0 = juce::roundToInt (r.x * k);
        const int y0 = juce::roundToInt (r.y * k);
        const int x1 = juce::roundToInt ((r.x + r.w) * k);
        const int y1 = juce::roundToInt ((r.y + r.h) * k);
        return { x0, y0, x1 - x0, y1 - y0 };
    }

    void placeRotary (juce::Slider& slider, const PedalLayout::Rect& r, double k)
    {
        slider.setBounds (scaled (r, k));
    }

    void placeButton (juce::Component& c, const PedalLayout::Rect& r, double k)
    {
        c.setBounds (scaled (r, k));
    }
}

HarmonySingerAudioProcessorEditor::HarmonySingerAudioProcessorEditor (HarmonySingerAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), processor (p)
{
    setOpaque (false);

    backgroundImage = loadImg (BinaryData::pedal_background_clean_png, BinaryData::pedal_background_clean_pngSize);

    // --- Harmony mode knob (8 positions) ---
    harmonyLnf.setStrip (loadImg (BinaryData::knob_harmony_8pos_strip_png, BinaryData::knob_harmony_8pos_strip_pngSize), 200, 200);
    harmonySlider.setLookAndFeel (&harmonyLnf);
    harmonySlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    harmonySlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    harmonySlider.setRange (0.0, 7.0, 1.0);
    addAndMakeVisible (harmonySlider);
    harmonyAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, ParamIDs::harmonyMode, harmonySlider);

    // --- Level knob (64-frame continuous) ---
    levelLnf.setStrip (loadImg (BinaryData::knob_level_64f_strip_png, BinaryData::knob_level_64f_strip_pngSize), 200, 200);
    levelSlider.setLookAndFeel (&levelLnf);
    levelSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    levelSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    levelSlider.setRange (0.0, 1.0);
    addAndMakeVisible (levelSlider);
    levelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, ParamIDs::level, levelSlider);

    // --- Reverb knob (4 positions: OFF/ROOM/CLUB/HALL) ---
    reverbLnf.setStrip (loadImg (BinaryData::knob_reverb_4pos_strip_png, BinaryData::knob_reverb_4pos_strip_pngSize), 200, 200);
    reverbSlider.setLookAndFeel (&reverbLnf);
    reverbSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    reverbSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    reverbSlider.setRange (0.0, 3.0, 1.0);
    addAndMakeVisible (reverbSlider);
    reverbAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, ParamIDs::reverbType, reverbSlider);

    // --- Mix In 1/L knob (64-frame continuous, smaller) ---
    mixInLLnf.setStrip (loadImg (BinaryData::knob_mix_in_1L_64f_strip_png, BinaryData::knob_mix_in_1L_64f_strip_pngSize), 124, 124);
    mixInLSlider.setLookAndFeel (&mixInLLnf);
    mixInLSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    mixInLSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    mixInLSlider.setRange (0.0, 1.0);
    addAndMakeVisible (mixInLSlider);
    mixInLAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, ParamIDs::mixInL, mixInLSlider);

    // --- TONE button (click = toggle Adaptive Tone; Ctrl+click on the
    //     footswitch, below, swaps Normal / Less Bright voicing) ---
    toneButton.addState (loadImg (BinaryData::tone_button_led_off_png,          BinaryData::tone_button_led_off_pngSize));          // 0
    toneButton.addState (loadImg (BinaryData::tone_button_led_on_png,           BinaryData::tone_button_led_on_pngSize));           // 1
    toneButton.addState (loadImg (BinaryData::tone_button_led_off_pressed_png,  BinaryData::tone_button_led_off_pressed_pngSize));  // 2
    toneButton.addState (loadImg (BinaryData::tone_button_led_on_pressed_png,   BinaryData::tone_button_led_on_pressed_pngSize));   // 3
    toneButton.onPressedStateChanged = [this] (bool down) { toneButtonPressed = down; refreshToggleVisuals(); };
    toneButton.onClick = [this] (const juce::ModifierKeys&) { processor.toggleToneEnabled(); refreshToggleVisuals(); };
    addAndMakeVisible (toneButton);

    // --- REF OUT switch ---
    refOutButton.addState (loadImg (BinaryData::refout_switch_up_png,   BinaryData::refout_switch_up_pngSize));   // 0 = off
    refOutButton.addState (loadImg (BinaryData::refout_switch_down_png, BinaryData::refout_switch_down_pngSize)); // 1 = on
    refOutButton.onClick = [this] (const juce::ModifierKeys&)
    {
        if (auto* param = processor.apvts.getParameter (ParamIDs::refOut))
            param->setValueNotifyingHost (param->getValue() > 0.5f ? 0.0f : 1.0f);
        refreshToggleVisuals();
    };
    addAndMakeVisible (refOutButton);

    // --- Footswitch: klik = harmony FX on/off.
    //     Ctrl + klik (Mac: Cmd + klik) atau klik kanan = ganti voicing Tone Normal <-> Less Bright ---
    footswitch.addState (loadImg (BinaryData::footswitch_up_png,   BinaryData::footswitch_up_pngSize));   // 0 = up
    footswitch.addState (loadImg (BinaryData::footswitch_down_png, BinaryData::footswitch_down_pngSize)); // 1 = down (momentary)
    footswitch.onPressedStateChanged = [this] (bool down) { footswitch.setStateIndex (down ? 1 : 0); };
    footswitch.onClick = [this] (const juce::ModifierKeys& mods)
    {
        if (mods.isCtrlDown() || mods.isCommandDown() || mods.isPopupMenu())
        {
            processor.toggleToneVoicing();

            const auto* voicingParam = processor.apvts.getParameter (ParamIDs::toneVoicing);
            const bool lessBright = voicingParam != nullptr && voicingParam->getValue() > 0.5f;
            const bool toneOn = *processor.apvts.getRawParameterValue (ParamIDs::toneEnabled) > 0.5f;
            showOverlay (juce::String ("TONE: ") + (lessBright ? "LESS BRIGHT" : "NORMAL")
                         + (toneOn ? "" : "  (tombol TONE mati)"));
        }
        else
        {
            processor.toggleFxEnabled();
        }
        refreshToggleVisuals();
    };
    addAndMakeVisible (footswitch);

    // --- LEDs ---
    ledStatusFx.addState (loadImg (BinaryData::led_status_red_off_png, BinaryData::led_status_red_off_pngSize)); // 0
    ledStatusFx.addState (loadImg (BinaryData::led_status_red_on_png,  BinaryData::led_status_red_on_pngSize));  // 1
    // klik LED status = menu Key / Scale (Auto = ikuti akor dari input 2, seperti TC-Helicon)
    ledStatusFx.onClick = [this] (const juce::ModifierKeys&) { showKeyMenu(); };
    addAndMakeVisible (ledStatusFx);

    ledPower.addState (loadImg (BinaryData::led_power_green_off_png, BinaryData::led_power_green_off_pngSize)); // 0
    ledPower.addState (loadImg (BinaryData::led_power_green_on_png,  BinaryData::led_power_green_on_pngSize));  // 1
    ledPower.setStateIndex (1); // power LED is on whenever the plugin is loaded
    addAndMakeVisible (ledPower);

    ledRefOut.addState (loadImg (BinaryData::led_refout_amber_off_png, BinaryData::led_refout_amber_off_pngSize)); // 0
    ledRefOut.addState (loadImg (BinaryData::led_refout_amber_on_png,  BinaryData::led_refout_amber_on_pngSize));  // 1
    ledRefOut.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (ledRefOut);

    refreshToggleVisuals();

    // --- ukuran: default kecil (60%), bisa di-resize (40% - 120%), rasio tetap ---
    const double savedScale = juce::jlimit (PedalLayout::minScale, PedalLayout::maxScale,
        static_cast<double> (processor.apvts.state.getProperty ("uiScale", PedalLayout::defaultScale)));

    setResizable (true, true);
    setResizeLimits (juce::roundToInt (PedalLayout::bgWidth  * PedalLayout::minScale),
                     juce::roundToInt (PedalLayout::bgHeight * PedalLayout::minScale),
                     juce::roundToInt (PedalLayout::bgWidth  * PedalLayout::maxScale),
                     juce::roundToInt (PedalLayout::bgHeight * PedalLayout::maxScale));
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio (static_cast<double> (PedalLayout::bgWidth) / PedalLayout::bgHeight);

    setSize (juce::roundToInt (PedalLayout::bgWidth  * savedScale),
             juce::roundToInt (PedalLayout::bgHeight * savedScale));
    startTimerHz (20);
}

HarmonySingerAudioProcessorEditor::~HarmonySingerAudioProcessorEditor()
{
    harmonySlider.setLookAndFeel (nullptr);
    levelSlider.setLookAndFeel (nullptr);
    reverbSlider.setLookAndFeel (nullptr);
    mixInLSlider.setLookAndFeel (nullptr);
}

void HarmonySingerAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::transparentBlack);
    if (backgroundImage.isValid())
    {
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (backgroundImage, getLocalBounds().toFloat());
    }

    // pesan singkat (hilang sendiri setelah ~2 detik)
    const double remaining = overlayUntilMs - juce::Time::getMillisecondCounterHiRes();

    // keterangan kecil di bawah LED status: akor / key yang sedang dipakai harmoni
    if (remaining <= 0.0)
    {
        const juce::String ctx = describeContext (processor.getHarmonyDisplayCode());
        if (ctx.isNotEmpty())
        {
            const float k = static_cast<float> (getWidth()) / static_cast<float> (PedalLayout::bgWidth);
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.setFont (juce::Font (13.0f * k, juce::Font::bold));
            g.drawText (ctx, juce::Rectangle<float> (230.0f * k, 568.0f * k, 220.0f * k, 22.0f * k),
                        juce::Justification::centred, false);
        }
    }
    if (remaining > 0.0 && overlayText.isNotEmpty())
    {
        const float k = static_cast<float> (getWidth()) / static_cast<float> (PedalLayout::bgWidth);
        const float alpha = juce::jlimit (0.0f, 1.0f, static_cast<float> (remaining / 400.0));
        const juce::Rectangle<float> r (50.0f * k, 566.0f * k, 360.0f * k, 32.0f * k);

        g.setColour (juce::Colours::black.withAlpha (0.60f * alpha));
        g.fillRoundedRectangle (r, 8.0f * k);
        g.setColour (juce::Colours::white.withAlpha (alpha));
        g.setFont (juce::Font (14.0f * k, juce::Font::bold));
        g.drawText (overlayText, r, juce::Justification::centred, false);
    }
}

juce::String HarmonySingerAudioProcessorEditor::describeContext (int code)
{
    const int kind = code & 15;
    if (kind == 0)
        return {};

    static const char* roots[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static const char* chordSuffix[6] = { "", "m", "7", "maj7", "m7", "dim" };

    const juce::String root = roots[(code >> 4) & 15];
    const int quality = (code >> 8) & 15;

    if (kind == 2) return "CHORD  " + root + chordSuffix[juce::jlimit (0, 5, quality)];
    if (kind == 3) return "KEY  " + root + (quality == 1 ? " minor" : " major");
    return "KEY  " + root + " (auto)";
}

void HarmonySingerAudioProcessorEditor::showKeyMenu()
{
    auto* rootParam  = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter (ParamIDs::keyRoot));
    auto* scaleParam = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter (ParamIDs::keyScale));
    if (rootParam == nullptr || scaleParam == nullptr)
        return;

    const int curRoot  = rootParam->getIndex();    // 0 = Auto, 1..12 = C..B
    const int curScale = scaleParam->getIndex();   // 0 = Major, 1 = Minor
    const auto names = keyRootNames();

    juce::PopupMenu major, minor;
    for (int i = 1; i <= 12; ++i)
    {
        major.addItem (100 + i, names[i], true, curRoot == i && curScale == 0);
        minor.addItem (200 + i, names[i], true, curRoot == i && curScale == 1);
    }

    juce::PopupMenu menu;
    menu.addSectionHeader ("HARMONY KEY");
    menu.addItem (1, "Auto (ikuti akor dari input 2)", true, curRoot == 0);
    menu.addSubMenu ("Manual - Major", major);
    menu.addSubMenu ("Manual - Minor", minor);

    juce::Component::SafePointer<HarmonySingerAudioProcessorEditor> safeThis (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&ledStatusFx),
                        [safeThis, rootParam, scaleParam, names] (int result)
    {
        if (result == 0 || safeThis == nullptr)
            return;

        int root = 0, scale = 0;
        if (result >= 200)      { root = result - 200; scale = 1; }
        else if (result >= 100) { root = result - 100; scale = 0; }

        rootParam->beginChangeGesture();
        rootParam->setValueNotifyingHost (rootParam->convertTo0to1 (static_cast<float> (root)));
        rootParam->endChangeGesture();

        if (root > 0)
        {
            scaleParam->beginChangeGesture();
            scaleParam->setValueNotifyingHost (scaleParam->convertTo0to1 (static_cast<float> (scale)));
            scaleParam->endChangeGesture();
        }

        safeThis->showOverlay (root == 0 ? juce::String ("KEY: AUTO")
                                         : "KEY: " + names[root] + (scale == 1 ? " minor" : " major"));
    });
}

void HarmonySingerAudioProcessorEditor::showOverlay (const juce::String& text)
{
    overlayText = text;
    overlayUntilMs = juce::Time::getMillisecondCounterHiRes() + 2000.0;
    repaint();
}

void HarmonySingerAudioProcessorEditor::resized()
{
    const double k = static_cast<double> (getWidth()) / PedalLayout::bgWidth;

    placeRotary (harmonySlider, PedalLayout::harmonyKnob, k);
    placeRotary (levelSlider,   PedalLayout::levelKnob,   k);
    placeRotary (reverbSlider,  PedalLayout::reverbKnob,  k);
    placeRotary (mixInLSlider,  PedalLayout::mixInLKnob,  k);

    placeButton (toneButton,   PedalLayout::toneButton,   k);
    placeButton (refOutButton, PedalLayout::refOutButton, k);
    placeButton (footswitch,   PedalLayout::footswitch,   k);

    placeButton (ledStatusFx, PedalLayout::ledStatusFx, k);
    placeButton (ledPower,    PedalLayout::ledPower,    k);
    placeButton (ledRefOut,   PedalLayout::ledRefOut,   k);

    // ingat ukuran terakhir (ikut tersimpan di state plugin / preset DAW)
    processor.apvts.state.setProperty ("uiScale", k, nullptr);
}

void HarmonySingerAudioProcessorEditor::timerCallback()
{
    refreshToggleVisuals();

    const int code = processor.getHarmonyDisplayCode();
    if (code != lastDisplayCode)
    {
        lastDisplayCode = code;
        repaint();
    }

    if (overlayUntilMs > 0.0)
    {
        repaint();
        if (juce::Time::getMillisecondCounterHiRes() > overlayUntilMs)
            overlayUntilMs = 0.0;
    }
}

void HarmonySingerAudioProcessorEditor::refreshToggleVisuals()
{
    const bool toneOn = *processor.apvts.getRawParameterValue (ParamIDs::toneEnabled) > 0.5f;
    toneButton.setStateIndex (toneButtonPressed ? (toneOn ? 3 : 2) : (toneOn ? 1 : 0));

    const bool fxOn = *processor.apvts.getRawParameterValue (ParamIDs::fxEnabled) > 0.5f;
    ledStatusFx.setStateIndex (fxOn ? 1 : 0);

    const bool refOutOn = *processor.apvts.getRawParameterValue (ParamIDs::refOut) > 0.5f;
    refOutButton.setStateIndex (refOutOn ? 1 : 0);
    ledRefOut.setStateIndex (refOutOn ? 1 : 0);   // LED REF OUT: menyala = output 2/R aktif
}
