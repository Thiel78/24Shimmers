#include "MaskArt.h"
#include "LuchaStyle.h"

using namespace juce;
using namespace Lucha::Colours;

namespace
{
constexpr float eyeU = 0.29f, eyeV = 0.47f, eyeRadiusU = 0.13f;
constexpr float studU = 0.43f, studV = 0.705f, studRadiusU = 0.033f;
constexpr float medallionV = 0.165f, medallionRadiusU = 0.09f;
constexpr float flameBaseV = 0.355f, flameTipV = 0.135f;
constexpr float boltTopV = 0.69f, boltBottomV = 0.88f;
constexpr float crestFillBottomV = 0.47f, crestFillTopV = 0.25f;
constexpr float arcStart = -2.35f, arcEnd = 2.35f;

const AffineTransform mirror = AffineTransform::scale (-1.0f, 1.0f).translated (1.0f, 0.0f);

AffineTransform toMask (Rectangle<float> r)
{
    return AffineTransform::scale (r.getWidth(), r.getHeight()).translated (r.getX(), r.getY());
}

//==============================================================================
// Unit-square geometry. Paired features are defined for the left side and mirrored.

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

Path unitEyeTrim()
{
    Path p;
    p.startNewSubPath (0.475f, 0.535f);
    p.cubicTo (0.46f, 0.37f, 0.32f, 0.30f, 0.19f, 0.31f);
    p.cubicTo (0.13f, 0.315f, 0.09f, 0.37f, 0.085f, 0.43f);
    p.cubicTo (0.08f, 0.50f, 0.11f, 0.62f, 0.20f, 0.66f);
    p.cubicTo (0.30f, 0.70f, 0.44f, 0.64f, 0.475f, 0.535f);
    p.closeSubPath();
    return p;
}

// Eyebrow flame: three tongues rising from the top of the eye trim towards the temple.
Path unitFlame()
{
    Path p;
    p.startNewSubPath (0.31f, 0.318f);
    p.cubicTo (0.30f, 0.27f, 0.27f, 0.24f, 0.255f, 0.20f);
    p.cubicTo (0.25f, 0.18f, 0.24f, 0.165f, 0.225f, 0.15f);
    p.cubicTo (0.225f, 0.19f, 0.215f, 0.215f, 0.20f, 0.225f);
    p.cubicTo (0.195f, 0.20f, 0.18f, 0.165f, 0.155f, 0.135f);
    p.cubicTo (0.16f, 0.18f, 0.15f, 0.22f, 0.135f, 0.245f);
    p.cubicTo (0.125f, 0.23f, 0.11f, 0.215f, 0.095f, 0.205f);
    p.cubicTo (0.10f, 0.26f, 0.105f, 0.31f, 0.115f, 0.355f);
    p.cubicTo (0.17f, 0.33f, 0.24f, 0.315f, 0.31f, 0.318f);
    p.closeSubPath();
    return p;
}

// Lightning bolt on the cheek, between the eye and the mouth.
Path unitBolt()
{
    // Classic bolt in a unit box, then placed on the cheek.
    const float pts[][2] { { 0.58f, 0.0f }, { 0.08f, 0.56f }, { 0.42f, 0.56f }, { 0.22f, 1.0f },
                           { 0.92f, 0.38f }, { 0.56f, 0.38f }, { 0.84f, 0.0f } };
    const float x = 0.17f, y = boltTopV, w = 0.14f, h = boltBottomV - boltTopV;

    Path p;
    p.startNewSubPath (x + pts[0][0] * w, y + pts[0][1] * h);
    for (int i = 1; i < 7; ++i)
        p.lineTo (x + pts[i][0] * w, y + pts[i][1] * h);
    p.closeSubPath();
    return p;
}

Path unitEllipse (float x, float y, float w, float h)
{
    Path p;
    p.addEllipse (x, y, w, h);
    return p;
}

//==============================================================================
Path mapped (Path p, Rectangle<float> r, bool mirrored = false)
{
    if (mirrored)
        p.applyTransform (mirror);
    p.applyTransform (toMask (r));
    return p;
}

Point<float> at (Rectangle<float> r, float u, float v)
{
    return { r.getX() + u * r.getWidth(), r.getY() + v * r.getHeight() };
}

Rectangle<float> circle (Point<float> c, float radius)
{
    return Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (c);
}

void outlineAndFill (Graphics& g, const Path& p, Colour fill, float outline)
{
    g.setColour (ink);
    g.strokePath (p, PathStrokeType (outline, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (fill);
    g.fillPath (p);
}

void fillGold (Graphics& g, const Path& p, float outline, float brightness = 0.0f)
{
    const auto b = p.getBounds();
    g.setColour (ink);
    g.strokePath (p, PathStrokeType (outline, PathStrokeType::curved, PathStrokeType::rounded));
    g.setGradientFill (ColourGradient (gold.brighter (0.45f + brightness), b.getX(), b.getY(),
                                       goldDark.brighter (brightness), b.getRight(), b.getBottom(), false));
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

void highlight (Graphics& g, const Path& p, bool active)
{
    if (! active)
        return;

    g.setColour (juce::Colours::white.withAlpha (0.25f));
    g.strokePath (p, PathStrokeType (6.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.strokePath (p, PathStrokeType (1.8f, PathStrokeType::curved, PathStrokeType::rounded));
}

// Fills the part of a shape below (or above) a level with a glowing gradient.
void fillLevel (Graphics& g, const Path& shape, float levelY, bool fromBottom, Colour hot, Colour warm)
{
    const auto b = shape.getBounds();
    Graphics::ScopedSaveState save (g);
    g.reduceClipRegion (shape);

    auto lit = fromBottom ? b.withTop (levelY) : b.withBottom (levelY);
    g.setGradientFill (ColourGradient (hot, b.getCentreX(), fromBottom ? levelY : b.getY(),
                                       warm, b.getCentreX(), fromBottom ? b.getBottom() : levelY, false));
    g.fillRect (lit);

    // Bright edge at the level.
    g.setColour (juce::Colours::white.withAlpha (0.8f));
    g.fillRect (b.withY (levelY - 1.0f).withHeight (2.0f));
}

void drawLabel (Graphics& g, Rectangle<float> mask, float u, float v, const String& label, const String& value, bool active)
{
    const auto c = at (mask, u, v);
    const auto area = Rectangle<float> (110.0f, 30.0f).withCentre (c);
    Lucha::drawOutlinedText (g, label, area.withHeight (13.0f), Lucha::font (11.0f), active ? cream : gold, Justification::centred, 2.5f);
    if (value.isNotEmpty())
        Lucha::drawOutlinedText (g, value, area.withTrimmedTop (13.0f), Lucha::font (13.5f), cream, Justification::centred, 2.5f);
}

//==============================================================================
void drawEye (Graphics& g, Point<float> c, float r, const MaskArt::FeatureState& s, Colour accent)
{
    const float ringOuter = r * 1.22f;
    const float ringMid = (r + ringOuter) * 0.5f;
    const float ringW = (ringOuter - r) * 0.62f;

    // Red band holding the value arc.
    g.setColour (ink);
    g.fillEllipse (circle (c, ringOuter + 2.0f));
    g.setColour (redDeep);
    g.fillEllipse (circle (c, ringOuter));

    Path track;
    track.addCentredArc (c.x, c.y, ringMid, ringMid, 0.0f, arcStart, arcEnd, true);
    g.setColour (ink.withAlpha (0.5f));
    g.strokePath (track, PathStrokeType (ringW, PathStrokeType::curved, PathStrokeType::rounded));

    const float angle = arcStart + s.value * (arcEnd - arcStart);
    if (s.value > 0.001f)
    {
        Path arc;
        arc.addCentredArc (c.x, c.y, ringMid, ringMid, 0.0f, arcStart, angle, true);
        g.setColour (accent.withAlpha (0.35f));
        g.strokePath (arc, PathStrokeType (ringW * 2.2f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (accent);
        g.strokePath (arc, PathStrokeType (ringW, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // The hole, glowing a little more as the value rises.
    g.setColour (ink);
    g.fillEllipse (circle (c, r));
    g.setGradientFill (ColourGradient (accent.withAlpha (0.08f + 0.3f * s.value), c.x, c.y,
                                       accent.withAlpha (0.0f), c.x + r, c.y, true));
    g.fillEllipse (circle (c, r));

    // Pointer: a bright pupil riding the ring.
    const auto dot = c.getPointOnCircumference (ringMid, angle);
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.fillEllipse (circle (dot, ringW * 1.1f));
    g.setColour (cream);
    g.fillEllipse (circle (dot, ringW * 0.6f));

    auto text = circle (c, r * 0.8f);
    Lucha::drawOutlinedText (g, s.label, text.withSizeKeepingCentre (text.getWidth(), 16.0f).translated (0.0f, -13.0f),
                             Lucha::font (13.0f), s.active ? cream : gold, Justification::centred, 2.0f);
    Lucha::drawOutlinedText (g, s.valueText, text.withSizeKeepingCentre (text.getWidth(), 24.0f).translated (0.0f, 10.0f),
                             Lucha::font (21.0f), cream, Justification::centred, 2.0f);

    if (s.active)
    {
        Path ring;
        ring.addEllipse (circle (c, ringOuter + 1.0f));
        highlight (g, ring, true);
    }
}

void drawStud (Graphics& g, Point<float> c, float r, const MaskArt::FeatureState& s)
{
    g.setColour (ink);
    g.fillEllipse (circle (c, r + 2.0f));
    g.setGradientFill (ColourGradient (gold.brighter (0.4f), c.x - r, c.y - r, goldDark, c.x + r, c.y + r, false));
    g.fillEllipse (circle (c, r));

    const float arcR = r * 0.7f;
    Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, arcStart, arcEnd, true);
    g.setColour (ink.withAlpha (0.55f));
    g.strokePath (track, PathStrokeType (r * 0.28f, PathStrokeType::curved, PathStrokeType::rounded));

    const float angle = arcStart + s.value * (arcEnd - arcStart);
    if (s.value > 0.001f)
    {
        Path arc;
        arc.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, arcStart, angle, true);
        g.setColour (red);
        g.strokePath (arc, PathStrokeType (r * 0.28f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    g.setColour (ink);
    g.fillEllipse (circle (c, r * 0.32f));

    if (s.active)
    {
        Path ring;
        ring.addEllipse (circle (c, r + 1.0f));
        highlight (g, ring, true);
    }
}

// Little line drawings of each space for the medallion.
Path spaceIcon (int space, Point<float> c, float size)
{
    Path p;
    const float s = size;
    const float x = c.x - s * 0.5f, y = c.y - s * 0.5f;

    if (space == 0) // room: a house
    {
        p.startNewSubPath (x + s * 0.12f, y + s * 0.48f);
        p.lineTo (x + s * 0.5f, y + s * 0.12f);
        p.lineTo (x + s * 0.88f, y + s * 0.48f);
        p.addRectangle (x + s * 0.22f, y + s * 0.48f, s * 0.56f, s * 0.42f);
        p.addRectangle (x + s * 0.42f, y + s * 0.64f, s * 0.16f, s * 0.26f);
    }
    else if (space == 1) // hall: a pediment on columns
    {
        p.startNewSubPath (x + s * 0.06f, y + s * 0.34f);
        p.lineTo (x + s * 0.5f, y + s * 0.08f);
        p.lineTo (x + s * 0.94f, y + s * 0.34f);
        p.closeSubPath();
        for (int i = 0; i < 4; ++i)
            p.addRectangle (x + s * (0.16f + 0.2f * (float) i), y + s * 0.42f, s * 0.08f, s * 0.38f);
        p.addRectangle (x + s * 0.06f, y + s * 0.84f, s * 0.88f, s * 0.08f);
    }
    else // cathedral: a tall gothic window with a rose window in its point
    {
        p.startNewSubPath (x + s * 0.22f, y + s * 1.0f);
        p.lineTo (x + s * 0.22f, y + s * 0.5f);
        p.quadraticTo (x + s * 0.22f, y + s * 0.18f, x + s * 0.5f, y - s * 0.08f);
        p.quadraticTo (x + s * 0.78f, y + s * 0.18f, x + s * 0.78f, y + s * 0.5f);
        p.lineTo (x + s * 0.78f, y + s * 1.0f);
        p.addEllipse (x + s * 0.39f, y + s * 0.2f, s * 0.22f, s * 0.22f);
        p.startNewSubPath (x + s * 0.5f, y + s * 0.52f);
        p.lineTo (x + s * 0.5f, y + s * 1.0f);
        p.startNewSubPath (x + s * 0.1f, y + s * 1.0f);
        p.lineTo (x + s * 0.9f, y + s * 1.0f);
    }
    return p;
}

void drawMedallion (Graphics& g, Point<float> c, float r, const MaskArt::FeatureState& s)
{
    Path star;
    star.addStar (c, 5, r * 0.78f, r * 1.5f, 0.0f);
    fillGold (g, star, 3.0f, s.active ? 0.15f : 0.0f);

    g.setColour (ink);
    g.fillEllipse (circle (c, r + 2.5f));
    g.setGradientFill (ColourGradient (pink, c.x - r, c.y - r, red.darker (0.3f), c.x + r, c.y + r, false));
    g.fillEllipse (circle (c, r));
    g.setColour (gold);
    g.drawEllipse (circle (c, r - 3.0f), 1.5f);

    const auto icon = spaceIcon (s.step, c.translated (0.0f, -r * 0.2f), r * 0.75f);
    g.setColour (ink);
    g.strokePath (icon, PathStrokeType (4.0f, PathStrokeType::mitered, PathStrokeType::rounded));
    g.setColour (cream);
    g.strokePath (icon, PathStrokeType (1.8f, PathStrokeType::mitered, PathStrokeType::rounded));

    const auto label = Rectangle<float> (r * 1.8f, 14.0f).withCentre (c.translated (0.0f, r * 0.5f));
    Lucha::drawOutlinedText (g, s.valueText.toUpperCase(), label, Lucha::font (11.0f), cream, Justification::centred, 2.5f);

    if (s.active)
    {
        Path ring;
        ring.addEllipse (circle (c, r + 1.5f));
        highlight (g, ring, true);
    }
}

void drawMouth (Graphics& g, Rectangle<float> mask, const MaskArt::FeatureState& s)
{
    const bool frozen = s.value > 0.5f;
    fillGold (g, mapped (unitEllipse (0.335f, 0.75f, 0.33f, 0.17f), mask), 3.0f);
    outlineAndFill (g, mapped (unitEllipse (0.355f, 0.765f, 0.29f, 0.14f), mask), red, 2.0f);

    const auto hole = mapped (unitEllipse (0.378f, 0.782f, 0.244f, 0.106f), mask);
    const auto b = hole.getBounds();
    g.setColour (ink);
    g.fillPath (hole);

    {
        Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (hole);

        if (frozen)
        {
            g.setGradientFill (ColourGradient (ice.withAlpha (0.95f), b.getCentreX(), b.getCentreY(),
                                               ice.withAlpha (0.15f), b.getRight(), b.getCentreY(), true));
            g.fillPath (hole);
        }

        // Teeth along the top and bottom.
        const int numTeeth = 7;
        const float toothW = b.getWidth() * 0.62f / (float) numTeeth;
        const float left = b.getCentreX() - toothW * (float) numTeeth * 0.5f;
        g.setColour (frozen ? juce::Colours::white : cream.withAlpha (0.85f));
        for (int i = 0; i < numTeeth; ++i)
        {
            const float x = left + toothW * (float) i;
            Path top, bottom;
            top.addTriangle (x + 1.0f, b.getY(), x + toothW - 1.0f, b.getY(), x + toothW * 0.5f, b.getY() + b.getHeight() * 0.28f);
            bottom.addTriangle (x + 1.0f, b.getBottom(), x + toothW - 1.0f, b.getBottom(), x + toothW * 0.5f, b.getBottom() - b.getHeight() * 0.22f);
            g.fillPath (top);
            g.fillPath (bottom);
        }
    }

    g.setColour (frozen ? ink : (s.active ? ice : ice.withAlpha (0.7f)));
    g.setFont (Lucha::font (14.0f));
    g.drawText (frozen ? "FROZEN" : "FREEZE", b.translated (0.0f, 1.0f), Justification::centred);

    highlight (g, hole, s.active);
}
}

//==============================================================================
namespace MaskArt
{
Layout makeLayout (Rectangle<float> r)
{
    Layout l;
    l.bounds = r;
    l.head = mapped (unitHead(), r);

    l.eyeRadius = eyeRadiusU * r.getWidth();
    l.studRadius = studRadiusU * r.getWidth();
    l.medallionRadius = medallionRadiusU * r.getWidth();

    l.eyeCentre[0] = at (r, eyeU, eyeV);
    l.eyeCentre[1] = at (r, 1.0f - eyeU, eyeV);
    l.studCentre[0] = at (r, studU, studV);
    l.studCentre[1] = at (r, 1.0f - studU, studV);
    l.medallionCentre = at (r, 0.5f, medallionV);

    auto circlePath = [] (Point<float> c, float radius)
    {
        Path p;
        p.addEllipse (circle (c, radius));
        return p;
    };

    auto& h = l.hitAreas;
    h[(size_t) Feature::medallion] = circlePath (l.medallionCentre, l.medallionRadius * 1.25f);
    h[(size_t) Feature::eyeLeft] = circlePath (l.eyeCentre[0], l.eyeRadius * 1.25f);
    h[(size_t) Feature::eyeRight] = circlePath (l.eyeCentre[1], l.eyeRadius * 1.25f);
    h[(size_t) Feature::studLeft] = circlePath (l.studCentre[0], l.studRadius * 1.4f);
    h[(size_t) Feature::studRight] = circlePath (l.studCentre[1], l.studRadius * 1.4f);
    h[(size_t) Feature::flameLeft] = mapped (unitFlame(), r);
    h[(size_t) Feature::flameRight] = mapped (unitFlame(), r, true);
    h[(size_t) Feature::boltLeft] = mapped (unitBolt(), r);
    h[(size_t) Feature::boltRight] = mapped (unitBolt(), r, true);
    h[(size_t) Feature::mouth] = mapped (unitEllipse (0.355f, 0.765f, 0.29f, 0.14f), r);

    // The crest is narrow at the bottom; use its fill region, widened a little.
    Path crestHit;
    crestHit.addRectangle (at (r, 0.44f, crestFillTopV).x, at (r, 0.44f, crestFillTopV).y,
                           r.getWidth() * 0.12f, r.getHeight() * (crestFillBottomV - crestFillTopV));
    h[(size_t) Feature::crest] = crestHit;
    return l;
}

void drawBackground (Graphics& g, Rectangle<float> b)
{
    g.setGradientFill (ColourGradient (Colour (0xff22123f), b.getCentreX(), b.getY() + b.getHeight() * 0.4f,
                                       Colour (0xff05030a), b.getX(), b.getBottom(), true));
    g.fillRect (b);

    // Spotlight behind the mask.
    g.setGradientFill (ColourGradient (pink.withAlpha (0.22f), b.getCentreX(), b.getY() + b.getHeight() * 0.45f,
                                       pink.withAlpha (0.0f), b.getCentreX() + b.getWidth() * 0.48f, b.getY() + b.getHeight() * 0.45f, true));
    g.fillRect (b);

    // Ring ropes.
    const Colour ropes[] = { red, cream, maskBlue };
    for (int i = 0; i < 3; ++i)
    {
        const float y = b.getY() + b.getHeight() * (0.34f + 0.16f * (float) i);
        Path rope;
        rope.startNewSubPath (b.getX(), y);
        rope.quadraticTo (b.getCentreX(), y + 16.0f, b.getRight(), y);

        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.strokePath (rope, PathStrokeType (8.0f), AffineTransform::translation (0.0f, 4.0f));
        g.setColour (ropes[i].withAlpha (0.55f));
        g.strokePath (rope, PathStrokeType (7.0f));
        g.setColour (juce::Colours::white.withAlpha (0.18f));
        g.strokePath (rope, PathStrokeType (1.5f), AffineTransform::translation (0.0f, -1.5f));
    }
}

void drawMask (Graphics& g, const Layout& l, const State& state)
{
    const auto r = l.bounds;
    const auto& head = l.head;
    auto st = [&state] (Feature f) -> const FeatureState& { return state[(size_t) f]; };

    DropShadow (juce::Colours::black.withAlpha (0.7f), 30, { 0, 12 }).drawForPath (g, head);

    g.setGradientFill (ColourGradient (maskBlue.brighter (0.25f), at (r, 0.4f, 0.15f), maskDeep, at (r, 0.5f, 1.05f), false));
    g.fillPath (head);

    {
        Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (head);

        // Crest (MIX): liquid gold rising from the nose.
        const auto crest = mapped (unitCrest(), r);
        outlineAndFill (g, crest, redDeep, 3.0f);
        const float mixLevel = at (r, 0.5f, crestFillBottomV - st (Feature::crest).value * (crestFillBottomV - crestFillTopV)).y;
        fillLevel (g, crest, mixLevel, true, gold.brighter (0.5f), pink);
        stitch (g, crest, gold.withAlpha (0.8f), 1.3f);
        highlight (g, crest, st (Feature::crest).active);

        // Eyebrow flames: OCTAVE (left, three heights) and SIZE (right).
        for (int side = 0; side < 2; ++side)
        {
            const auto feature = side == 0 ? Feature::flameLeft : Feature::flameRight;
            const auto& s = st (feature);
            const auto flame = mapped (unitFlame(), r, side == 1);
            const float level = side == 0 ? (float) (s.step + 1) / 3.0f : s.value;

            outlineAndFill (g, flame, redDeep.darker (0.3f), 3.0f);
            fillLevel (g, flame, at (r, 0.0f, flameBaseV - level * (flameBaseV - flameTipV)).y, true,
                       Colour (0xffffe27a), Colour (0xffff4a1c));
            highlight (g, flame, s.active);
        }

        // Eye trims.
        for (bool m : { false, true })
            fillGold (g, mapped (unitEyeTrim(), r, m), 3.5f);

        // Cheek bolts: TONE (left, gold) and MOD (right, ice). They light up from the top.
        for (int side = 0; side < 2; ++side)
        {
            const auto feature = side == 0 ? Feature::boltLeft : Feature::boltRight;
            const auto& s = st (feature);
            const auto bolt = mapped (unitBolt(), r, side == 1);
            outlineAndFill (g, bolt, redDeep, 2.5f);
            fillLevel (g, bolt, at (r, 0.0f, boltTopV + s.value * (boltBottomV - boltTopV)).y, false,
                       side == 0 ? gold.brighter (0.3f) : ice, side == 0 ? red : maskBlue.brighter (0.4f));
            highlight (g, bolt, s.active);
        }

        drawMouth (g, r, st (Feature::mouth));

        // Light from the top-left, shade towards the edges.
        g.setGradientFill (ColourGradient (juce::Colours::white.withAlpha (0.14f), at (r, 0.33f, 0.12f),
                                           juce::Colours::white.withAlpha (0.0f), at (r, 0.75f, 0.45f), true));
        g.fillPath (head);
        g.setGradientFill (ColourGradient (juce::Colours::black.withAlpha (0.0f), at (r, 0.5f, 0.45f),
                                           juce::Colours::black.withAlpha (0.4f), at (r, 1.02f, 0.45f), true));
        g.fillPath (head);
    }

    // Features that should not be shaded.
    drawEye (g, l.eyeCentre[0], l.eyeRadius, st (Feature::eyeLeft), pink);
    drawEye (g, l.eyeCentre[1], l.eyeRadius, st (Feature::eyeRight), ice);
    drawStud (g, l.studCentre[0], l.studRadius, st (Feature::studLeft));
    drawStud (g, l.studCentre[1], l.studRadius, st (Feature::studRight));
    drawMedallion (g, l.medallionCentre, l.medallionRadius, st (Feature::medallion));

    // Small engraved labels for the features that don't carry their own text.
    drawLabel (g, r, 0.5f, 0.30f, "MIX", st (Feature::crest).valueText, st (Feature::crest).active);
    drawLabel (g, r, 0.335f, 0.215f, "OCTAVE", st (Feature::flameLeft).valueText, st (Feature::flameLeft).active);
    drawLabel (g, r, 0.665f, 0.215f, "SIZE", st (Feature::flameRight).valueText, st (Feature::flameRight).active);
    drawLabel (g, r, 0.36f, 0.745f, "TONE", st (Feature::boltLeft).valueText, st (Feature::boltLeft).active);
    drawLabel (g, r, 0.64f, 0.745f, "MOD", st (Feature::boltRight).valueText, st (Feature::boltRight).active);
    drawLabel (g, r, studU, studV - 0.062f, "PRE", {}, st (Feature::studLeft).active);
    drawLabel (g, r, 1.0f - studU, studV - 0.062f, "LOW CUT", {}, st (Feature::studRight).active);

    // Outline and stitching around the edge.
    g.setColour (ink);
    g.strokePath (head, PathStrokeType (3.5f));
    auto inner = unitHead();
    inner.applyTransform (AffineTransform::scale (0.965f, 0.975f, 0.5f, 0.5f));
    stitch (g, mapped (inner, r), gold.withAlpha (0.55f), 1.2f);
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
