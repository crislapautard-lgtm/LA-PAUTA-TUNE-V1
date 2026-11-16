#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "PianoRoll.h"

class LaPautaTuneEditor : public juce::AudioProcessorEditor
{
public:
    LaPautaTuneEditor (LaPautaTuneProcessor&);
    ~LaPautaTuneEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void setTool (PianoRoll::Tool t);

private:
    LaPautaTuneProcessor& audioProcessor;
    PianoRoll pianoRoll;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LaPautaTuneEditor)
};
