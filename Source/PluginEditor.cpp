#include "PluginProcessor.h"
#include "PluginEditor.h"

LaPautaTuneEditor::LaPautaTuneEditor (LaPautaTuneProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), pianoRoll (p)
{
    addAndMakeVisible (pianoRoll);
    setSize (800, 600);
}

LaPautaTuneEditor::~LaPautaTuneEditor() {}

void LaPautaTuneEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void LaPautaTuneEditor::resized()
{
    pianoRoll.setBounds (getLocalBounds());
}

void LaPautaTuneEditor::setTool (PianoRoll::Tool t)
{
    pianoRoll.setTool (t);
}
