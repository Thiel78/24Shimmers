#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "MaskArt.h"
#include "PluginProcessor.h"

// An invisible hit area shaped like one feature of the mask, wired to a parameter.
// The editor draws the feature; this component only handles the mouse.
class MaskControl : public juce::Component
{
public:
    enum class Kind { drag, cycle, toggle };

    MaskControl (juce::RangedAudioParameter& param, juce::String label, Kind kind, std::function<void()> onChange);

    void setHitArea (const juce::Path& areaInParent);

    float getNormalisedValue() const { return param.getValue(); }
    int getStep() const;
    const juce::String& getLabel() const { return label; }
    juce::String getValueText() const { return param.getCurrentValueAsText(); }
    bool isActive() const { return isMouseOverOrDragging(); }

    bool hitTest (int x, int y) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    juce::RangedAudioParameter& param;
    juce::String label;
    Kind kind;
    std::function<void()> onChange;
    juce::ParameterAttachment attachment;
    juce::Path hitArea;
    float dragStartValue = 0.0f;
    bool dragging = false;
};

class ShimmerEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ShimmerEditor (ShimmerProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;

    // startAgeRange > 0 starts sparkles part-way through their life (used for previews).
    void spawnSparkles (int count, float startAgeRange = 0.0f);

    const MaskArt::Layout& getLayout() const { return layout; }
    juce::Component* getControl (MaskArt::Feature f) const { return controls[(size_t) f].get(); }

private:
    struct Sparkle
    {
        juce::Point<float> pos, velocity;
        float size, age, life;
        juce::Colour colour;
    };

    void timerCallback() override;
    void drawBanner (juce::Graphics&);
    const MaskControl& control (MaskArt::Feature f) const { return *controls[(size_t) f]; }

    ShimmerProcessor& processor;

    std::vector<std::unique_ptr<MaskControl>> controls; // indexed by MaskArt::Feature
    MaskArt::Layout layout;

    const juce::Rectangle<float> maskArea { 100.0f, 60.0f, 480.0f, 560.0f };
    const juce::Rectangle<float> bannerArea { 60.0f, 640.0f, 560.0f, 56.0f };

    std::vector<Sparkle> sparkles;
    juce::Random random;
    float level = 0.0f, spawnBudget = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ShimmerEditor)
};
