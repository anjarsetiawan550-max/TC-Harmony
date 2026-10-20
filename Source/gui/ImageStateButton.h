#pragma once
#include <JuceHeader.h>

/**
    A simple component that displays one of several pre-rendered PNG
    states (e.g. "up"/"down", or "led_on"/"led_off") and reports clicks
    (with modifier keys) via a callback. Used for the TONE button, REF
    OUT switch, footswitch and both LEDs, all of which are supplied as
    discrete photographed states rather than a single drawable shape.
*/
class ImageStateButton : public juce::Component
{
public:
    void addState (juce::Image image) { states.push_back (image); }

    void setStateIndex (int index)
    {
        index = juce::jlimit (0, static_cast<int> (states.size()) - 1, index);
        if (index != currentIndex)
        {
            currentIndex = index;
            repaint();
        }
    }

    int getStateIndex() const noexcept { return currentIndex; }

    // Called on mouseUp with the modifier keys that were held, unless this
    // button is purely a visual indicator (no callback set -> clicks ignored).
    std::function<void (const juce::ModifierKeys&)> onClick;

    // If set, this is called on mouseDown/mouseUp to drive a momentary
    // "pressed" look (used by the footswitch / tone button artwork).
    std::function<void (bool isDown)> onPressedStateChanged;

    void paint (juce::Graphics& g) override
    {
        if (currentIndex >= 0 && currentIndex < static_cast<int> (states.size()) && states[static_cast<size_t> (currentIndex)].isValid())
        {
            g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
            g.drawImage (states[static_cast<size_t> (currentIndex)], getLocalBounds().toFloat());
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        // Simpan modifier saat tombol mouse DITEKAN: kalau Ctrl sudah dilepas sebelum
        // mouse dilepas (atau host tidak meneruskannya di mouseUp), Ctrl+klik tetap terbaca.
        downMods = e.mods;
        if (onPressedStateChanged) onPressedStateChanged (true);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (onPressedStateChanged) onPressedStateChanged (false);
        if (onClick && contains (e.getPosition()))
        {
            const int keep = juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::commandModifier
                           | juce::ModifierKeys::altModifier  | juce::ModifierKeys::shiftModifier
                           | juce::ModifierKeys::popupMenuClickModifier;

            const int flags = (e.mods.getRawFlags() | downMods.getRawFlags()
                               | juce::ModifierKeys::getCurrentModifiers().getRawFlags()) & keep;

            onClick (juce::ModifierKeys (flags));
        }
    }

private:
    std::vector<juce::Image> states;
    int currentIndex = 0;
    juce::ModifierKeys downMods;
};
