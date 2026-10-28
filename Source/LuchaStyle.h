#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace Lucha
{
namespace Colours
{
    inline const juce::Colour maskBlue  { 0xff1e4fe0 };
    inline const juce::Colour maskDeep  { 0xff0b2380 };
    inline const juce::Colour gold      { 0xfff7c531 };
    inline const juce::Colour goldDark  { 0xffa8740a };
    inline const juce::Colour red       { 0xffe8173f };
    inline const juce::Colour redDeep   { 0xff7a0b24 };
    inline const juce::Colour pink      { 0xffff3d7f };
    inline const juce::Colour ink       { 0xff0b0b12 };
    inline const juce::Colour cream     { 0xfffff4d6 };
    inline const juce::Colour ice       { 0xff7fe7ff };
}

juce::Font font (float height, bool bold = true);

// Draws text with a dark outline so it stays readable on the busy mask.
void drawOutlinedText (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area,
                       juce::Font f, juce::Colour fill, juce::Justification just = juce::Justification::centred,
                       float outline = 3.0f);
}
