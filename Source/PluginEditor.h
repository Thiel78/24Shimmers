#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "LuchaLookAndFeel.h"
#include "PluginProcessor.h"

// A rotary knob with its name and current value underneath.
// On the mask the text sits in a black tag so it reads over the artwork.
class LuchaKnob : public juce::Component
{
public:
    LuchaKnob (juce::AudioProcessorValueTreeState& state, const juce::String& paramID, juce::String label, bool onMask);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Slider slider;
    juce::RangedAudioParameter& param;
    juce::String label;
    bool onMask;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// Lives in the mask's mouth.
class FreezeButton : public juce::Button
{
public:
    FreezeButton();
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

class ShimmerEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ShimmerEditor (ShimmerProcessor&);
    ~ShimmerEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // startAgeRange > 0 starts sparkles part-way through their life (used for previews).
    void spawnSparkles (int count, float startAgeRange = 0.0f);

private:
    struct Sparkle
    {
        juce::Point<float> pos, velocity;
        float size, age, life;
        juce::Colour colour;
    };

    void timerCallback() override;
    void drawPanel (juce::Graphics&, juce::Rectangle<float> area, const juce::String& title, const juce::String& subtitle);

    ShimmerProcessor& processor;
    LuchaLookAndFeel lookAndFeel;

    LuchaKnob shimmerKnob, decayKnob;
    LuchaKnob sizeKnob, preDelayKnob, toneKnob, lowCutKnob;
    LuchaKnob shift1Knob, shift2Knob, blendKnob, modKnob;

    juce::Slider mixSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;

    FreezeButton freezeButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeAttachment;

    juce::Rectangle<float> maskArea { 240.0f, 68.0f, 400.0f, 424.0f };
    juce::Rectangle<float> leftPanel { 20.0f, 76.0f, 200.0f, 408.0f };
    juce::Rectangle<float> rightPanel { 660.0f, 76.0f, 200.0f, 408.0f };
    juce::Rectangle<int> beltArea { 70, 508, 740, 74 };

    std::vector<Sparkle> sparkles;
    juce::Random random;
    float level = 0.0f, spawnBudget = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ShimmerEditor)
};
