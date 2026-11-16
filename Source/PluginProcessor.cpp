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
    juce::int64 offset = 0;
    bool fromScan = false;
    std::shared_ptr<std::vector<float>> mono;
    std::shared_ptr<pauta::F0Track> track;
    std::vector<pauta::Note> notes;
    std::vector<float> peaks;
};

//==============================================================================
class LaPautaTuneProcessor::AnalysisThread : public juce::Thread
{
public:
    // Desde un archivo de audio
    AnalysisThread (LaPautaTuneProcessor& o, juce::File f, std::vector<pauta::Note> saved, int gen,
                    juce::int64 offsetSamples, bool scanProject)
        : juce::Thread ("La Pauta Tune analisis"), owner (o), file (std::move (f)),
          savedNotes (std::move (saved)), generation (gen), offset (offsetSamples), fromScan (scanProject)
    {
    }

    // Desde audio ya capturado del DAW (se guarda además como WAV para poder reabrir el proyecto)
    AnalysisThread (LaPautaTuneProcessor& o, juce::File f, std::shared_ptr<std::vector<float>> captured,
                    double sr, int gen, juce::int64 offsetSamples)
        : juce::Thread ("La Pauta Tune analisis"), owner (o), file (std::move (f)), generation (gen),
          offset (offsetSamples), fromScan (true), preloaded (std::move (captured)), preSampleRate (sr)
    {
    }

    void run() override
    {
        auto res = std::make_shared<LoadResult>();
        res->generation = generation;
        res->offset = offset;
        res->fromScan = fromScan;
        res->path = file.getFullPathName();
        res->name = file.getFileNameWithoutExtension();

        std::shared_ptr<std::vector<float>> monoData;

        if (preloaded != nullptr)
        {
            monoData = preloaded;
            res->sampleRate = preSampleRate;
            res->channels = 1;
            res->name = "Escaneo";
            writeScanFile (*monoData);
        }
        else
        {
            monoData = readFile (*res);
            if (monoData == nullptr)
            {
                if (! threadShouldExit())
                    post (res);
                return;
            }
        }

        // picos para dibujar la onda dentro de las notas
        {
            const size_t nb = (size_t) ((monoData->size() + (size_t) peakBlock - 1) / (size_t) peakBlock);
            res->peaks.assign (nb, 0.0f);
            float gmax = 1.0e-6f;
            for (size_t b = 0; b < nb; ++b)
            {
                const size_t a = b * (size_t) peakBlock;
                const size_t e = std::min (monoData->size(), a + (size_t) peakBlock);
                float m = 0.0f;
                for (size_t i = a; i < e; ++i)
                    m = std::max (m, std::fabs ((*monoData)[i]));
                res->peaks[b] = m;
                gmax = std::max (gmax, m);
            }
            for (auto& p : res->peaks)
                p /= gmax;
        }

        auto trk = std::make_shared<pauta::F0Track>();
        const bool finished = pauta::analyzePitch (*monoData, res->sampleRate, *trk, [this] (float p)
        {
            owner.loadProgress.store (p);
            return ! threadShouldExit();
        });

        if (! finished || threadShouldExit())
            return;

        res->mono = monoData;
        res->track = trk;
        res->notes = savedNotes.empty() ? pauta::segmentNotes (*trk, (int) monoData->size()) : savedNotes;
        res->ok = true;
        post (res);
    }

private:
    std::shared_ptr<std::vector<float>> readFile (LoadResult& res)
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));

        if (reader == nullptr || reader->lengthInSamples <= 0 || reader->numChannels == 0)
        {
            res.error = T ("No se pudo abrir el audio. Usa WAV, AIFF, FLAC, MP3 u OGG.");
            return nullptr;
        }

        res.sampleRate = reader->sampleRate;
        res.channels = (int) reader->numChannels;
        const juce::int64 total = reader->lengthInSamples;

        if (total > (juce::int64) 60 * 60 * (juce::int64) reader->sampleRate)
        {
            res.error = T ("El audio es demasiado largo (máximo 60 minutos).");
            return nullptr;
        }

        auto data = std::make_shared<std::vector<float>> ((size_t) total, 0.0f);
        const int chunk = 65536;
        juce::AudioBuffer<float> buf ((int) reader->numChannels, chunk);
        for (juce::int64 pos = 0; pos < total; pos += chunk)
        {
            if (threadShouldExit())
                return nullptr;
            const int len = (int) std::min<juce::int64> (chunk, total - pos);
            reader->read (&buf, 0, len, pos, true, true);
            for (int i = 0; i < len; ++i)
            {
                float s = 0.0f;
                for (int c = 0; c < buf.getNumChannels(); ++c)
                    s += buf.getSample (c, i);
                (*data)[(size_t) (pos + i)] = s / (float) buf.getNumChannels();
            }
        }
        return data;
    }

    void writeScanFile (const std::vector<float>& data)
    {
        file.getParentDirectory().createDirectory();
        std::unique_ptr<juce::FileOutputStream> stream (file.createOutputStream());
        if (stream == nullptr)
            return;

        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (stream.get(), preSampleRate, 1, 32, {}, 0));
        if (writer == nullptr)
            return;
        stream.release(); // el writer se queda con el stream

        const float* chans[1] = { data.data() };
        writer->writeFromFloatArrays (chans, 1, (int) data.size());
    }

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
    juce::int64 offset;
    bool fromScan;
    std::shared_ptr<std::vector<float>> preloaded;
    double preSampleRate = 44100.0;
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
    : juce::AudioProcessor (BusesProperties()
                                .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    renderThread = std::make_unique<RenderThread> (*this);
    renderThread->startThread();
    startTimerHz (10);
}

LaPautaTuneProcessor::~LaPautaTuneProcessor()
{
    stopTimer();

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
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void LaPautaTuneProcessor::timerCallback()
{
    if (scanFinishRequested.exchange (false))
        finishScan();
}

//==============================================================================
void LaPautaTuneProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    midi.clear();

    const int n = buffer.getNumSamples();
    const int nch = buffer.getNumChannels();

    bool hostPlaying = false;
    double hostSeconds = 0.0;
    juce::int64 hostSamples = 0;

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            hostPlaying = pos->getIsPlaying();
            if (auto t = pos->getTimeInSeconds())
                hostSeconds = *t;
            if (auto s = pos->getTimeInSamples())
                hostSamples = *s;
            else
                hostSamples = (juce::int64) std::llround (hostSeconds * hostSampleRate);
            if (auto b = pos->getBpm())
                if (*b > 0.0)
                    bpm.store (*b);
        }

    //--------------------------------------------------------------------------
    // 1) Escaneo: se captura el audio de la pista y se deja pasar sin cambios
    if (scanState.load() != 0)
    {
        if (hostPlaying)
        {
            const juce::SpinLock::ScopedTryLockType tl (captureLock);
            if (tl.isLocked() && ! captureBuf.empty())
            {
                juce::int64 base = capBase.load();
                if (base < 0)
                {
                    base = hostSamples;
                    capBase.store (base);
                }

                const juce::int64 idx = hostSamples - base;
                if (idx >= 0)
                {
                    if (idx + n <= (juce::int64) captureBuf.size())
                    {
                        const int nIn = std::max (1, std::min (getTotalNumInputChannels(), nch));
                        float* dst = captureBuf.data() + idx;
                        for (int i = 0; i < n; ++i)
                        {
                            float s = 0.0f;
                            for (int c = 0; c < nIn; ++c)
                                s += buffer.getSample (c, i);
                            dst[i] = s / (float) nIn;
                        }
                        capLen.store (std::max (capLen.load(), idx + n));
                        scanState.store (2);
                    }
                    else
                    {
                        scanFinishRequested.store (true); // se llenó el máximo
                    }
                }
            }
        }
        else if (capLen.load() > 0)
        {
            scanFinishRequested.store (true); // el DAW se detuvo: terminar
        }
        return;
    }

    //--------------------------------------------------------------------------
    // 2) Normal: la voz afinada reemplaza a la original en su sitio del proyecto
    std::shared_ptr<const std::vector<float>> buf;
    {
        const juce::SpinLock::ScopedLockType sl (bufLock);
        buf = playBuf;
    }

    const double srcSR = srcSampleRate.load();
    const juce::int64 offset = projectOffset.load();
    const bool fromScan = projectFromScan.load();
    const bool internalOnly = internalPlaying.load() && ! hostPlaying;
    const bool haveBuf = buf != nullptr && ! buf->empty();

    // Un proyecto cargado desde archivo suena solo; uno escaneado deja pasar la pista fuera de la voz
    if (haveBuf && (! fromScan || internalOnly))
        buffer.clear();

    double pos = 0.0;
    bool active = false;

    if (hostPlaying)
    {
        pos = (srcSR == hostSampleRate) ? (double) (hostSamples - offset)
                                        : hostSeconds * srcSR - (double) offset;
        active = true;
    }
    else if (internalOnly)
    {
        pos = internalPos.load();
        active = true;
    }

    const double ratio = srcSR / hostSampleRate;

    if (haveBuf && active)
    {
        const auto& v = *buf;
        const double size = (double) v.size();

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

        if (internalOnly)
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
    else if (internalOnly)
    {
        internalPlaying = false;
    }

    playPosSeconds.store (hostPlaying ? pos / srcSR : internalPos.load() / srcSR);
}

void LaPautaTuneProcessor::setPlayPositionSeconds (double s)
{
    s = std::max (0.0, s);
    internalPos.store (s * srcSampleRate.load());
    playPosSeconds.store (s);
}

//==============================================================================
void LaPautaTuneProcessor::startScan()
{
    if (scanState.load() != 0)
    {
        finishScan(); // segundo clic: terminar y analizar lo capturado
        return;
    }

    if (analysisThread != nullptr)
        analysisThread->stopThread (5000);
    loading = false;
    internalPlaying = false;

    std::vector<float> fresh ((size_t) ((double) maxScanSeconds * hostSampleRate), 0.0f);
    {
        const juce::SpinLock::ScopedLockType sl (captureLock);
        captureBuf.swap (fresh);
        capBase.store (-1);
        capLen.store (0);
        scanFinishRequested.store (false);
        scanState.store (1);
    }

    messageText = T ("Dale Play en tu DAW sobre la voz");
    sendChangeMessage();
}

void LaPautaTuneProcessor::finishScan()
{
    if (scanState.load() == 0)
        return;

    std::shared_ptr<std::vector<float>> captured;
    juce::int64 base = 0;

    {
        const juce::SpinLock::ScopedLockType sl (captureLock);
        scanState.store (0);
        const juce::int64 len = capLen.load();
        base = capBase.load();
        if (len > 0 && base >= 0)
            captured = std::make_shared<std::vector<float>> (captureBuf.begin(), captureBuf.begin() + (long) len);
        std::vector<float>().swap (captureBuf);
    }

    if (captured == nullptr || (double) captured->size() < 0.5 * hostSampleRate)
    {
        messageText = T ("No se capturó audio. Pulsa Escanear, luego Play en el DAW sobre la voz y deja que suene.");
        sendChangeMessage();
        return;
    }

    auto folder = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                      .getChildFile ("La Pauta Tune").getChildFile ("Escaneos");
    const auto file = folder.getChildFile ("Escaneo_" + juce::Time::getCurrentTime().formatted ("%Y%m%d_%H%M%S") + ".wav");

    if (analysisThread != nullptr)
        analysisThread->stopThread (5000);

    ++loadGeneration;
    loading = true;
    loadProgress = 0.0f;
    messageText = T ("Analizando la voz…");
    sendChangeMessage();

    analysisThread = std::make_unique<AnalysisThread> (*this, file, captured, hostSampleRate, loadGeneration, base);
    analysisThread->startThread();
}

//==============================================================================
void LaPautaTuneProcessor::loadFile (const juce::File& file)
{
    if (scanState.load() != 0)
    {
        const juce::SpinLock::ScopedLockType sl (captureLock);
        scanState.store (0);
        std::vector<float>().swap (captureBuf);
    }

    if (analysisThread != nullptr)
        analysisThread->stopThread (5000);

    ++loadGeneration;
    loading = true;
    loadProgress = 0.0f;
    messageText = T ("Analizando la voz…");
    sendChangeMessage();

    analysisThread = std::make_unique<AnalysisThread> (*this, file, std::vector<pauta::Note>(), loadGeneration, 0, false);
    analysisThread->startThread();
}

void LaPautaTuneProcessor::restoreState (const juce::String& path, std::vector<pauta::Note> savedNotes,
                                         juce::int64 offset, bool fromScan)
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

    analysisThread = std::make_unique<AnalysisThread> (*this, f, std::move (savedNotes), loadGeneration, offset, fromScan);
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
    srcChannels = r.channels;
    projectName = r.name;
    projectPath = r.path;
    undoStack.clear();

    srcSampleRate.store (r.sampleRate);
    projectOffset.store (r.offset);
    projectFromScan.store (r.fromScan);

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
    auto stamp = juce::Time::getCurrentTime().formatted ("%H%M%S");
    juce::String startTag;
    if (projectOffset.load() > 0)
        startTag = "_inicio" + juce::String (getTimeOffsetSeconds(), 2) + "s";
    auto file = folder.getChildFile (safeName + "_afinada" + startTag + "_" + stamp + ".wav");

    const int nch = std::max (1, srcChannels);
    juce::AudioBuffer<float> buf (nch, (int) out.size());
    for (int c = 0; c < nch; ++c)
        buf.copyFrom (c, 0, out.data(), (int) out.size());

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::FileOutputStream> stream (file.createOutputStream());
    if (stream == nullptr)
        return {};

    std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (stream.get(), srcSampleRate.load(), (unsigned int) nch, 24, {}, 0));
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
    xml.setAttribute ("offset", juce::String (projectOffset.load()));
    xml.setAttribute ("scan", projectFromScan.load());
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
    const juce::int64 offset = xml->getStringAttribute ("offset", "0").getLargeIntValue();
    const bool fromScan = xml->getBoolAttribute ("scan", false);
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
    juce::MessageManager::callAsync ([weak, path, offset, fromScan, k, sc, lk, gr, ca, bp, saved]
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
            weak->restoreState (path, saved, offset, fromScan);
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
