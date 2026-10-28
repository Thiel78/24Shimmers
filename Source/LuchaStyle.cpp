#include "LuchaStyle.h"

using namespace juce;

namespace Lucha
{
Font font (float height, bool bold)
{
    return Font (FontOptions (height, bold ? Font::bold : Font::plain));
}

void drawOutlinedText (Graphics& g, const String& text, Rectangle<float> area, Font f, Colour fill,
                       Justification just, float outline)
{
    GlyphArrangement glyphs;
    glyphs.addFittedText (f, text, area.getX(), area.getY(), area.getWidth(), area.getHeight(), just, 1);

    Path p;
    glyphs.createPath (p);

    g.setColour (Colours::ink);
    g.strokePath (p, PathStrokeType (outline, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (fill);
    g.fillPath (p);
}
}
