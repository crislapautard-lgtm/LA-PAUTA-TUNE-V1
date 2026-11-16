#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PianoRoll.h"

//==============================================================================
struct PautaLookAndFeel : public juce::LookAndFeel_V4
{
    PautaLookAndFeel()
    {
        const juce::Colour orange (0xffe5953d), dark (0xff1b232b), white (0xffffffff);

        setColour (juce::ResizableWindow::backgroundColourId, juce::Colour (0xff262f38));
        setColour (juce::TextButton::buttonColourId, juce::Colour (0xff3a4652));
        setColour (juce::TextButton::buttonOnColourId, orange);
        setColour (juce::TextButton::textColourOffId, white);
        setColour (juce::TextButton::textColourOnId, juce::Colour (0xff10151b));
        setColour (juce::ComboBox::backgroundColourId, dark);
        setColour (juce::ComboBox::textColourId, white);
        setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff3a4652));
        setColour (juce::ComboBox::arrowColourId, orange);
        setColour (juce::PopupMenu::backgroundColourId, dark);
        setColour (juce::PopupMenu::textColourId, white);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, orange);
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colour (0xff10151b));
        setColour (juce::Slider::rotarySliderFillColourId, orange);
        setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff10151b));
        setColour (juce::Slider::thumbColourId, white);
        setColour (juce::Slider::trackColourId, orange);
        setColour (juce::Slider::backgroundColourId, dark);
        setColour (juce::Slider::textBoxTextColourId, white);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId, juce::Colour (0xffc9d1d9));
        setColour (juce::ScrollBar::thumbColourId, juce::Colour (0xff4b5967));
        setColour (juce::TooltipWindow::backgroundColourId, dark);
        setColour (juce::TooltipWindow::textColourId, white);
    }
};

//==============================================================================
// Icono de nota: se mantiene pulsado y se arrastra al proyecto del DAW.
class DragIcon : public juce::Component,
                 public juce::SettableTooltipClient
{
public:
    explicit DragIcon (LaPautaTuneProcessor& p) : proc (p)
    {
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        setTooltip (T ("Mantén pulsado y arrastra al proyecto para reemplazar la voz por la afinada"));
    }

    void paint (juce::Graphics& g) override
    {
        const float a = proc.hasProject() ? 1.0f : 0.35f;
        const auto r = getLocalBounds().toFloat();
        const float cx = r.getCentreX();

        // hoja de papel
        const juce::Rectangle<float> page (cx - 22.0f, 4.0f, 38.0f, 50.0f);
        g.setColour (juce::Colour (0xfff4f1e8).withAlpha (a));
        g.fillRoundedRectangle (page, 4.0f);
        g.setColour (juce::Colour (0xff9aa0a6).withAlpha (a));
        for (int i = 0; i < 4; ++i)
            g.fillRect (page.getX() + 6.0f, page.getY() + 11.0f + (float) i * 8.0f, 26.0f, 2.0f);

        // lápiz
        juce::Path pencil;
        pencil.addRectangle (0.0f, 0.0f, 8.0f, 26.0f);
        pencil.addTriangle (0.0f, 26.0f, 8.0f, 26.0f, 4.0f, 35.0f);
        pencil.applyTransform (juce::AffineTransform::rotation (0.6f).translated (cx + 12.0f, 18.0f));
        g.setColour (juce::Colour (0xffe5953d).withAlpha (a));
        g.fillPath (pencil);
        g.setColour (juce::Colour (0xff10151b).withAlpha (a));
        g.strokePath (pencil, juce::PathStrokeType (1.0f));

        g.setColour (juce::Colour (0xffc9d1d9).withAlpha (a));
        g.setFont (juce::FontOptions (11.5f));
        g.drawFittedText (T ("Arrastra al proyecto"), 0, 62, getWidth(), 32, juce::Justification::centredTop, 2);
    }

    void mouseDown (const juce::MouseEvent&) override { started = false; }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (started || e.getDistanceFromDragStart() < 6 || ! proc.hasProject())
            return;

        started = true;
        const auto f = proc.exportCorrectedAudio();
        if (f.existsAsFile())
            juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this);
    }

private:
    LaPautaTuneProcessor& proc;
    bool started = false;
};

//==============================================================================
class LaPautaTuneEditor : public juce::AudioProcessorEditor,
                          public juce::ChangeListener,
                          public juce::FileDragAndDropTarget,
                          private juce::Timer
{
public:
    explicit LaPautaTuneEditor (LaPautaTuneProcessor&);
    ~LaPautaTuneEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    void timerCallback() override;
    void refreshFromProcessor();
    void refreshVariationKnob();
    void updateStatus();
    void chooseFile();

    LaPautaTuneProcessor& proc;
    PautaLookAndFeel laf;
    PianoRoll roll;
    DragIcon dragIcon;

    juce::TextButton loadBtn { T ("Cargar voz") }, playBtn { "Play" }, stopBtn { "Stop" }, exportBtn { T ("Exportar WAV") };
    juce::TextButton toolPitchBtn { "Tono" }, toolTimeBtn { "Tiempo" }, toolCutBtn { "Corte" };
    juce::TextButton lockBtn { T ("Bloquear escala") }, centerBtn { "Centrar" }, resetBtn { "Restablecer" }, undoBtn { "Deshacer" };

    juce::ComboBox keyBox, scaleBox, gridBox;
    juce::Slider centerKnob, variationKnob, bpmSlider, zoomSlider;
    juce::Label keyLabel, scaleLabel, gridLabel, bpmLabel, zoomLabel, centerLabel, variationLabel, statusLabel;

    juce::TooltipWindow tooltip { this, 700 };
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LaPautaTuneEditor)
};
