#include "PluginEditor.h"
#include "MaskArt.h"

using namespace juce;
using namespace Lucha::Colours;

//==============================================================================
LuchaKnob::LuchaKnob (AudioProcessorValueTreeState& state, const String& paramID, String labelText, bool isOnMask)
    : param (*state.getParameter (paramID)), label (std::move (labelText)), onMask (isOnMask)
{
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    slider.getProperties().set ("lucha_big", onMask);
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);

    attachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (state, paramID, slider);
    slider.setDoubleClickReturnValue (true, param.convertFrom0to1 (param.getDefaultValue()));
}

void LuchaKnob::resized()
{
    auto b = getLocalBounds();
    const int knob = jmin (b.getWidth(), b.getHeight() - (onMask ? 28 : 36));
    slider.setBounds (b.removeFromTop (knob).withSizeKeepingCentre (knob, knob));
}

void LuchaKnob::paint (Graphics& g)
{
    auto text = getLocalBounds().toFloat().withTop ((float) slider.getBottom() + 2.0f);
    const auto value = param.getCurrentValueAsText();

    if (onMask)
    {
        const auto line = label + "  " + value;
        const auto f = Lucha::font (13.0f);
        const float w = GlyphArrangement::getStringWidth (f, line) + 22.0f;
        auto tag = text.withHeight (22.0f).withSizeKeepingCentre (w, 22.0f);

        g.setColour (ink.withAlpha (0.9f));
        g.fillRoundedRectangle (tag, 11.0f);
        g.setColour (gold);
        g.drawRoundedRectangle (tag.reduced (0.75f), 11.0f, 1.5f);

        g.setFont (f);
        g.setColour (gold);
        g.drawText (label, tag.reduced (11.0f, 0.0f), Justification::centredLeft);
        g.setColour (cream);
        g.drawText (value, tag.reduced (11.0f, 0.0f), Justification::centredRight);
    }
    else
    {
        Lucha::drawOutlinedText (g, label.toUpperCase(), text.removeFromTop (18.0f), Lucha::font (14.0f), gold);
        g.setColour (cream.withAlpha (0.85f));
        g.setFont (Lucha::font (12.5f, false));
        g.drawText (value, text.removeFromTop (16.0f), Justification::centredTop);
    }
}

//==============================================================================
FreezeButton::FreezeButton() : Button ("Freeze")
{
    setClickingTogglesState (true);
    setTooltip ("Freeze: hold the tail forever");
}

void FreezeButton::paintButton (Graphics& g, bool highlighted, bool)
{
    const auto b = getLocalBounds().toFloat().reduced (2.0f);
    const bool on = getToggleState();

    g.setColour (ink);
    g.fillEllipse (b);

    if (on)
    {
        g.setGradientFill (ColourGradient (ice.withAlpha (0.95f), b.getCentreX(), b.getCentreY(),
                                           ice.withAlpha (0.1f), b.getRight(), b.getCentreY(), true));
        g.fillEllipse (b);
    }

    // Teeth.
    g.setColour (cream.withAlpha (on ? 0.9f : 0.75f));
    const int numTeeth = 6;
    const float toothW = b.getWidth() * 0.5f / numTeeth;
    for (int i = 0; i < numTeeth; ++i)
    {
        const float x = b.getCentreX() - toothW * numTeeth * 0.5f + toothW * (float) i;
        Path tooth;
        tooth.addTriangle (x + 1.0f, b.getY() + 3.0f, x + toothW - 1.0f, b.getY() + 3.0f, x + toothW * 0.5f, b.getY() + b.getHeight() * 0.26f);
        g.fillPath (tooth);
    }

    const auto colour = on ? ink : (highlighted ? ice : ice.withAlpha (0.65f));
    g.setColour (colour);
    g.setFont (Lucha::font (13.0f));
    g.drawText ("FREEZE", b.withTrimmedTop (b.getHeight() * 0.25f), Justification::centred);
}

//==============================================================================
ShimmerEditor::ShimmerEditor (ShimmerProcessor& p)
    : AudioProcessorEditor (p),
      processor (p),
      shimmerKnob (p.state, ParamIDs::shimmer, "SHIMMER", true),
      decayKnob (p.state, ParamIDs::decay, "DECAY", true),
      sizeKnob (p.state, ParamIDs::size, "Size", false),
      preDelayKnob (p.state, ParamIDs::preDelay, "Pre-Delay", false),
      toneKnob (p.state, ParamIDs::tone, "Tone", false),
      lowCutKnob (p.state, ParamIDs::lowCut, "Low Cut", false),
      shift1Knob (p.state, ParamIDs::shift1, "Shift 1", false),
      shift2Knob (p.state, ParamIDs::shift2, "Shift 2", false),
      blendKnob (p.state, ParamIDs::blend, "Blend", false),
      modKnob (p.state, ParamIDs::mod, "Mod", false)
{
    setLookAndFeel (&lookAndFeel);

    for (auto* k : { &shimmerKnob, &decayKnob, &sizeKnob, &preDelayKnob, &toneKnob, &lowCutKnob,
                     &shift1Knob, &shift2Knob, &blendKnob, &modKnob })
        addAndMakeVisible (k);

    mixSlider.setSliderStyle (Slider::LinearHorizontal);
    mixSlider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible (mixSlider);
    mixAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (p.state, ParamIDs::mix, mixSlider);
    mixSlider.setDoubleClickReturnValue (true, 0.35);

    addAndMakeVisible (freezeButton);
    freezeAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (p.state, ParamIDs::freeze, freezeButton);

    setSize (880, 600);
    startTimerHz (30);
}

ShimmerEditor::~ShimmerEditor()
{
    setLookAndFeel (nullptr);
}

void ShimmerEditor::resized()
{
    for (auto [knob, left] : { std::pair { &shimmerKnob, true }, std::pair { &decayKnob, false } })
    {
        const auto c = MaskArt::eyeCentre (maskArea, left);
        knob->setBounds (Rectangle<int> (160, 126).withCentre ({ (int) c.x, (int) c.y + 15 }));
    }

    freezeButton.setBounds (MaskArt::mouthBounds (maskArea).toNearestInt());

    auto layoutPanel = [] (Rectangle<float> panel, std::initializer_list<LuchaKnob*> knobs)
    {
        auto area = panel.reduced (8.0f).withTrimmedTop (48.0f).toNearestInt();
        const int cellW = area.getWidth() / 2, cellH = area.getHeight() / 2;
        int i = 0;
        for (auto* k : knobs)
        {
            const Rectangle<int> cell (area.getX() + (i % 2) * cellW, area.getY() + (i / 2) * cellH, cellW, cellH);
            k->setBounds (cell.withSizeKeepingCentre (cellW - 6, 128));
            ++i;
        }
    };

    layoutPanel (leftPanel, { &sizeKnob, &preDelayKnob, &toneKnob, &lowCutKnob });
    layoutPanel (rightPanel, { &shift1Knob, &shift2Knob, &blendKnob, &modKnob });

    mixSlider.setBounds (beltArea.reduced (58, 0));
}

void ShimmerEditor::drawPanel (Graphics& g, Rectangle<float> area, const String& title, const String& subtitle)
{
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (area, 14.0f);
    g.setColour (gold.withAlpha (0.55f));
    g.drawRoundedRectangle (area.reduced (1.0f), 14.0f, 2.0f);

    auto header = area.reduced (10.0f, 8.0f).removeFromTop (44.0f);
    Lucha::drawOutlinedText (g, title, header.removeFromTop (26.0f), Lucha::font (21.0f), gold);
    g.setColour (cream.withAlpha (0.6f));
    g.setFont (Lucha::font (11.0f, false));
    g.drawText (subtitle, header, Justification::centredTop);
}

void ShimmerEditor::paint (Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    MaskArt::drawBackground (g, bounds);

    // Title.
    const Rectangle<float> titleArea (24.0f, 12.0f, 360.0f, 48.0f);
    Lucha::drawOutlinedText (g, "24 SHIMMERS", titleArea.translated (3.0f, 3.0f), Lucha::font (36.0f), pink, Justification::centredLeft);
    Lucha::drawOutlinedText (g, "24 SHIMMERS", titleArea, Lucha::font (36.0f), gold, Justification::centredLeft);

    g.setColour (cream.withAlpha (0.7f));
    g.setFont (Lucha::font (13.0f));
    g.drawText ("LUCHA LIBRE SHIMMER REVERB", Rectangle<float> (520.0f, 20.0f, 336.0f, 34.0f), Justification::centredRight);

    drawPanel (g, leftPanel, "EL RING", "THE ROOM");
    drawPanel (g, rightPanel, "EL CIELO", "THE SHIMMER");

    MaskArt::drawMask (g, maskArea);

    for (const auto& s : sparkles)
    {
        const float alpha = std::sin (MathConstants<float>::pi * s.age / s.life);
        MaskArt::drawSparkle (g, s.pos, s.size, s.colour, alpha);
    }

    // Belt ends.
    const auto belt = beltArea.toFloat();
    Lucha::drawOutlinedText (g, "DRY", belt.withWidth (52.0f), Lucha::font (16.0f), cream);
    Lucha::drawOutlinedText (g, "WET", belt.withTrimmedLeft (belt.getWidth() - 52.0f), Lucha::font (16.0f), cream);
}

void ShimmerEditor::spawnSparkles (int count, float startAgeRange)
{
    const bool frozen = processor.state.getRawParameterValue (ParamIDs::freeze)->load() > 0.5f;
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

    const auto& st = processor.state;
    const float shimmer = st.getRawParameterValue (ParamIDs::shimmer)->load();
    const bool frozen = st.getRawParameterValue (ParamIDs::freeze)->load() > 0.5f;

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
        repaint (maskArea.expanded (80.0f, 40.0f).toNearestInt());
}
