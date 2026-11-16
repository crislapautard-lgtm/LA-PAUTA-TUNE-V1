#include "PluginEditor.h"

LaPautaTuneAudioProcessorEditor::LaPautaTuneAudioProcessorEditor(
    LaPautaTuneAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(1000, 680);
    setResizable(true, true);
    setResizeLimits(760, 540, 1500, 1000);

    title.setText("LA PAUTA TUNE", juce::dontSendNotification);
    title.setFont(juce::Font(juce::FontOptions(30.0f, juce::Font::bold)));
    title.setColour(juce::Label::textColourId, juce::Colour(0xff202124));
    addAndMakeVisible(title);

    subtitle.setText("VOCAL PITCH EDITOR  |  IMPORT  •  EDIT  •  EXPORT",
                     juce::dontSendNotification);
    subtitle.setFont(juce::Font(juce::FontOptions(12.0f)));
    subtitle.setColour(juce::Label::textColourId, juce::Colour(0xff777777));
    addAndMakeVisible(subtitle);

    keyBox.addItemList({"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}, 1);
    scaleBox.addItemList({"Chromatic", "Major", "Minor"}, 1);
    addAndMakeVisible(keyBox);
    addAndMakeVisible(scaleBox);

    keyLabel.setText("KEY", juce::dontSendNotification);
    scaleLabel.setText("SCALE", juce::dontSendNotification);
    for (auto* label : {&keyLabel, &scaleLabel})
    {
        label->setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        label->setColour(juce::Label::textColourId, juce::Colour(0xff55585c));
        addAndMakeVisible(*label);
    }

    setupSlider(centerSlider, centerLabel, "CENTER");
    setupSlider(variationSlider, variationLabel, "VARIATION");
    setupSlider(transposeSlider, transposeLabel, "TRANSPOSE");
    setupSlider(retuneSlider, retuneLabel, "RETUNE SPEED");
    setupSlider(humanizeSlider, humanizeLabel, "HUMANIZE");
    setupSlider(outputSlider, outputLabel, "OUTPUT");

    centerSlider.setRange(0.0, 100.0, 0.1);
    variationSlider.setRange(0.0, 100.0, 0.1);
    transposeSlider.setRange(-12.0, 12.0, 0.01);
    retuneSlider.setRange(1.0, 500.0, 0.1);
    humanizeSlider.setRange(0.0, 100.0, 0.1);
    outputSlider.setRange(-24.0, 12.0, 0.01);

    auto& apvts = processor.parameters;
    keyAttachment = std::make_unique<ComboAttachment>(apvts, "key", keyBox);
    scaleAttachment = std::make_unique<ComboAttachment>(apvts, "scale", scaleBox);
    centerAttachment = std::make_unique<SliderAttachment>(apvts, "center", centerSlider);
    variationAttachment = std::make_unique<SliderAttachment>(apvts, "variation", variationSlider);
    transposeAttachment = std::make_unique<SliderAttachment>(apvts, "transpose", transposeSlider);
    retuneAttachment = std::make_unique<SliderAttachment>(apvts, "retune", retuneSlider);
    humanizeAttachment = std::make_unique<SliderAttachment>(apvts, "humanize", humanizeSlider);
    outputAttachment = std::make_unique<SliderAttachment>(apvts, "output", outputSlider);

    status.setText("ENGINE STATUS: FOUNDATION BUILD — PITCH EDITOR NOT IMPLEMENTED YET",
                   juce::dontSendNotification);
    status.setFont(juce::Font(juce::FontOptions(11.0f)));
    status.setColour(juce::Label::textColourId, juce::Colour(0xff777777));
    addAndMakeVisible(status);
}

void LaPautaTuneAudioProcessorEditor::setupSlider(
    juce::Slider& slider, juce::Label& label, const juce::String& text)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 78, 22);
    slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffaeb3b9));
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xffd0d3d7));
    slider.setColour(juce::Slider::thumbColourId, juce::Colour(0xff777d84));
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xff333333));
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xfff1f2f3));
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xffd7d9dc));
    addAndMakeVisible(slider);

    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    label.setColour(juce::Label::textColourId, juce::Colour(0xff55585c));
    addAndMakeVisible(label);
}

void LaPautaTuneAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xffffffff));

    g.setColour(juce::Colour(0xffe7e8ea));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(10.0f), 12.0f, 1.0f);

    auto editorArea = juce::Rectangle<float>(24.0f, 180.0f,
        static_cast<float>(getWidth() - 48), 300.0f);
    g.setColour(juce::Colour(0xfff4f5f6));
    g.fillRoundedRectangle(editorArea, 8.0f);

    g.setColour(juce::Colour(0xffd9dcdf));
    for (int i = 0; i <= 24; ++i)
    {
        const float x = editorArea.getX() + editorArea.getWidth() * i / 24.0f;
        g.drawVerticalLine(static_cast<int>(x), editorArea.getY(), editorArea.getBottom());
    }
    for (int i = 0; i <= 12; ++i)
    {
        const float y = editorArea.getY() + editorArea.getHeight() * i / 12.0f;
        g.drawHorizontalLine(static_cast<int>(y), editorArea.getX(), editorArea.getRight());
    }

    g.setColour(juce::Colour(0xff666b70));
    g.setFont(13.0f);
    g.drawText("VOICE NOTE EDITOR AREA", editorArea.toNearestInt().reduced(12),
               juce::Justification::centred, false);
    g.setColour(juce::Colour(0xff888d92));
    g.setFont(11.0f);
    g.drawText("Pitch detection, draggable notes and audio import/export are the next engine stage.",
               editorArea.toNearestInt().withTrimmedTop(165).reduced(12),
               juce::Justification::centred, false);
}

void LaPautaTuneAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(24);
    title.setBounds(area.removeFromTop(42));
    subtitle.setBounds(area.removeFromTop(24));

    auto selectors = area.removeFromTop(62);
    keyLabel.setBounds(selectors.removeFromLeft(40).withY(selectors.getY() + 2).withHeight(18));
    keyBox.setBounds(selectors.removeFromLeft(100).withY(selectors.getY() + 20).withHeight(28));
    selectors.removeFromLeft(24);
    scaleLabel.setBounds(selectors.removeFromLeft(55).withY(selectors.getY() + 2).withHeight(18));
    scaleBox.setBounds(selectors.removeFromLeft(150).withY(selectors.getY() + 20).withHeight(28));

    area.removeFromTop(310);
    auto controls = area.removeFromTop(210);
    const int cell = controls.getWidth() / 6;
    juce::Slider* sliders[] = {&centerSlider, &variationSlider, &transposeSlider,
                               &retuneSlider, &humanizeSlider, &outputSlider};
    juce::Label* labels[] = {&centerLabel, &variationLabel, &transposeLabel,
                             &retuneLabel, &humanizeLabel, &outputLabel};

    for (int i = 0; i < 6; ++i)
    {
        auto column = controls.removeFromLeft(i == 5 ? controls.getWidth() : cell);
        labels[i]->setBounds(column.removeFromTop(24));
        sliders[i]->setBounds(column.reduced(5));
    }
    status.setBounds(getLocalBounds().reduced(24).removeFromBottom(22));
}
