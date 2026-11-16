#pragma once
#include <JuceHeader.h>
#include "Scales.h"
#include "Engine.h"
class LaPautaTuneProcessor : public juce::AudioProcessor, public juce::ChangeBroadcaster {
public:
 LaPautaTuneProcessor(); ~LaPautaTuneProcessor() override;
 void prepareToPlay(double,int) override; void releaseResources() override {}
 bool isBusesLayoutSupported(const BusesLayout&) const override;
 void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
 juce::AudioProcessorEditor* createEditor() override; bool hasEditor() const override {return true;}
 const juce::String getName() const override {return "La Pauta Tune";}
 bool acceptsMidi() const override {return false;} bool producesMidi() const override {return false;}
 double getTailLengthSeconds() const override {return 0;} int getNumPrograms() override {return 1;}
 int getCurrentProgram() override {return 0;} void setCurrentProgram(int) override {}
 const juce::String getProgramName(int) override {return {};} void changeProgramName(int,const juce::String&) override {}
 void getStateInformation(juce::MemoryBlock&) override; void setStateInformation(const void*,int) override;
 void loadFile(const juce::File&); void restoreState(const juce::String&,std::vector<pauta::Note>);
 bool hasProject() const {return mono!=nullptr&&track!=nullptr;}
 bool isLoading() const {return loading;} float getLoadProgress() const {return loadProgress.load();}
 juce::String getMessage() const {return messageText;} juce::String getProjectName() const {return projectName;}
 int getNumSourceSamples() const {return mono?(int)mono->size():0;} double getSourceSampleRate() const {return srcSampleRate;}
 const std::vector<pauta::Note>& getNotes() const {return notes;} std::vector<pauta::Note>& getNotes(){return notes;}
 const std::vector<float>& getPeaks() const {return peaks;}
 void requestRender(); void pushUndo(); void undo(); bool cutNote(int,int); void selectAll(bool); bool anySelected() const;
 void centerNotes(); void resetNotes(); void applyVariation(float);
 juce::File exportCorrectedAudio(); juce::File exportForDragAndDrop();
 void setPlaying(bool p){ internalPlaying=p; } bool isInternalPlaying() const {return internalPlaying.load();}
 void setPlayPositionSeconds(double); double getPlayPosSeconds() const {return playPosSeconds.load();}
 int key=9, scaleType=1, gridIndex=2; bool lockScale=true; float centerAmount=1.f, variationAmount=1.f;
 std::atomic<double> bpm{88.0};
 pauta::ScaleMask getScaleMask() const {return pauta::makeScaleMask(key,scaleType);}
 static double gridDivision(int idx){ static const double d[]={0,1,2,4,8,16}; return (idx>=0&&idx<6)?d[idx]:4.0; }
 static constexpr int peakBlock=256;
 struct LoadResult; class AnalysisThread; class RenderThread;
private:
 void installProject(LoadResult&);
 std::shared_ptr<std::vector<float>> mono; std::shared_ptr<pauta::F0Track> track;
 std::vector<pauta::Note> notes; std::vector<float> peaks;
 double srcSampleRate=44100; int srcChannels=1; juce::String projectName,projectPath,messageText;
 std::atomic<bool> loading{false}; std::atomic<float> loadProgress{0}; std::atomic<int> loadGeneration{0};
 std::unique_ptr<AnalysisThread> analysisThread; std::unique_ptr<RenderThread> renderThread;
 juce::CriticalSection renderLock; std::shared_ptr<const std::vector<float>> renderMono; std::shared_ptr<const pauta::F0Track> renderTrack; std::vector<pauta::Note> renderNotes;
 juce::SpinLock bufLock; std::shared_ptr<const std::vector<float>> playBuf;
 std::atomic<double> internalPos{0}, playPosSeconds{0}, hostSampleRate{44100}; std::atomic<bool> internalPlaying{false};
 std::vector<std::vector<pauta::Note>> undoStack;
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LaPautaTuneProcessor)
};
