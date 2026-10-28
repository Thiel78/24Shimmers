#include "PluginProcessor.h"
#include "PluginEditor.h"

ShimmerProcessor::ShimmerProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "24Shimmers", createParameterLayout())
{
    mix = state.getRawParameterValue (ParamIDs::mix);
    decay = state.getRawParameterValue (ParamIDs::decay);
    size = state.getRawParameterValue (ParamIDs::size);
    preDelay = state.getRawParameterValue (ParamIDs::preDelay);
    shimmerAmount = state.getRawParameterValue (ParamIDs::shimmer);
    shift1 = state.getRawParameterValue (ParamIDs::shift1);
    shift2 = state.getRawParameterValue (ParamIDs::shift2);
    blend = state.getRawParameterValue (ParamIDs::blend);
    tone = state.getRawParameterValue (ParamIDs::tone);
    lowCut = state.getRawParameterValue (ParamIDs::lowCut);
    mod = state.getRawParameterValue (ParamIDs::mod);
    freeze = state.getRawParameterValue (ParamIDs::freeze);
}

shimmer::Parameters ShimmerProcessor::readParameters() const
{
    auto interval = [] (const std::atomic<float>* choice)
    {
        const int index = juce::jlimit (0, (int) std::size (shiftSemitones) - 1, (int) choice->load());
        return shiftSemitones[index];
    };

    shimmer::Parameters p;
    p.mix = mix->load();
    p.decaySeconds = decay->load();
    p.size = size->load();
    p.preDelayMs = preDelay->load();
    p.shimmer = shimmerAmount->load();
    p.shift1 = interval (shift1);
    p.shift2 = interval (shift2);
    p.blend = blend->load();
    p.tone = tone->load();
    p.lowCutHz = lowCut->load();
    p.modulation = mod->load();
    p.freeze = freeze->load() > 0.5f;
    return p;
}

void ShimmerProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    engine.setParameters (readParameters());
}

void ShimmerProcessor::reset()
{
    engine.reset();
}

bool ShimmerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

void ShimmerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();

    if (getTotalNumInputChannels() == 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);

    engine.setParameters (readParameters());
    engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), numSamples);

    const float peak = buffer.getMagnitude (0, numSamples);
    if (peak > outputPeak.load())
        outputPeak.store (peak);
}

double ShimmerProcessor::getTailLengthSeconds() const
{
    return (double) decay->load() + 0.5;
}

juce::AudioProcessorEditor* ShimmerProcessor::createEditor()
{
    return new ShimmerEditor (*this);
}

void ShimmerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void ShimmerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
            state.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ShimmerProcessor();
}
