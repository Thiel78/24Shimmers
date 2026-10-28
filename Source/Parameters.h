#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace ParamIDs
{
inline constexpr const char* mix       = "mix";
inline constexpr const char* decay     = "decay";
inline constexpr const char* size      = "size";
inline constexpr const char* preDelay  = "predelay";
inline constexpr const char* shimmer   = "shimmer";
inline constexpr const char* space     = "space";
inline constexpr const char* octave    = "octave";
inline constexpr const char* tone      = "tone";
inline constexpr const char* lowCut    = "lowcut";
inline constexpr const char* mod       = "mod";
inline constexpr const char* freeze    = "freeze";
}

inline const juce::StringArray spaceNames { "Room", "Hall", "Cathedral" };
inline const juce::StringArray octaveNames { "-1 oct", "+1 oct", "+2 oct" };
inline constexpr int octaveValues[] { -1, 1, 2 };

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;

    auto percent = AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; })
        .withValueFromStringFunction ([] (const String& s) { return s.getFloatValue() / 100.0f; });

    auto seconds = AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return v < 10.0f ? String (v, 1) + " s" : String (roundToInt (v)) + " s"; });

    auto millis = AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return String (roundToInt (v)) + " ms"; });

    auto hertz = AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return String (roundToInt (v)) + " Hz"; });

    auto skewed = [] (float lo, float hi, float centre)
    {
        NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    };

    return {
        std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::space, 1 }, "Space", spaceNames, 1),
        std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::mix, 1 }, "Mix", NormalisableRange<float> (0.0f, 1.0f), 0.35f, percent),
        std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::decay, 1 }, "Decay", skewed (0.3f, 60.0f, 5.0f), 6.0f, seconds),
        std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::size, 1 }, "Size", NormalisableRange<float> (0.0f, 1.0f), 0.5f, percent),
        std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::preDelay, 1 }, "Pre-Delay", skewed (0.0f, 500.0f, 80.0f), 20.0f, millis),
        std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::shimmer, 1 }, "Shimmer", NormalisableRange<float> (0.0f, 1.0f), 0.5f, percent),
        std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::octave, 1 }, "Octave", octaveNames, 1),
        std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::tone, 1 }, "Tone", NormalisableRange<float> (0.0f, 1.0f), 0.6f, percent),
        std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::lowCut, 1 }, "Low Cut", skewed (20.0f, 1000.0f, 150.0f), 120.0f, hertz),
        std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::mod, 1 }, "Modulation", NormalisableRange<float> (0.0f, 1.0f), 0.4f, percent),
        std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::freeze, 1 }, "Freeze", false),
    };
}
