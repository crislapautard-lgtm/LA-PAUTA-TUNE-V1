#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
class PianoRoll : public juce::Component, public juce::ScrollBar::Listener {
public: enum class Tool{ pitch,time,cut };
 PianoRoll(LaPautaTuneProcessor&); void setTool(Tool); void setPixelsPerSecond(double,double anchorX=-1); void setPlayheadSeconds(double); void projectChanged();
 std::function<void()> onSelectionChanged,onBeginEdit; std::function<void(double)> onZoomChanged;
 double getPixelsPerSecond() const {return pps;}
 void paint(juce::Graphics&) override; void resized() override;
 void mouseDown(const juce::MouseEvent&) override; void mouseDrag(const juce::MouseEvent&) override; void mouseUp(const juce::MouseEvent&) override;
 void mouseMove(const juce::MouseEvent&) override; void mouseDoubleClick(const juce::MouseEvent&) override; void mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails&) override;
 bool keyPressed(const juce::KeyPress&) override; void scrollBarMoved(juce::ScrollBar*,double) override;
private:
 LaPautaTuneProcessor& proc; Tool tool=Tool::pitch; juce::ScrollBar hBar{false},vBar{true};
 double pps=100,scrollX=0,topMidi=72,playheadSec=0; int lastProjectSamples=-1; double lastProjectRate=-1;
 bool dragging=false; juce::Point<float> downPos; std::vector<float> initTarget; std::vector<int> initShift; int cutHoverIndex=-1; double cutHoverSample=0;
 static constexpr int keyW=52,rulerH=26,barSize=14,rowH=16,midiLo=24,midiHi=96;
 double sr() const {return proc.getSourceSampleRate();} double totalSeconds() const;
 juce::Rectangle<int> contentArea() const; int visibleRows() const; void clampScroll(); void updateScrollbars();
 double gridSeconds() const; double snapTimelineSample(double) const;
 float xOfSample(double s) const {return (float)(keyW+(s/sr()-scrollX)*pps);} float xOfSeconds(double t) const {return (float)(keyW+(t-scrollX)*pps);}
 double sampleOfX(double x) const {return ((x-keyW)/pps+scrollX)*sr();}
 float yOfMidi(float m) const {return (float)(rulerH+(topMidi-m)*rowH+rowH*0.5f);} float yTopOfRow(int m) const {return (float)(rulerH+(topMidi-m)*rowH);}
 juce::Rectangle<float> noteRect(const pauta::Note&) const; int hitTest(juce::Point<float>) const;
 void drawRows(juce::Graphics&,juce::Rectangle<int>); void drawGrid(juce::Graphics&,juce::Rectangle<int>); void drawNotes(juce::Graphics&,juce::Rectangle<int>); void drawKeys(juce::Graphics&); void drawRuler(juce::Graphics&);
};
