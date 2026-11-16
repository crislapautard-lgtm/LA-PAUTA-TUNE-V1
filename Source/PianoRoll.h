#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class PianoRoll : public juce::Component
{
public:
    PianoRoll();
    ~PianoRoll() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoRoll)
};
