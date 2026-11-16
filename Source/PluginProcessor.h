#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <memory>
#include <vector>

#include "Engine.h"
#include "Scales.h"

inline juce::String T (const char* utf8) { return juce::String::fromUTF8 (utf8); }

class LaPautaTuneProcessor : public juce::AudioProcessor,
                             public juce::ChangeBroadcaster
{
public:
    LaPautaTuneProcessor();
    ~LaPautaTuneProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "La Pauta Tune"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //==========================================================================
    // Proyecto (voz cargada)
    void loadFile (const juce::File& file);
    bool hasProject() const { return mono != nullptr && track != nullptr; }
    bool isLoading() const { return loading.load(); }
    float getLoadProgress() const { return loadProgress.load(); }
    juce::String getProjectName() const { return projectName; }
    juce::String getMessageText() const { return messageText; }
    void setMessageText (const juce::String& s) { messageText = s; sendChangeMessage(); }

    double getSourceSampleRate() const { return srcSampleRate; }
    int getNumSourceSamples() const { return mono != nullptr ? (int) mono->size() : 0; }
    const std::vector<float>& getPeaks() const { return peaks; }
    static constexpr int peakBlock = 128;

    std::vector<pauta::Note>& getNotes() { return notes; }
    const pauta::F0Track* getTrack() const { return track.get(); }

    //==========================================================================
    // Edición
    void pushUndo();
    void undo();
    void requestRender();
    bool cutNote (int index, int atSample);
    void centerNotes();                       // Centrar a la escala elegida
    void resetNotes();                        // volver al tono detectado
    void applyVariation (float v);
    bool anySelected() const;
    void selectAll (bool shouldSelect);

    pauta::ScaleMask getScaleMask() const { return pauta::makeScaleMask (key, scaleType); }

    juce::File exportCorrectedAudio();        // renderiza y guarda un WAV, devuelve el archivo

    //==========================================================================
    // Reproducción
    void play() { internalPlaying = true; }
    void pausePlayback() { internalPlaying = false; }
    void stopPlayback() { internalPlaying = false; setPlayPositionSeconds (0.0); }
    bool isPlayingInternal() const { return internalPlaying.load(); }
    void setPlayPositionSeconds (double s);
    double getPlayPositionSeconds() const { return playPosSeconds.load(); }

    //==========================================================================
    // Ajustes (solo hilo de mensajes, salvo bpm)
    int key = 0;
    int scaleType = 0;
    bool lockScale = false;
    int gridIndex = 2;                        // 0:1/1 1:1/2 2:1/4 3:1/8 4:1/16 5:1/32 6:sin rejilla
    float centerAmount = 1.0f;                // 0..1
    std::atomic<double> bpm { 120.0 };

    static double gridDivision (int gridIdx)  // 0 si no hay rejilla
    {
        static const double d[7] = { 1, 2, 4, 8, 16, 32, 0 };
        return d[std::clamp (gridIdx, 0, 6)];
    }

private:
    class AnalysisThread;
    class RenderThread;
    struct LoadResult;

    void installProject (LoadResult& r);
    void restoreState (const juce::String& path, std::vector<pauta::Note> savedNotes);

    // datos del proyecto (hilo de mensajes)
    std::shared_ptr<const std::vector<float>> mono;
    std::shared_ptr<const pauta::F0Track> track;
    std::vector<pauta::Note> notes;
    std::vector<float> peaks;
    std::vector<std::vector<pauta::Note>> undoStack;
    juce::String projectName, projectPath, messageText;
    double srcSampleRate = 44100.0;
    int srcChannels = 1;
    int loadGeneration = 0;

    std::atomic<bool> loading { false };
    std::atomic<float> loadProgress { 0.0f };
    std::unique_ptr<AnalysisThread> analysisThread;

    // render en segundo plano
    juce::CriticalSection renderLock;
    std::shared_ptr<const std::vector<float>> renderMono;
    std::shared_ptr<const pauta::F0Track> renderTrack;
    std::vector<pauta::Note> renderNotes;
    std::unique_ptr<RenderThread> renderThread;

    // reproducción
    juce::SpinLock bufLock;
    std::shared_ptr<const std::vector<float>> playBuf;
    std::atomic<bool> internalPlaying { false };
    std::atomic<double> internalPos { 0.0 };      // en muestras de la voz original
    std::atomic<double> playPosSeconds { 0.0 };
    double hostSampleRate = 44100.0;

    JUCE_DECLARE_WEAK_REFERENCEABLE (LaPautaTuneProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LaPautaTuneProcessor)
};
