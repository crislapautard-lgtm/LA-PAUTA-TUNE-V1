#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class LaPautaTuneProcessor : public juce::AudioProcessor
{
public:
    LaPautaTuneProcessor();
    ~LaPautaTuneProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Métodos requeridos por la interfaz y controles
    void loadFile (const juce::File& file);
    bool hasProject() const;
    void setPlaying (bool playing);
    void setPlayPositionSeconds (double seconds);
    void exportCorrectedAudio (const juce::File& file);
    void lockScale (bool locked);
    void centerNotes();
    void resetNotes();
    void undo();
    void requestRender();

    int key = 0;
    int scaleType = 0;
    int gridIndex = 0;

    JUCE_DECLARE_WEAK_REFERENCEABLE (LaPautaTuneProcessor)

    juce::CriticalSection bufLock;
    juce::AudioBuffer<float> playBuf;
    
    double hostSampleRate = 44100.0;
    double srcSampleRate = 44100.0;
    double bpm = 120.0;

    bool internalPlaying = false;
    double internalPos = 0.0;

    std::unique_ptr<juce::Thread> renderThread;
    std::unique_ptr<juce::Thread> analysisThread;

    bool mono = false;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LaPautaTuneProcessor)
};
