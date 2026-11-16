#include "PianoRoll.h"
#include "PluginProcessor.h"

PianoRoll::PianoRoll (LaPautaTuneProcessor& p) : audioProcessor (p) {}

PianoRoll::~PianoRoll() {}

void PianoRoll::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB(30, 30, 30));

    g.setColour (juce::Colour::fromRGB(50, 50, 50));
    float heightPerPage = (float)getHeight() / 12.0f;
    for (int i = 0; i < 12; ++i)
    {
        // Corrección de static_int_cast a static_cast<int>
        g.drawHorizontalLine (static_cast<int>(i * heightPerPage), 0.0f, (float)getWidth());
    }

    g.setColour (juce::Colours::white);
    g.setFont (14.0f);
    g.drawText ("La Pauta Tune - Piano Roll", getLocalBounds(), juce::Justification::centredTop, true);
}

void PianoRoll::resized() {}

void PianoRoll::mouseDown (const juce::MouseEvent& event)
{
    auto clickPos = event.getPosition().toFloat();
    juce::ignoreUnused (clickPos);
    repaint();
}

void PianoRoll::mouseDrag (const juce::MouseEvent& event)
{
    auto dragPos = event.getPosition().toFloat();
    juce::ignoreUnused (dragPos);
    repaint();
}

void PianoRoll::setTool (Tool newTool) { currentTool = newTool; }
void PianoRoll::setPixelsPerSecond (double pps) { pixelsPerSecond = pps; repaint(); }
