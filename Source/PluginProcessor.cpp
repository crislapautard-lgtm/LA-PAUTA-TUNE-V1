#include "PluginProcessor.h"
#include "PluginEditor.h"

LaPautaTuneProcessor::LaPautaTuneProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
}

LaPautaTuneProcessor::~LaPautaTuneProcessor()
{
}

const juce::String LaPautaTuneProcessor::getName() const
{
    return JucePlugin_Name;
}

bool LaPautaTuneProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool LaPautaTuneProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool LaPautaTuneProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double LaPautaTuneProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int LaPautaTuneProcessor::getNumPrograms()
{
    return 1;
}

int LaPautaTuneProcessor::getCurrentProgram()
{
    return 0;
}

void LaPautaTuneProcessor::setCurrentProgram (int index)
{
}

const juce::String LaPautaTuneProcessor::getProgramName (int index)
{
    return {};
}

void LaPautaTuneProcessor::changeProgramName (int index, const juce::String& newName)
{
}

void LaPautaTuneProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    hostSampleRate = sampleRate;
    srcSampleRate = sampleRate;

    const juce::ScopedLock sl (bufLock);
    playBuf.setSize (2, samplesPerBlock);
    playBuf.clear();
}

void LaPautaTuneProcessor::releaseResources()
{
}

bool LaPautaTuneProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}

void LaPautaTuneProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    {
        const juce::ScopedLock sl (bufLock);
        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            auto* channelData = buffer.getWritePointer (channel);
            juce::ignoreUnused (channelData);
        }
    }
}

bool LaPautaTuneProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* LaPautaTuneProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

void LaPautaTuneProcessor::getStateInformation (juce::MemoryBlock& destData)
{
}

void LaPautaTuneProcessor::setStateInformation (const void* data, int sizeInBytes)
{
}
