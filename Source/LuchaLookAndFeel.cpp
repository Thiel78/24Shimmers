#include "LuchaLookAndFeel.h"

using namespace juce;

namespace Lucha
{
Font font (float height, bool bold)
{
    return Font (FontOptions (height, bold ? Font::bold : Font::plain));
}

void drawOutlinedText (Graphics& g, const String& text, Rectangle<float> area, Font f, Colour fill, Justification just)
{
    GlyphArrangement glyphs;
    glyphs.addFittedText (f, text, area.getX(), area.getY(), area.getWidth(), area.getHeight(), just, 1);

    Path p;
    glyphs.createPath (p);

    g.setColour (Colours::ink);
    g.strokePath (p, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (fill);
    g.fillPath (p);
}
}

LuchaLookAndFeel::LuchaLookAndFeel()
{
    setColour (Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
}

void LuchaLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int width, int height, float sliderPos,
                                         float startAngle, float endAngle, Slider& slider)
{
    using namespace Lucha::Colours;

    const bool big = slider.getProperties()["lucha_big"];
    const auto bounds = Rectangle<int> (x, y, width, height).toFloat().reduced (3.0f);
    const float r = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = startAngle + sliderPos * (endAngle - startAngle);
    auto circle = [c] (float radius) { return Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (c); };

    // Drop shadow + gold bezel.
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (circle (r).translated (0.0f, 2.5f));

    g.setGradientFill (ColourGradient (gold.brighter (0.4f), c.x, c.y - r, goldDark, c.x, c.y + r, false));
    g.fillEllipse (circle (r));

    // Track ring with the value arc.
    const float ringOuter = r * 0.86f, ringInner = r * 0.66f;
    g.setColour (ink);
    g.fillEllipse (circle (ringOuter));

    const float arcRadius = (ringOuter + ringInner) * 0.5f;
    const float arcWidth = (ringOuter - ringInner) * 0.62f;

    Path track;
    track.addCentredArc (c.x, c.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.strokePath (track, PathStrokeType (arcWidth, PathStrokeType::curved, PathStrokeType::rounded));

    if (sliderPos > 0.001f)
    {
        Path value;
        value.addCentredArc (c.x, c.y, arcRadius, arcRadius, 0.0f, startAngle, angle, true);
        const auto arcColour = big ? pink : red.brighter (0.25f);
        g.setColour (arcColour.withAlpha (0.35f));
        g.strokePath (value, PathStrokeType (arcWidth * 2.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (arcColour);
        g.strokePath (value, PathStrokeType (arcWidth, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // Face.
    g.setGradientFill (ColourGradient (big ? maskBlue.brighter (0.2f) : Colour (0xff2a2440), c.x, c.y - ringInner,
                                       big ? maskDeep : Colour (0xff0e0b18), c.x, c.y + ringInner, false));
    g.fillEllipse (circle (ringInner));

    // Pointer.
    const auto tip = c.getPointOnCircumference (ringInner * 0.88f, angle);
    const auto base = c.getPointOnCircumference (ringInner * 0.25f, angle);
    g.setColour (ink.withAlpha (0.6f));
    g.drawLine ({ base.translated (0.0f, 1.5f), tip.translated (0.0f, 1.5f) }, big ? 5.0f : 4.0f);
    g.setColour (cream);
    g.drawLine ({ base, tip }, big ? 4.0f : 3.0f);

    // Centre cap: a little star for the big knobs, a stud for the small ones.
    if (big)
    {
        Path star;
        star.addStar (c, 5, ringInner * 0.13f, ringInner * 0.3f, angle);
        g.setColour (gold);
        g.fillPath (star);
        g.setColour (ink);
        g.strokePath (star, PathStrokeType (1.2f));
    }
    else
    {
        g.setColour (red);
        g.fillEllipse (circle (ringInner * 0.22f));
        g.setColour (gold);
        g.drawEllipse (circle (ringInner * 0.22f), 1.5f);
    }
}

int LuchaLookAndFeel::getSliderThumbRadius (Slider& slider)
{
    return slider.isHorizontal() ? 62 : LookAndFeel_V4::getSliderThumbRadius (slider);
}

// The mix slider is drawn as a championship belt: leather strap, studs, gold plate.
void LuchaLookAndFeel::drawLinearSlider (Graphics& g, int x, int y, int width, int height, float sliderPos,
                                         float minSliderPos, float maxSliderPos, Slider::SliderStyle style, Slider& slider)
{
    using namespace Lucha::Colours;

    if (style != Slider::LinearHorizontal)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
        return;
    }

    // JUCE insets the track by the thumb radius; the strap should span the whole slider.
    ignoreUnused (x, width);
    const auto bounds = slider.getLocalBounds().toFloat().withY ((float) y).withHeight ((float) height);
    const float strapH = bounds.getHeight() * 0.5f;
    auto strap = bounds.withSizeKeepingCentre (bounds.getWidth(), strapH);

    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (strap.translated (0.0f, 3.0f), strapH * 0.3f);
    g.setGradientFill (ColourGradient (Colour (0xff2b1d1a), 0.0f, strap.getY(), Colour (0xff120c0b), 0.0f, strap.getBottom(), false));
    g.fillRoundedRectangle (strap, strapH * 0.3f);

    // Glowing fill up to the plate.
    auto filled = strap.reduced (4.0f).withRight (sliderPos);
    g.setGradientFill (ColourGradient (red.withAlpha (0.55f), filled.getX(), 0.0f, pink, filled.getRight(), 0.0f, false));
    g.fillRoundedRectangle (filled, 4.0f);

    // Stitching and studs.
    const float dash[] = { 5.0f, 4.0f };
    for (float yy : { strap.getY() + 3.0f, strap.getBottom() - 3.0f })
    {
        Path stitch;
        stitch.startNewSubPath (strap.getX() + 8.0f, yy);
        stitch.lineTo (strap.getRight() - 8.0f, yy);
        Path dashed;
        PathStrokeType (1.2f).createDashedStroke (dashed, stitch, dash, 2);
        g.setColour (gold.withAlpha (0.55f));
        g.fillPath (dashed);
    }

    for (float sx = strap.getX() + 22.0f; sx < strap.getRight() - 12.0f; sx += 34.0f)
    {
        auto stud = Rectangle<float> (7.0f, 7.0f).withCentre ({ sx, strap.getCentreY() });
        g.setGradientFill (ColourGradient (cream, stud.getX(), stud.getY(), goldDark, stud.getRight(), stud.getBottom(), false));
        g.fillEllipse (stud);
    }

    // Plate.
    const float plateW = 124.0f;
    auto plate = Rectangle<float> (plateW, bounds.getHeight() - 4.0f).withCentre ({ sliderPos, bounds.getCentreY() });

    Path platePath;
    platePath.addRoundedRectangle (plate, 14.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillPath (platePath, AffineTransform::translation (0.0f, 3.0f));
    g.setGradientFill (ColourGradient (gold.brighter (0.5f), plate.getX(), plate.getY(), goldDark, plate.getRight(), plate.getBottom(), false));
    g.fillPath (platePath);
    g.setColour (ink);
    g.strokePath (platePath, PathStrokeType (2.0f));

    auto inner = plate.reduced (7.0f);
    g.setColour (red);
    g.fillRoundedRectangle (inner, 9.0f);
    g.setColour (gold.brighter (0.3f));
    g.drawRoundedRectangle (inner, 9.0f, 1.5f);

    g.setColour (cream);
    g.setFont (Lucha::font (12.0f));
    g.drawText ("MIX", inner.removeFromTop (inner.getHeight() * 0.42f), Justification::centredBottom);
    g.setFont (Lucha::font (17.0f));
    g.drawText (slider.getTextFromValue (slider.getValue()), inner, Justification::centredTop);
}
