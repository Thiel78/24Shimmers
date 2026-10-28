// Development tool: renders the plugin editor to PNG files so the UI can be
// checked without a DAW.  Usage: UISnapshot <output-dir>

#include <juce_audio_utils/juce_audio_utils.h>

#include "../Source/PluginEditor.h"
#include "../Source/PluginProcessor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;

    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();

    ShimmerProcessor processor;
    std::unique_ptr<ShimmerEditor> editor (dynamic_cast<ShimmerEditor*> (processor.createEditor()));
    editor->setVisible (true); // hosts show the editor; hit-testing ignores hidden components
    editor->spawnSparkles (90, 0.9f);

    auto save = [&] (const juce::String& name)
    {
        auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
        auto file = outDir.getChildFile (name);
        file.deleteFile();
        juce::FileOutputStream out (file);
        juce::PNGImageFormat().writeImageToStream (image, out);
        std::printf ("wrote %s\n", file.getFullPathName().toRawUTF8());
    };

    save ("ui-default.png");

    // Every feature should be reachable: sample points inside each hit area and
    // count how many land on the right control rather than a neighbour.
    int problems = 0;
    for (size_t i = 0; i < MaskArt::numFeatures; ++i)
    {
        const auto& area = editor->getLayout().hitAreas[i];
        const auto b = area.getBounds();
        int inside = 0, mine = 0;
        for (float y = b.getY(); y < b.getBottom(); y += 2.0f)
            for (float x = b.getX(); x < b.getRight(); x += 2.0f)
                if (area.contains (x, y))
                {
                    ++inside;
                    mine += editor->getComponentAt (juce::Point<float> (x, y).toInt()) == editor->getControl ((MaskArt::Feature) i) ? 1 : 0;
                }
        const float share = inside > 0 ? (float) mine / (float) inside : 0.0f;
        std::printf ("feature %2d: %3.0f%% of its area is clickable\n", (int) i, share * 100.0f);
        problems += share < 0.6f ? 1 : 0;
    }

    // Simulate gestures: drag the shimmer eye up, click the medallion and the mouth.
    auto event = [&] (juce::Component* c, juce::Point<float> pos, juce::Point<float> downPos, bool dragged, juce::ModifierKeys mods = {})
    {
        const auto now = juce::Time::getCurrentTime();
        return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), pos, mods, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                 c, c, now, downPos, now, 1, dragged);
    };
    auto centreOf = [&] (MaskArt::Feature f)
    {
        auto* c = editor->getControl (f);
        return c->getLocalBounds().getCentre().toFloat();
    };

    auto* eye = editor->getControl (MaskArt::Feature::eyeLeft);
    const auto p0 = centreOf (MaskArt::Feature::eyeLeft);
    const float before = processor.state.getParameter (ParamIDs::shimmer)->getValue();
    eye->mouseDown (event (eye, p0, p0, false));
    eye->mouseDrag (event (eye, p0.translated (0.0f, -60.0f), p0, true));
    eye->mouseUp (event (eye, p0.translated (0.0f, -60.0f), p0, true));
    const float after = processor.state.getParameter (ParamIDs::shimmer)->getValue();
    std::printf ("drag shimmer eye up 60 px: %.2f -> %.2f\n", before, after);
    problems += std::abs (after - before - 0.25f) < 0.01f ? 0 : 1;

    auto click = [&] (MaskArt::Feature f)
    {
        auto* c = editor->getControl (f);
        const auto p = centreOf (f);
        c->mouseDown (event (c, p, p, false));
        c->mouseUp (event (c, p, p, false));
    };

    click (MaskArt::Feature::medallion);
    const auto space = processor.state.getParameter (ParamIDs::space)->getCurrentValueAsText();
    click (MaskArt::Feature::mouth);
    const bool frozen = processor.state.getParameter (ParamIDs::freeze)->getValue() > 0.5f;
    click (MaskArt::Feature::mouth);
    const bool unfrozen = processor.state.getParameter (ParamIDs::freeze)->getValue() < 0.5f;
    std::printf ("click medallion: Hall -> %s, click mouth: frozen %d, again: unfrozen %d\n", space.toRawUTF8(), frozen, unfrozen);
    problems += (space == "Cathedral" && frozen && unfrozen) ? 0 : 1;

    auto set = [&] (const char* id, float normalised) { processor.state.getParameter (id)->setValueNotifyingHost (normalised); };
    set (ParamIDs::space, 1.0f);
    set (ParamIDs::octave, 1.0f);
    set (ParamIDs::shimmer, 0.85f);
    set (ParamIDs::mix, 0.7f);
    set (ParamIDs::size, 0.9f);
    set (ParamIDs::tone, 0.3f);
    set (ParamIDs::mod, 0.8f);
    set (ParamIDs::freeze, 1.0f);
    editor->spawnSparkles (60, 0.9f);
    save ("ui-cathedral-frozen.png");

    set (ParamIDs::space, 0.0f);
    set (ParamIDs::octave, 0.0f);
    set (ParamIDs::freeze, 0.0f);
    set (ParamIDs::mix, 0.1f);
    save ("ui-room.png");

    editor = nullptr;
    return problems == 0 ? 0 : 1;
}
