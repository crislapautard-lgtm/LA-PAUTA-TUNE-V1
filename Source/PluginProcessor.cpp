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

LaPautaTuneProcessor::~LaPautaTuneProcessor() {}

const juce::String LaPautaTuneProcessor::getName() const { return JucePlugin_Name; }
bool LaPautaTuneProcessor::acceptsMidi() const { return false; }
bool LaPautaTuneProcessor::producesMidi() const { return false; }
bool LaPautaTuneProcessor::isMidiEffect() const { return false; }
double LaPautaTuneProcessor::getTailLengthSeconds() const { return 0.0; }
int LaPautaTuneProcessor::getNumPrograms() { return 1; }
int LaPautaTuneProcessor::getCurrentProgram() { return 0; }
void LaPautaTuneProcessor::setCurrentProgram (int) {}
const juce::String LaPautaTuneProcessor::getProgramName (int) { return {}; }
void LaPautaTuneProcessor::changeProgramName (int, const juce::String&) {}

void LaPautaTuneProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    hostSampleRate = sampleRate;
    srcSampleRate = sampleRate;
    const juce::ScopedLock sl (bufLock);
    playBuf.setSize (2, samplesPerBlock);
    playBuf.clear();
}

void LaPautaTuneProcessor::releaseResources() {}

bool LaPautaTuneProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
  #endif
}

void LaPautaTuneProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    for (int i = 0; i < buffer.getNumChannels(); ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
}

bool LaPautaTuneProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* LaPautaTuneProcessor::createEditor()
{
    return new LaPautaTuneEditor (*this);
}

void LaPautaTuneProcessor::getStateInformation (juce::MemoryBlock&) {}
void LaPautaTuneProcessor::setStateInformation (const void*, int) {}

// Stubs requeridos por el editor
void LaPautaTuneProcessor::loadFile (const juce::File&) {}
bool LaPautaTuneProcessor::hasProject() const { return false; }
void LaPautaTuneProcessor::setPlaying (bool) {}
void LaPautaTuneProcessor::setPlayPositionSeconds (double) {}
void LaPautaTuneProcessor::exportCorrectedAudio (const juce::File&) {}
void LaPautaTuneProcessor::lockScale (bool) {}
void LaPautaTuneProcessor::centerNotes() {}
void LaPautaTuneProcessor::resetNotes() {}
void LaPautaTuneProcessor::undo() {}
void LaPautaTuneProcessor::requestRender() {}
