#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

// Piano roll con las notas detectadas de la voz (como Newtone):
// Tono = arrastrar arriba/abajo, Tiempo = arrastrar de lado (con rejilla), Corte = partir una nota.
class PianoRoll : public juce::Component,
                  private juce::ScrollBar::Listener
{
public:
    enum class Tool { pitch, time, cut };

    explicit PianoRoll (LaPautaTuneProcessor& p);

    void setTool (Tool t);
    void setPixelsPerSecond (double pps, double anchorX = -1.0);
    double getPixelsPerSecond() const { return pps; }
    void projectChanged();
    void setPlayheadSeconds (double s);

    std::function<void()> onSelectionChanged;
    std::function<void (double)> onZoomChanged;
    std::function<void()> onBeginEdit;

    //==========================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    static constexpr int keyW = 58, rulerH = 22, rowH = 16, midiLo = 24, midiHi = 108, barSize = 12;

    void scrollBarMoved (juce::ScrollBar*, double newRangeStart) override;

    double sr() const { return std::max (1.0, proc.getSourceSampleRate()); }
    double totalSeconds() const;
    juce::Rectangle<int> contentArea() const;
    int visibleRows() const;
    void clampScroll();
    void updateScrollbars();
    double gridSeconds() const;      // 0 si no hay rejilla

    float xOfSample (double s) const { return (float) keyW + (float) ((s / sr() - scrollX) * pps); }
    double sampleOfX (double x) const { return ((x - keyW) / pps + scrollX) * sr(); }
    float xOfSeconds (double t) const { return (float) keyW + (float) ((t - scrollX) * pps); }
    float yTopOfRow (int midi) const { return (float) rulerH + (float) ((topMidi - midi) * rowH); }
    float yOfMidi (float m) const { return (float) rulerH + (float) ((topMidi - (double) m + 0.5) * rowH); }
    int rowAtY (float y) const { return (int) std::ceil (topMidi - (double) (y - (float) rulerH) / rowH); }

    juce::Rectangle<float> noteRect (const pauta::Note& n) const;
    int hitTest (juce::Point<float> p) const;
    double snapTimelineSample (double s) const;

    void drawRows (juce::Graphics&, juce::Rectangle<int> content);
    void drawGrid (juce::Graphics&, juce::Rectangle<int> content);
    void drawNotes (juce::Graphics&, juce::Rectangle<int> content);
    void drawKeys (juce::Graphics&);
    void drawRuler (juce::Graphics&);

    LaPautaTuneProcessor& proc;
    Tool tool = Tool::pitch;

    juce::ScrollBar hBar { false }, vBar { true };
    double pps = 100.0;
    double scrollX = 0.0;        // segundos
    double topMidi = 84.0;       // fila superior visible
    double playheadSec = 0.0;

    // arrastre
    bool dragging = false;
    juce::Point<int> downPos;
    std::vector<float> initTarget;
    std::vector<int> initShift;

    // vista previa del corte
    int cutHoverIndex = -1;
    double cutHoverSample = 0.0;

    int lastProjectSamples = -1;
    double lastProjectRate = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoRoll)
};
