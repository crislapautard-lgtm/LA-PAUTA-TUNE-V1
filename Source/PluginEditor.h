#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class LaPautaTuneAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit LaPautaTuneAudioProcessorEditor(LaPautaTuneAudioProcessor&);
    ~LaPautaTuneAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    LaPautaTuneAudioProcessor& processor;

    juce::Label title;
    juce::Label subtitle;
    juce::Label status;

    juce::ComboBox keyBox, scaleBox;
    juce::Slider centerSlider, variationSlider, transposeSlider;
    juce::Slider retuneSlider, humanizeSlider, outputSlider;

    juce::Label keyLabel, scaleLabel;
    juce::Label centerLabel, variationLabel, transposeLabel;
    juce::Label retuneLabel, humanizeLabel, outputLabel;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::unique_ptr<ComboAttachment> keyAttachment, scaleAttachment;
    std::unique_ptr<SliderAttachment> centerAttachment, variationAttachment;
    std::unique_ptr<SliderAttachment> transposeAttachment, retuneAttachment;
    std::unique_ptr<SliderAttachment> humanizeAttachment, outputAttachment;

    void setupSlider(juce::Slider&, juce::Label&, const juce::String&);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LaPautaTuneAudioProcessorEditor)
};
