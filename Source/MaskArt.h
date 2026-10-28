#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Vector artwork for the luchador mask and the ring around it.
// All mask geometry is defined in a unit square and mapped onto the mask bounds.
namespace MaskArt
{
void drawBackground (juce::Graphics&, juce::Rectangle<float> bounds);
void drawMask (juce::Graphics&, juce::Rectangle<float> maskBounds);

// Where the controls that live on the mask go.
juce::Point<float> eyeCentre (juce::Rectangle<float> maskBounds, bool left);
juce::Rectangle<float> mouthBounds (juce::Rectangle<float> maskBounds);
juce::Path headOutline (juce::Rectangle<float> maskBounds);

void drawSparkle (juce::Graphics&, juce::Point<float> centre, float size, juce::Colour colour, float alpha);
}
