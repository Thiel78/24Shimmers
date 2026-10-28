#include "MaskArt.h"
#include "LuchaLookAndFeel.h"

using namespace juce;
using namespace Lucha::Colours;

namespace
{
constexpr float eyeX = 0.29f, eyeY = 0.475f, eyeTilt = 0.2f;

AffineTransform toMask (Rectangle<float> r)
{
    return AffineTransform::scale (r.getWidth(), r.getHeight()).translated (r.getX(), r.getY());
}

const AffineTransform mirror = AffineTransform::scale (-1.0f, 1.0f).translated (1.0f, 0.0f);

Path unitHead()
{
    Path p;
    p.startNewSubPath (0.5f, 0.0f);
    p.cubicTo (0.80f, 0.0f, 0.97f, 0.17f, 0.98f, 0.42f);
    p.cubicTo (0.99f, 0.66f, 0.80f, 0.97f, 0.5f, 1.0f);
    p.cubicTo (0.20f, 0.97f, 0.01f, 0.66f, 0.02f, 0.42f);
    p.cubicTo (0.03f, 0.17f, 0.20f, 0.0f, 0.5f, 0.0f);
    p.closeSubPath();
    return p;
}

Path unitCrest()
{
    Path p;
    p.startNewSubPath (0.35f, -0.02f);
    p.lineTo (0.65f, -0.02f);
    p.lineTo (0.60f, 0.14f);
    p.quadraticTo (0.56f, 0.36f, 0.5f, 0.64f);
    p.quadraticTo (0.44f, 0.36f, 0.40f, 0.14f);
    p.closeSubPath();
    return p;
}

// Left eye trim: an almond that sweeps up into a flame towards the temple.
Path unitEyeTrim()
{
    Path p;
    p.startNewSubPath (0.475f, 0.535f);
    p.cubicTo (0.455f, 0.36f, 0.30f, 0.29f, 0.17f, 0.30f);
    p.cubicTo (0.11f, 0.30f, 0.065f, 0.25f, 0.035f, 0.18f);
    p.cubicTo (0.035f, 0.31f, 0.06f, 0.41f, 0.095f, 0.47f);
    p.cubicTo (0.10f, 0.61f, 0.22f, 0.675f, 0.33f, 0.655f);
    p.cubicTo (0.42f, 0.645f, 0.47f, 0.60f, 0.475f, 0.535f);
    p.closeSubPath();
    return p;
}

Path unitEyeHole (float scale)
{
    Path p;
    p.addEllipse (eyeX - 0.135f * scale, eyeY - 0.118f * scale, 0.27f * scale, 0.236f * scale);
    p.applyTransform (AffineTransform::rotation (eyeTilt, eyeX, eyeY));
    return p;
}

// Lightning bolt running down the left cheek.
Path unitCheekBolt()
{
    Path p;
    p.startNewSubPath (0.085f, 0.585f);
    p.lineTo (0.205f, 0.695f);
    p.lineTo (0.165f, 0.705f);
    p.lineTo (0.315f, 0.875f);
    p.lineTo (0.175f, 0.745f);
    p.lineTo (0.215f, 0.735f);
    p.lineTo (0.075f, 0.62f);
    p.closeSubPath();
    return p;
}

void fillWithOutline (Graphics& g, const Path& p, Colour fill, float outline)
{
    g.setColour (ink);
    g.strokePath (p, PathStrokeType (outline, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (fill);
    g.fillPath (p);
}

void fillGold (Graphics& g, const Path& p, Rectangle<float> area, float outline)
{
    g.setColour (ink);
    g.strokePath (p, PathStrokeType (outline, PathStrokeType::curved, PathStrokeType::rounded));
    g.setGradientFill (ColourGradient (gold.brighter (0.45f), area.getX(), area.getY(), goldDark, area.getRight(), area.getBottom(), false));
    g.fillPath (p);
}

void stitch (Graphics& g, const Path& p, Colour colour, float width)
{
    const float dash[] = { 5.0f, 4.0f };
    Path dashed;
    PathStrokeType (width).createDashedStroke (dashed, p, dash, 2);
    g.setColour (colour);
    g.fillPath (dashed);
}
}

namespace MaskArt
{
Path headOutline (Rectangle<float> r)
{
    auto p = unitHead();
    p.applyTransform (toMask (r));
    return p;
}

Point<float> eyeCentre (Rectangle<float> r, bool left)
{
    return { r.getX() + r.getWidth() * (left ? eyeX : 1.0f - eyeX), r.getY() + r.getHeight() * eyeY };
}

Rectangle<float> mouthBounds (Rectangle<float> r)
{
    return { r.getX() + r.getWidth() * 0.37f, r.getY() + r.getHeight() * 0.775f, r.getWidth() * 0.26f, r.getHeight() * 0.12f };
}

void drawBackground (Graphics& g, Rectangle<float> b)
{
    g.setGradientFill (ColourGradient (Colour (0xff22123f), b.getCentreX(), b.getY() + b.getHeight() * 0.4f,
                                       Colour (0xff05030a), b.getX(), b.getBottom(), true));
    g.fillRect (b);

    // Spotlight behind the mask.
    g.setGradientFill (ColourGradient (pink.withAlpha (0.22f), b.getCentreX(), b.getY() + b.getHeight() * 0.42f,
                                       pink.withAlpha (0.0f), b.getCentreX() + b.getWidth() * 0.36f, b.getY() + b.getHeight() * 0.42f, true));
    g.fillRect (b);

    // Ring ropes.
    const Colour ropes[] = { red, cream, maskBlue };
    for (int i = 0; i < 3; ++i)
    {
        const float y = b.getY() + b.getHeight() * (0.36f + 0.15f * (float) i);
        Path rope;
        rope.startNewSubPath (b.getX(), y);
        rope.quadraticTo (b.getCentreX(), y + 14.0f, b.getRight(), y);

        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.strokePath (rope, PathStrokeType (8.0f), AffineTransform::translation (0.0f, 4.0f));
        g.setColour (ropes[i].withAlpha (0.55f));
        g.strokePath (rope, PathStrokeType (7.0f));
        g.setColour (juce::Colours::white.withAlpha (0.18f));
        g.strokePath (rope, PathStrokeType (1.5f), AffineTransform::translation (0.0f, -1.5f));
    }
}

void drawMask (Graphics& g, Rectangle<float> r)
{
    const auto t = toMask (r);
    auto mapped = [&t] (Path p, bool mirrored = false)
    {
        if (mirrored)
            p.applyTransform (mirror);
        p.applyTransform (t);
        return p;
    };
    auto at = [&r] (float u, float v) { return Point<float> (r.getX() + u * r.getWidth(), r.getY() + v * r.getHeight()); };

    const auto head = mapped (unitHead());

    DropShadow (juce::Colours::black.withAlpha (0.7f), 28, { 0, 10 }).drawForPath (g, head);

    // Base fabric with a soft sheen.
    g.setGradientFill (ColourGradient (maskBlue.brighter (0.25f), at (0.4f, 0.15f), maskDeep, at (0.5f, 1.05f), false));
    g.fillPath (head);

    {
        Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (head);

        // Crest down the middle, with the forehead star.
        const auto crest = mapped (unitCrest());
        fillWithOutline (g, crest, red, 3.0f);
        stitch (g, mapped (unitCrest()), gold.withAlpha (0.8f), 1.3f);

        // Cheek bolts.
        for (bool m : { false, true })
            fillWithOutline (g, mapped (unitCheekBolt(), m), red, 2.5f);

        // Eyes: gold flame trim, red band, black hole.
        for (bool m : { false, true })
        {
            const auto trim = mapped (unitEyeTrim(), m);
            fillGold (g, trim, trim.getBounds(), 3.5f);
            fillWithOutline (g, mapped (unitEyeHole (1.2f), m), red, 2.0f);

            const auto hole = mapped (unitEyeHole (1.0f), m);
            g.setColour (ink);
            g.fillPath (hole);
        }

        // Mouth trim.
        Path mouthTrim;
        mouthTrim.addEllipse (0.335f, 0.75f, 0.33f, 0.17f);
        fillGold (g, mapped (mouthTrim), mapped (mouthTrim).getBounds(), 3.0f);
        Path mouthBand;
        mouthBand.addEllipse (0.355f, 0.765f, 0.29f, 0.14f);
        fillWithOutline (g, mapped (mouthBand), red, 2.0f);

        // Breathing holes.
        for (float u : { 0.468f, 0.532f })
        {
            const auto c = at (u, 0.70f);
            const float rad = r.getWidth() * 0.011f;
            g.setColour (gold);
            g.fillEllipse (Rectangle<float> (rad * 3.2f, rad * 3.2f).withCentre (c));
            g.setColour (ink);
            g.fillEllipse (Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (c));
        }

        // Light from the top-left, shade towards the edges.
        g.setGradientFill (ColourGradient (juce::Colours::white.withAlpha (0.16f), at (0.33f, 0.12f),
                                           juce::Colours::white.withAlpha (0.0f), at (0.75f, 0.45f), true));
        g.fillPath (head);
        g.setGradientFill (ColourGradient (juce::Colours::black.withAlpha (0.0f), at (0.5f, 0.45f),
                                           juce::Colours::black.withAlpha (0.45f), at (1.02f, 0.45f), true));
        g.fillPath (head);
    }

    // Forehead medallion (drawn outside the shading so it pops).
    const auto medal = at (0.5f, 0.17f);
    const float medalR = r.getWidth() * 0.085f;
    Path star;
    star.addStar (medal, 5, medalR * 0.75f, medalR * 1.55f, 0.0f);
    fillGold (g, star, star.getBounds(), 3.0f);

    auto disc = Rectangle<float> (medalR * 2.0f, medalR * 2.0f).withCentre (medal);
    g.setColour (ink);
    g.fillEllipse (disc.expanded (2.5f));
    g.setGradientFill (ColourGradient (pink, disc.getX(), disc.getY(), red.darker (0.3f), disc.getRight(), disc.getBottom(), false));
    g.fillEllipse (disc);
    g.setColour (gold);
    g.drawEllipse (disc.reduced (2.5f), 1.5f);
    Lucha::drawOutlinedText (g, "24", disc.translated (0.0f, 1.0f), Lucha::font (medalR * 1.15f), cream);

    // Outline and stitching around the edge.
    g.setColour (ink);
    g.strokePath (head, PathStrokeType (3.5f));

    auto inner = unitHead();
    inner.applyTransform (AffineTransform::scale (0.965f, 0.975f, 0.5f, 0.5f));
    stitch (g, mapped (inner), gold.withAlpha (0.55f), 1.2f);
}

void drawSparkle (Graphics& g, Point<float> c, float size, Colour colour, float alpha)
{
    g.setColour (colour.withAlpha (alpha * 0.25f));
    g.fillEllipse (Rectangle<float> (size * 1.4f, size * 1.4f).withCentre (c));

    Path p;
    p.addStar (c, 4, size * 0.16f, size, 0.0f);
    g.setColour (colour.withAlpha (alpha));
    g.fillPath (p);
}
}
