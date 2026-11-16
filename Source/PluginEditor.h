#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PianoRoll.h"

class LaPautaTuneEditor : public juce::AudioProcessorEditor, public juce::ChangeListener, public juce::Timer, public juce::FileDragAndDropTarget
{
public:
    LaPautaTuneEditor(LaPautaTuneProcessor&);
    ~LaPautaTuneEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray&, int, int) override;
private:
    LaPautaTuneProcessor& proc;
    PianoRoll roll;
    juce::TextButton loadBtn { "Cargar voz" }, playBtn { "Play" }, stopBtn { "Stop" }, exportBtn { "Exportar WAV" };
    juce::TextButton tonoBtn { "Tono" }, tiempoBtn { "Tiempo" }, corteBtn { "Corte" };
    juce::TextButton lockBtn { "Bloquear escala" }, centerBtn { "Centrar" }, resetBtn { "Restablecer" }, undoBtn { "Deshacer" };
    juce::ComboBox keyBox, scaleBox, gridBox, bpmBox;
    juce::Slider zoomSlider, centroKnob, variacionKnob;
    juce::Label centroLabel { {}, "100 %\nCentro" }, variacionLabel { {}, "100 %\nVariación" }, statusLabel;
    juce::TextButton dragBtn { "Arrastra al\nproyecto" };
    juce::Label titleLabel { {}, "LA PAUTA TUNE" };
    void refreshFromProcessor();
    void setTool(PianoRoll::Tool);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LaPautaTuneEditor)
};
