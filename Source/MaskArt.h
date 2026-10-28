#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>

// Vector artwork for the luchador mask. Every control is a feature of the mask,
// drawn from its current value. Geometry is defined in a unit square and
// mapped onto the mask bounds.
namespace MaskArt
{
enum class Feature
{
    medallion,  // space
    crest,      // mix
    eyeLeft,    // shimmer
    eyeRight,   // decay
    flameLeft,  // octave
    flameRight, // size
    boltLeft,   // tone
    boltRight,  // mod
    studLeft,   // pre-delay
    studRight,  // low cut
    mouth,      // freeze
    count
};

constexpr size_t numFeatures = (size_t) Feature::count;

struct Layout
{
    juce::Rectangle<float> bounds;
    juce::Path head;
    std::array<juce::Path, numFeatures> hitAreas;   // in the same coordinates as bounds

    juce::Point<float> eyeCentre[2], studCentre[2], medallionCentre;
    float eyeRadius = 0, studRadius = 0, medallionRadius = 0;
};

Layout makeLayout (juce::Rectangle<float> maskBounds);

struct FeatureState
{
    float value = 0.0f;     // normalised 0..1
    int step = 0;           // for stepped controls
    juce::String label, valueText;
    bool active = false;    // hovered or being dragged
};

using State = std::array<FeatureState, numFeatures>;

void drawBackground (juce::Graphics&, juce::Rectangle<float> bounds);
void drawMask (juce::Graphics&, const Layout&, const State&);
void drawSparkle (juce::Graphics&, juce::Point<float> centre, float size, juce::Colour colour, float alpha);
}
