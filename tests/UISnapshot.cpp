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

    processor.state.getParameter (ParamIDs::freeze)->setValueNotifyingHost (1.0f);
    processor.state.getParameter (ParamIDs::shimmer)->setValueNotifyingHost (0.9f);
    processor.state.getParameter (ParamIDs::mix)->setValueNotifyingHost (0.8f);
    editor->spawnSparkles (60, 0.9f);
    save ("ui-freeze.png");

    editor = nullptr;
    return 0;
}
