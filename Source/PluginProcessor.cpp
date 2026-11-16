#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
struct LaPautaTuneProcessor::LoadResult
{
    int generation = 0;
    bool ok = false;
    juce::String error, path, name;
    double sampleRate = 44100.0;
    int channels = 1;
    std::shared_ptr<std::vector<float>> mono;
    std::shared_ptr<pauta::F0Track> track;
    std::vector<pauta::Note> notes;
    std::vector<float> peaks;
};

//==============================================================================
class LaPautaTuneProcessor::AnalysisThread : public juce::Thread
{
public:
    AnalysisThread (LaPautaTuneProcessor& o, juce::File f, std::vector<pauta::Note> saved, int gen)
        : juce::Thread ("La Pauta Tune analisis"), owner (o), file (std::move (f)),
          savedNotes (std::move (saved)), generation (gen)
    {
    }

    void run() override
    {
        auto res = std::make_shared<LoadResult>();
        res->generation = generation;
        res->path = file.getFullPathName();
        res->name = file.getFileNameWithoutExtension();

        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));

        if (reader == nullptr || reader->lengthInSamples <= 0 || reader->numChannels == 0)
        {
            res->error = T ("No se pudo abrir el audio. Usa WAV, AIFF, FLAC, MP3 u OGG.");
            post (res);
            return;
        }

        res->sampleRate = reader->sampleRate;
        res->channels = (int) reader->numChannels;
        const int64_t total = reader->lengthInSamples;

        if (total > (int64_t) 60 * 60 * (int64_t) reader->sampleRate)
        {
            res->error = T ("El audio es demasiado largo (máximo 60 minutos).");
            post (res);
            return;
        }

        auto mono = std::make_shared<std::vector<float>> ((size_t) total, 0.0f);
        {
            const int chunk = 65536;
            juce::AudioBuffer<float> buf ((int) reader->numChannels, chunk);
            for (int64_t pos = 0; pos < total; pos += chunk)
            {
                if (threadShouldExit())
                    return;
                const int len = (int) std::min<int64_t> (chunk, total - pos);
                reader->read (&buf, 0, len, pos, true, true);
                for (int i = 0; i < len; ++i)
                {
                    float s = 0.0f;
                    for (int c = 0; c < buf.getNumChannels(); ++c)
                        s += buf.getSample (c, i);
                    (*mono)[(size_t) (pos + i)] = s / (float) buf.getNumChannels();
                }
            }
        }

        // picos para dibujar la onda dentro de las notas
        {
            const size_t nb = (size_t) ((mono->size() + (size_t) peakBlock - 1) / (size_t) peakBlock);
            res->peaks.assign (nb, 0.0f);
            float gmax = 1.0e-6f;
            for (size_t b = 0; b < nb; ++b)
            {
                const size_t a = b * (size_t) peakBlock;
                const size_t e = std::min (mono->size(), a + (size_t) peakBlock);
                float m = 0.0f;
                for (size_t i = a; i < e; ++i)
                    m = std::max (m, std::fabs ((*mono)[i]));
                res->peaks[b] = m;
                gmax = std::max (gmax, m);
            }
            for (auto& p : res->peaks)
                p /= gmax;
        }

        auto trk = std::make_shared<pauta::F0Track>();
        const bool finished = pauta::analyzePitch (*mono, res->sampleRate, *trk, [this] (float p)
        {
            owner.loadProgress.store (p);
            return ! threadShouldExit();
        });

        if (! finished || threadShouldExit())
            return;

        res->mono = mono;
        res->track = trk;
        res->notes = savedNotes.empty() ? pauta::segmentNotes (*trk, (int) mono->size()) : savedNotes;
        res->ok = true;
        post (res);
    }

private:
    void post (std::shared_ptr<LoadResult> res)
    {
        if (threadShouldExit())
            return;
        juce::WeakReference<LaPautaTuneProcessor> weak (&owner);
        juce::MessageManager::callAsync ([weak, res]
        {
            if (weak != nullptr)
                weak->installProject (*res);
        });
    }

    LaPautaTuneProcessor& owner;
    juce::File file;
    std::vector<pauta::Note> savedNotes;
    int generation;
};

//==============================================================================
class LaPautaTuneProcessor::RenderThread : public juce::Thread
{
public:
    explicit RenderThread (LaPautaTuneProcessor& o) : juce::Thread ("La Pauta Tune render"), owner (o) {}

    void run() override
    {
        while (! threadShouldExit())
        {
            wait (-1);
            while (dirty.exchange (false) && ! threadShouldExit())
            {
                std::shared_ptr<const std::vector<float>> m;
                std::shared_ptr<const pauta::F0Track> t;
                std::vector<pauta::Note> n;
                {
                    const juce::ScopedLock sl (owner.renderLock);
                    m = owner.renderMono;
                    t = owner.renderTrack;
                    n = owner.renderNotes;
                }
                if (m == nullptr || t == nullptr)
                    continue;

                auto result = std::make_shared<const std::vector<float>> (pauta::render (*m, *t, n));
                const juce::SpinLock::ScopedLockType sl (owner.bufLock);
                owner.playBuf = result;
            }
        }
    }

    std::atomic<bool> dirty { false };

private:
    LaPautaTuneProcessor& owner;
};

//==============================================================================
LaPautaTuneProcessor::LaPautaTuneProcessor()
    : juce::AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    renderThread = std::make_unique<RenderThread> (*this);
    renderThread->startThread();
}

LaPautaTuneProcessor::~LaPautaTuneProcessor()
{
    if (analysisThread != nullptr)
        analysisThread->stopThread (5000);

    renderThread->signalThreadShouldExit();
    renderThread->notify();
    renderThread->stopThread (5000);
}

void LaPautaTuneProcessor::prepareToPlay (double sampleRate, int)
{
    hostSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
}

bool LaPautaTuneProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

//==============================================================================
void LaPautaTuneProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    midi.clear();
    buffer.clear();

    bool hostPlaying = false;
    double hostSeconds = 0.0;

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            hostPlaying = pos->getIsPlaying();
            if (auto t = pos->getTimeInSeconds())
                hostSeconds = *t;
            if (auto b = pos->getBpm())
                if (*b > 0.0)
                    bpm.store (*b);
        }

    std::shared_ptr<const std::vector<float>> buf;
    {
        const juce::SpinLock::ScopedLockType sl (bufLock);
        buf = playBuf;
    }

    const double srcSR = srcSampleRate;
    const int n = buffer.getNumSamples();
    const double ratio = srcSR / hostSampleRate;

    double pos = 0.0;
    bool active = false;

    if (hostPlaying)
    {
        pos = hostSeconds * srcSR;
        active = true;
    }
    else if (internalPlaying.load())
    {
        pos = internalPos.load();
        active = true;
    }

    if (buf != nullptr && active && ! buf->empty())
    {
        const auto& v = *buf;
        const double size = (double) v.size();
        const int nch = buffer.getNumChannels();

        for (int i = 0; i < n; ++i)
        {
            const double p = pos + (double) i * ratio;
            if (p < 0.0 || p >= size - 1.0)
                continue;

            const int i0 = (int) p;
            const float fr = (float) (p - (double) i0);
            const float s = v[(size_t) i0] + (v[(size_t) i0 + 1] - v[(size_t) i0]) * fr;
            for (int c = 0; c < nch; ++c)
                buffer.setSample (c, i, s);
        }

        if (! hostPlaying)
        {
            double np = pos + (double) n * ratio;
            if (np >= size - 1.0)
            {
                internalPlaying = false;
                np = 0.0;
            }
            internalPos.store (np);
        }
    }
    else if (! hostPlaying && internalPlaying.load())
    {
        internalPlaying = false;
    }

    playPosSeconds.store (hostPlaying ? hostSeconds : internalPos.load() / srcSR);
}

void LaPautaTuneProcessor::setPlayPositionSeconds (double s)
{
    s = std::max (0.0, s);
    internalPos.store (s * srcSampleRate);
    playPosSeconds.store (s);
}

//==============================================================================
void LaPautaTuneProcessor::loadFile (const juce::File& file)
{
    if (analysisThread != nullptr)
        analysisThread->stopThread (5000);

    ++loadGeneration;
    loading = true;
    loadProgress = 0.0f;
    messageText = T ("Analizando la voz…");
    sendChangeMessage();

    analysisThread = std::make_unique<AnalysisThread> (*this, file, std::vector<pauta::Note>(), loadGeneration);
    analysisThread->startThread();
}

void LaPautaTuneProcessor::restoreState (const juce::String& path, std::vector<pauta::Note> savedNotes)
{
    const juce::File f (path);
    if (! f.existsAsFile())
    {
        messageText = T ("No se encontró la voz guardada: ") + path;
        sendChangeMessage();
        return;
    }

    if (analysisThread != nullptr)
        analysisThread->stopThread (5000);

    ++loadGeneration;
    loading = true;
    loadProgress = 0.0f;
    messageText = T ("Analizando la voz…");
    sendChangeMessage();

    analysisThread = std::make_unique<AnalysisThread> (*this, f, std::move (savedNotes), loadGeneration);
    analysisThread->startThread();
}

void LaPautaTuneProcessor::installProject (LoadResult& r)
{
    if (r.generation != loadGeneration)
        return;

    loading = false;

    if (! r.ok)
    {
        messageText = r.error;
        sendChangeMessage();
        return;
    }

    mono = r.mono;
    track = r.track;
    notes = std::move (r.notes);
    peaks = std::move (r.peaks);
    srcSampleRate = r.sampleRate;
    srcChannels = r.channels;
    projectName = r.name;
    projectPath = r.path;
    undoStack.clear();

    internalPlaying = false;
    setPlayPositionSeconds (0.0);

    messageText = projectName + T ("  –  ") + juce::String ((int) notes.size()) + T (" notas detectadas");
    requestRender();
    sendChangeMessage();
}

//==============================================================================
void LaPautaTuneProcessor::requestRender()
{
    if (mono == nullptr || track == nullptr)
        return;

    {
        const juce::ScopedLock sl (renderLock);
        renderMono = mono;
        renderTrack = track;
        renderNotes = notes;
    }
    renderThread->dirty = true;
    renderThread->notify();
}

void LaPautaTuneProcessor::pushUndo()
{
    undoStack.push_back (notes);
    if (undoStack.size() > 100)
        undoStack.erase (undoStack.begin());
}

void LaPautaTuneProcessor::undo()
{
    if (undoStack.empty())
        return;
    notes = std::move (undoStack.back());
    undoStack.pop_back();
    requestRender();
    sendChangeMessage();
}

bool LaPautaTuneProcessor::cutNote (int index, int atSample)
{
    if (track == nullptr)
        return false;
    pushUndo();
    if (! pauta::splitNote (notes, index, atSample, *track))
    {
        undoStack.pop_back();
        return false;
    }
    requestRender();
    return true;
}

bool LaPautaTuneProcessor::anySelected() const
{
    for (const auto& n : notes)
        if (n.selected)
            return true;
    return false;
}

void LaPautaTuneProcessor::selectAll (bool shouldSelect)
{
    for (auto& n : notes)
        n.selected = shouldSelect;
}

void LaPautaTuneProcessor::centerNotes()
{
    const auto mask = getScaleMask();
    const bool onlySel = anySelected();
    for (auto& n : notes)
    {
        if (onlySel && ! n.selected)
            continue;
        const float snap = (float) pauta::nearestInScale (n.detectedMidi, mask);
        n.targetMidi = n.detectedMidi + (snap - n.detectedMidi) * centerAmount;
    }
    requestRender();
}

void LaPautaTuneProcessor::resetNotes()
{
    const bool onlySel = anySelected();
    for (auto& n : notes)
    {
        if (onlySel && ! n.selected)
            continue;
        n.targetMidi = n.detectedMidi;
        n.variation = 1.0f;
        n.shift = 0;
    }
    requestRender();
}

void LaPautaTuneProcessor::applyVariation (float v)
{
    const bool onlySel = anySelected();
    for (auto& n : notes)
        if (! onlySel || n.selected)
            n.variation = v;
    requestRender();
}

//==============================================================================
juce::File LaPautaTuneProcessor::exportCorrectedAudio()
{
    if (mono == nullptr || track == nullptr)
        return {};

    const auto out = pauta::render (*mono, *track, notes);

    auto folder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("La Pauta Tune");
    if (! folder.createDirectory().wasOk())
        folder = juce::File::getSpecialLocation (juce::File::tempDirectory);

    auto safeName = juce::File::createLegalFileName (projectName.isEmpty() ? juce::String ("voz") : projectName);
    auto file = folder.getChildFile (safeName + "_afinada_" + juce::Time::getCurrentTime().formatted ("%H%M%S") + ".wav");

    const int nch = std::max (1, srcChannels);
    juce::AudioBuffer<float> buf (nch, (int) out.size());
    for (int c = 0; c < nch; ++c)
        buf.copyFrom (c, 0, out.data(), (int) out.size());

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::FileOutputStream> stream (file.createOutputStream());
    if (stream == nullptr)
        return {};

    std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (stream.get(), srcSampleRate, (unsigned int) nch, 24, {}, 0));
    if (writer == nullptr)
        return {};
    stream.release(); // el writer se queda con el stream

    writer->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
    writer.reset();

    messageText = T ("Guardado: ") + file.getFullPathName();
    sendChangeMessage();
    return file;
}

//==============================================================================
void LaPautaTuneProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::XmlElement xml ("LaPautaTune");
    xml.setAttribute ("file", projectPath);
    xml.setAttribute ("key", key);
    xml.setAttribute ("scale", scaleType);
    xml.setAttribute ("lock", lockScale);
    xml.setAttribute ("grid", gridIndex);
    xml.setAttribute ("center", (double) centerAmount);
    xml.setAttribute ("bpm", bpm.load());

    for (const auto& n : notes)
    {
        auto* e = xml.createNewChildElement ("Note");
        e->setAttribute ("s", n.start);
        e->setAttribute ("e", n.end);
        e->setAttribute ("d", (double) n.detectedMidi);
        e->setAttribute ("t", (double) n.targetMidi);
        e->setAttribute ("v", (double) n.variation);
        e->setAttribute ("sh", n.shift);
    }
    copyXmlToBinary (xml, dest);
}

void LaPautaTuneProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName ("LaPautaTune"))
        return;

    const auto path = xml->getStringAttribute ("file");
    const int k = xml->getIntAttribute ("key", 0);
    const int sc = xml->getIntAttribute ("scale", 0);
    const bool lk = xml->getBoolAttribute ("lock", false);
    const int gr = xml->getIntAttribute ("grid", 2);
    const float ca = (float) xml->getDoubleAttribute ("center", 1.0);
    const double bp = xml->getDoubleAttribute ("bpm", 120.0);

    std::vector<pauta::Note> saved;
    for (auto* e : xml->getChildWithTagNameIterator ("Note"))
    {
        pauta::Note n;
        n.start = e->getIntAttribute ("s");
        n.end = e->getIntAttribute ("e");
        n.detectedMidi = (float) e->getDoubleAttribute ("d", 60.0);
        n.targetMidi = (float) e->getDoubleAttribute ("t", 60.0);
        n.variation = (float) e->getDoubleAttribute ("v", 1.0);
        n.shift = e->getIntAttribute ("sh");
        saved.push_back (n);
    }

    juce::WeakReference<LaPautaTuneProcessor> weak (this);
    juce::MessageManager::callAsync ([weak, path, k, sc, lk, gr, ca, bp, saved]
    {
        if (weak == nullptr)
            return;
        weak->key = k;
        weak->scaleType = sc;
        weak->lockScale = lk;
        weak->gridIndex = gr;
        weak->centerAmount = ca;
        weak->bpm.store (bp);
        if (path.isNotEmpty())
            weak->restoreState (path, saved);
        else
            weak->sendChangeMessage();
    });
}

//==============================================================================
juce::AudioProcessorEditor* LaPautaTuneProcessor::createEditor()
{
    return new LaPautaTuneEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LaPautaTuneProcessor();
}
