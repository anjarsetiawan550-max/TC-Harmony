#pragma once
#include <JuceHeader.h>

/**
    Draws a juce::Slider (rotary style) as one frame picked out of a
    vertically-stacked filmstrip PNG, exactly as described in the asset
    pack's README (knobs/*_strip.png: N frames of frameW x frameH,
    stacked top to bottom). The frame index is chosen from the slider's
    normalised 0..1 position.
*/
class FilmstripLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void setStrip (juce::Image stripImage, int frameWidth, int frameHeight)
    {
        strip = stripImage;
        frameW = frameWidth;
        frameH = frameHeight;
        numFrames = frameH > 0 ? strip.getHeight() / frameH : 1;
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                            float sliderPosProportional, float /*rotaryStartAngle*/,
                            float /*rotaryEndAngle*/, juce::Slider&) override
    {
        if (! strip.isValid() || numFrames <= 0)
            return;

        const int frameIndex = juce::jlimit (0, numFrames - 1,
            static_cast<int> (std::round (sliderPosProportional * static_cast<float> (numFrames - 1))));

        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (strip, x, y, width, height,
                      0, frameIndex * frameH, frameW, frameH);
    }

private:
    juce::Image strip;
    int frameW = 200, frameH = 200, numFrames = 1;
};
