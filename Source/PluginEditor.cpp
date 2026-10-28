#include "PluginEditor.h"
#include "LuchaStyle.h"

using namespace juce;
using namespace Lucha::Colours;
using MaskArt::Feature;

//==============================================================================
MaskControl::MaskControl (RangedAudioParameter& p, String labelText, Kind k, std::function<void()> changed)
    : param (p), label (std::move (labelText)), kind (k), onChange (std::move (changed)),
      attachment (p, [this] (float) { if (onChange) onChange(); })
{
    setMouseCursor (kind == Kind::drag ? MouseCursor::UpDownResizeCursor : MouseCursor::PointingHandCursor);
    setRepaintsOnMouseActivity (false);
    attachment.sendInitialUpdate();
}

void MaskControl::setHitArea (const Path& areaInParent)
{
    const auto bounds = areaInParent.getBounds().getSmallestIntegerContainer().expanded (2);
    setBounds (bounds);
    hitArea = areaInParent;
    hitArea.applyTransform (AffineTransform::translation ((float) -bounds.getX(), (float) -bounds.getY()));
}

int MaskControl::getStep() const
{
    return roundToInt (param.convertFrom0to1 (param.getValue()));
}

bool MaskControl::hitTest (int x, int y)
{
    return hitArea.contains ((float) x, (float) y);
}

void MaskControl::mouseEnter (const MouseEvent&) { if (onChange) onChange(); }
void MaskControl::mouseExit (const MouseEvent&)  { if (onChange) onChange(); }

void MaskControl::mouseDown (const MouseEvent&)
{
    dragging = false;
    dragStartValue = param.getValue();
}

void MaskControl::mouseDrag (const MouseEvent& e)
{
    if (kind != Kind::drag || e.mouseWasClicked())
        return;

    if (! dragging)
    {
        dragging = true;
        attachment.beginGesture();
    }

    const float range = e.mods.isShiftDown() ? 1200.0f : 240.0f;
    const float value = jlimit (0.0f, 1.0f, dragStartValue - (float) e.getDistanceFromDragStartY() / range);
    attachment.setValueAsPartOfGesture (param.convertFrom0to1 (value));
}

void MaskControl::mouseUp (const MouseEvent& e)
{
    if (dragging)
    {
        attachment.endGesture();
        dragging = false;
    }
    else if (e.mouseWasClicked() && e.getNumberOfClicks() == 1)
    {
        if (kind == Kind::toggle)
        {
            attachment.setValueAsCompleteGesture (param.getValue() > 0.5f ? 0.0f : 1.0f);
        }
        else if (kind == Kind::cycle)
        {
            const int numSteps = param.getNumSteps();
            const int next = (getStep() + (e.mods.isShiftDown() ? numSteps - 1 : 1)) % numSteps;
            attachment.setValueAsCompleteGesture ((float) next);
        }
    }

    if (onChange)
        onChange();
}

void MaskControl::mouseDoubleClick (const MouseEvent&)
{
    if (kind == Kind::drag)
        attachment.setValueAsCompleteGesture (param.convertFrom0to1 (param.getDefaultValue()));
}

void MaskControl::mouseWheelMove (const MouseEvent&, const MouseWheelDetails& wheel)
{
    if (kind != Kind::drag)
        return;

    const float delta = (wheel.isReversed ? -wheel.deltaY : wheel.deltaY) * 0.1f;
    attachment.setValueAsCompleteGesture (param.convertFrom0to1 (jlimit (0.0f, 1.0f, param.getValue() + delta)));
}

//==============================================================================
ShimmerEditor::ShimmerEditor (ShimmerProcessor& p)
    : AudioProcessorEditor (p), processor (p)
{
    struct Spec { const char* paramID; const char* label; MaskControl::Kind kind; };

    // Same order as MaskArt::Feature.
    const Spec specs[] {
        { ParamIDs::space,    "SPACE",     MaskControl::Kind::cycle },
        { ParamIDs::mix,      "MIX",       MaskControl::Kind::drag },
        { ParamIDs::shimmer,  "SHIMMER",   MaskControl::Kind::drag },
        { ParamIDs::decay,    "DECAY",     MaskControl::Kind::drag },
        { ParamIDs::octave,   "OCTAVE",    MaskControl::Kind::cycle },
        { ParamIDs::size,     "SIZE",      MaskControl::Kind::drag },
        { ParamIDs::tone,     "TONE",      MaskControl::Kind::drag },
        { ParamIDs::mod,      "MOD",       MaskControl::Kind::drag },
        { ParamIDs::preDelay, "PRE-DELAY", MaskControl::Kind::drag },
        { ParamIDs::lowCut,   "LOW CUT",   MaskControl::Kind::drag },
        { ParamIDs::freeze,   "FREEZE",    MaskControl::Kind::toggle },
    };
    static_assert (std::size (specs) == MaskArt::numFeatures);

    for (const auto& s : specs)
    {
        auto c = std::make_unique<MaskControl> (*p.state.getParameter (s.paramID), s.label, s.kind, [this] { repaint(); });
        addAndMakeVisible (*c);
        controls.push_back (std::move (c));
    }

    setSize (680, 712);
    startTimerHz (30);
}

void ShimmerEditor::resized()
{
    layout = MaskArt::makeLayout (maskArea);

    for (size_t i = 0; i < MaskArt::numFeatures; ++i)
        controls[i]->setHitArea (layout.hitAreas[i]);
}

void ShimmerEditor::paint (Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    MaskArt::drawBackground (g, bounds);

    const Rectangle<float> titleArea (22.0f, 10.0f, 320.0f, 44.0f);
    Lucha::drawOutlinedText (g, "24 SHIMMERS", titleArea.translated (3.0f, 3.0f), Lucha::font (32.0f), pink, Justification::centredLeft);
    Lucha::drawOutlinedText (g, "24 SHIMMERS", titleArea, Lucha::font (32.0f), gold, Justification::centredLeft);

    g.setColour (cream.withAlpha (0.7f));
    g.setFont (Lucha::font (12.0f));
    g.drawText ("LUCHA LIBRE SHIMMER REVERB", Rectangle<float> (360.0f, 16.0f, 298.0f, 32.0f), Justification::centredRight);

    MaskArt::State state;
    for (size_t i = 0; i < MaskArt::numFeatures; ++i)
    {
        const auto& c = *controls[i];
        state[i] = { c.getNormalisedValue(), c.getStep(), c.getLabel(), c.getValueText(), c.isActive() };
    }
    MaskArt::drawMask (g, layout, state);

    for (const auto& s : sparkles)
        MaskArt::drawSparkle (g, s.pos, s.size, s.colour, std::sin (MathConstants<float>::pi * s.age / s.life));

    drawBanner (g);
}

// The ring announcer: names whatever you're touching, otherwise sums up the patch.
void ShimmerEditor::drawBanner (Graphics& g)
{
    const auto b = bannerArea;
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (b.translated (0.0f, 3.0f), 12.0f);
    g.setColour (ink.withAlpha (0.92f));
    g.fillRoundedRectangle (b, 12.0f);
    g.setColour (gold.withAlpha (0.8f));
    g.drawRoundedRectangle (b.reduced (1.0f), 12.0f, 2.0f);

    const MaskControl* active = nullptr;
    for (auto& c : controls)
        if (c->isActive())
            active = c.get();

    auto text = b.reduced (18.0f, 6.0f);

    if (active != nullptr)
    {
        auto valueText = active->getValueText();
        if (active == &control (Feature::mouth))
            valueText = active->getNormalisedValue() > 0.5f ? "ON" : "OFF";

        g.setFont (Lucha::font (24.0f));
        g.setColour (gold);
        g.drawText (active->getLabel(), text.removeFromLeft (text.getWidth() * 0.5f).withTrimmedRight (10.0f), Justification::centredRight);
        g.setColour (cream);
        g.drawText (valueText.toUpperCase(), text.withTrimmedLeft (10.0f), Justification::centredLeft);
        return;
    }

    const auto dot = "   " + String::fromUTF8 ("\xc2\xb7") + "   ";
    const auto summary = control (Feature::medallion).getValueText().toUpperCase()
                       + dot + control (Feature::eyeRight).getValueText()
                       + dot + "SHIMMER " + control (Feature::eyeLeft).getValueText()
                       + "  " + control (Feature::flameLeft).getValueText().toUpperCase()
                       + dot + "MIX " + control (Feature::crest).getValueText();

    g.setColour (cream);
    g.setFont (Lucha::font (17.0f));
    g.drawText (summary, text.removeFromTop (text.getHeight() * 0.58f), Justification::centredBottom);
    g.setColour (cream.withAlpha (0.5f));
    g.setFont (Lucha::font (11.0f, false));
    g.drawText ("drag the eyes, crest, flames, bolts and studs" + dot + "click the medallion, octave flame and mouth" + dot + "double-click resets",
                text, Justification::centredTop);
}

void ShimmerEditor::spawnSparkles (int count, float startAgeRange)
{
    const bool frozen = control (Feature::mouth).getNormalisedValue() > 0.5f;
    const Colour palette[] = { gold, cream, pink, ice };
    const auto c = maskArea.getCentre();

    for (int i = 0; i < count && sparkles.size() < 160; ++i)
    {
        const float angle = random.nextFloat() * MathConstants<float>::twoPi;
        const float spread = 0.9f + 0.28f * random.nextFloat();
        const Point<float> dir (std::cos (angle), std::sin (angle));

        Sparkle s;
        s.pos = { c.x + dir.x * maskArea.getWidth() * 0.5f * spread, c.y + dir.y * maskArea.getHeight() * 0.5f * spread };
        s.velocity = dir * 0.35f + Point<float> (0.0f, frozen ? -0.05f : -0.45f);
        s.size = 3.0f + 7.0f * random.nextFloat() * random.nextFloat();
        s.life = (frozen ? 1.6f : 0.6f) + 0.8f * random.nextFloat();
        s.age = s.life * startAgeRange * random.nextFloat();
        s.colour = frozen ? (random.nextBool() ? ice : cream) : palette[random.nextInt (4)];
        sparkles.push_back (s);
    }
}

void ShimmerEditor::timerCallback()
{
    const float dt = 1.0f / 30.0f;
    level = jmax (jmin (1.0f, processor.takeOutputPeak()), level * 0.9f);

    const float shimmer = control (Feature::eyeLeft).getNormalisedValue();
    const bool frozen = control (Feature::mouth).getNormalisedValue() > 0.5f;

    spawnBudget += frozen ? 0.35f : shimmer * (0.15f + 5.0f * level);
    const int count = (int) spawnBudget;
    spawnBudget -= (float) count;

    const bool hadSparkles = ! sparkles.empty();
    spawnSparkles (count);

    for (auto& s : sparkles)
    {
        s.age += dt;
        s.pos += s.velocity;
    }

    sparkles.erase (std::remove_if (sparkles.begin(), sparkles.end(), [] (const Sparkle& s) { return s.age >= s.life; }),
                    sparkles.end());

    if (hadSparkles || ! sparkles.empty())
        repaint (maskArea.expanded (70.0f, 30.0f).toNearestInt());
}
