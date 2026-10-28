#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Parameters.h"
#include "ShimmerEngine.h"

class ShimmerProcessor : public juce::AudioProcessor
{
public:
    ShimmerProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "24 Shimmers"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState state;

    // Output peak since the editor last asked, used to drive the sparkles.
    float takeOutputPeak() { return outputPeak.exchange (0.0f); }

private:
    shimmer::Parameters readParameters() const;

    shimmer::Engine engine;
    std::atomic<float> outputPeak { 0.0f };

    std::atomic<float>* mix = nullptr;
    std::atomic<float>* decay = nullptr;
    std::atomic<float>* size = nullptr;
    std::atomic<float>* preDelay = nullptr;
    std::atomic<float>* shimmerAmount = nullptr;
    std::atomic<float>* shift1 = nullptr;
    std::atomic<float>* shift2 = nullptr;
    std::atomic<float>* blend = nullptr;
    std::atomic<float>* tone = nullptr;
    std::atomic<float>* lowCut = nullptr;
    std::atomic<float>* mod = nullptr;
    std::atomic<float>* freeze = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ShimmerProcessor)
};
