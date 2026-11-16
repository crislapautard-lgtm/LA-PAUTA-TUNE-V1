#include "PluginProcessor.h"
#include "PluginEditor.h"

LaPautaTuneAudioProcessor::LaPautaTuneAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "LA_PAUTA_TUNE_STATE", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout
LaPautaTuneAudioProcessor::createParameterLayout()
{
    using APF = juce::AudioParameterFloat;
    using APChoice = juce::AudioParameterChoice;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::make_unique<APChoice>(
        juce::ParameterID{"key", 1}, "Key",
        juce::StringArray{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}, 0));
    layout.add(std::make_unique<APChoice>(
        juce::ParameterID{"scale", 1}, "Scale",
        juce::StringArray{"Chromatic", "Major", "Minor"}, 2));
    layout.add(std::make_unique<APF>(
        juce::ParameterID{"center", 1}, "Center",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 100.0f));
    layout.add(std::make_unique<APF>(
        juce::ParameterID{"variation", 1}, "Variation",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f));
    layout.add(std::make_unique<APF>(
        juce::ParameterID{"transpose", 1}, "Transpose",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.01f), 0.0f));
    layout.add(std::make_unique<APF>(
        juce::ParameterID{"retune", 1}, "Retune Speed",
        juce::NormalisableRange<float>(1.0f, 500.0f, 0.1f, 0.35f), 35.0f));
    layout.add(std::make_unique<APF>(
        juce::ParameterID{"humanize", 1}, "Humanize",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 25.0f));
    layout.add(std::make_unique<APF>(
        juce::ParameterID{"output", 1}, "Output",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.01f), 0.0f));
    return layout;
}

void LaPautaTuneAudioProcessor::prepareToPlay(double, int) {}
void LaPautaTuneAudioProcessor::releaseResources() {}

bool LaPautaTuneAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& in = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    return in == out && (out == juce::AudioChannelSet::mono()
                      || out == juce::AudioChannelSet::stereo());
}

void LaPautaTuneAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int inputChannels = getTotalNumInputChannels();
    const int outputChannels = getTotalNumOutputChannels();

    for (int channel = inputChannels; channel < outputChannels; ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    const float outputDb = parameters.getRawParameterValue("output")->load();
    const float gain = juce::Decibels::decibelsToGain(outputDb);
    buffer.applyGain(gain);

    // This first build is the verified project foundation: audio passes through
    // with output gain. The pitch-detection/editor engine is not implemented yet.
}

juce::AudioProcessorEditor* LaPautaTuneAudioProcessor::createEditor()
{
    return new LaPautaTuneAudioProcessorEditor(*this);
}

void LaPautaTuneAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void LaPautaTuneAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName(parameters.state.getType()))
        parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LaPautaTuneAudioProcessor();
}
