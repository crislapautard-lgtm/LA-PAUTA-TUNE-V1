#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class LaPautaTuneProcessor;

class PianoRoll : public juce::Component
{
public:
    enum class Tool { pitch, time, cut };

    PianoRoll (LaPautaTuneProcessor& processor);
    ~PianoRoll() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;

    void setTool (Tool newTool);
    void setPixelsPerSecond (double pps);

    std::function<void(double)> onZoomChanged;
    std::function<void()> onSelectionChanged;

private:
    LaPautaTuneProcessor& audioProcessor;
    Tool currentTool = Tool::pitch;
    double pixelsPerSecond = 100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoRoll)
};
