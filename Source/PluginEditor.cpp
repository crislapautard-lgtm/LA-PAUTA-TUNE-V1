#include "PluginEditor.h"

namespace
{
    void setupLabel (juce::Label& l, const juce::String& text, juce::Justification j = juce::Justification::centredRight)
    {
        l.setText (text, juce::dontSendNotification);
        l.setJustificationType (j);
        l.setInterceptsMouseClicks (false, false);
        l.setFont (juce::FontOptions (13.0f));
    }
}

//==============================================================================
LaPautaTuneEditor::LaPautaTuneEditor (LaPautaTuneProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p), roll (p), dragIcon (p)
{
    setLookAndFeel (&laf);

    addAndMakeVisible (roll);
    addAndMakeVisible (dragIcon);

    for (auto* b : { &scanBtn, &fileBtn, &playBtn, &stopBtn, &exportBtn, &toolPitchBtn, &toolTimeBtn, &toolCutBtn,
                     &lockBtn, &centerBtn, &resetBtn, &undoBtn })
        addAndMakeVisible (b);

    for (auto* c : { &keyBox, &scaleBox, &gridBox })
        addAndMakeVisible (c);

    for (auto* s : { &centerKnob, &variationKnob, &bpmSlider, &zoomSlider })
        addAndMakeVisible (s);

    for (auto* l : { &keyLabel, &scaleLabel, &gridLabel, &bpmLabel, &zoomLabel, &centerLabel, &variationLabel, &statusLabel })
        addAndMakeVisible (l);

    setupLabel (keyLabel, "Tonalidad");
    setupLabel (scaleLabel, "Escala");
    setupLabel (gridLabel, "Rejilla");
    setupLabel (bpmLabel, "BPM");
    setupLabel (zoomLabel, "Zoom");
    setupLabel (centerLabel, "Centro", juce::Justification::centred);
    setupLabel (variationLabel, T ("Variación"), juce::Justification::centred);
    setupLabel (statusLabel, "", juce::Justification::centredLeft);
    statusLabel.setColour (juce::Label::textColourId, juce::Colour (0xff9fb0bf));

    // --- carga / reproducción
    scanBtn.onClick = [this] { proc.startScan(); updateStatus(); };
    scanBtn.setTooltip (T ("Escanea la voz de tu proyecto: pulsa Escanear, dale Play en el DAW sobre la voz "
                           "(desde donde empieza) y al detener el DAW aparecen las notas para afinar. "
                           "Pon el plugin en la pista de la voz"));

    fileBtn.onClick = [this] { chooseFile(); };
    fileBtn.setTooltip (T ("Abrir un audio de voz desde un archivo (también puedes arrastrarlo a la ventana)"));

    playBtn.onClick = [this]
    {
        if (proc.isPlayingInternal())
            proc.pausePlayback();
        else
            proc.play();
    };
    stopBtn.onClick = [this] { proc.stopPlayback(); };

    exportBtn.setTooltip (T ("Guarda la voz afinada como WAV en Documentos/La Pauta Tune"));
    exportBtn.onClick = [this]
    {
        if (! proc.hasProject())
            return;
        const auto f = proc.exportCorrectedAudio();
        if (f.existsAsFile())
            f.revealToUser();
    };

    // --- herramientas
    for (auto* b : { &toolPitchBtn, &toolTimeBtn, &toolCutBtn })
    {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (1001);
    }
    toolPitchBtn.setToggleState (true, juce::dontSendNotification);
    toolPitchBtn.setTooltip (T ("Arrastra una nota hacia arriba o abajo para cambiar su tono. Doble clic: llevarla a la escala"));
    toolTimeBtn.setTooltip (T ("Arrastra una nota de lado para moverla en el tiempo (se pega a la rejilla)"));
    toolCutBtn.setTooltip (T ("Haz clic sobre una nota para partirla en dos (se pega a la rejilla)"));
    toolPitchBtn.onClick = [this] { roll.setTool (PianoRoll::Tool::pitch); };
    toolTimeBtn.onClick  = [this] { roll.setTool (PianoRoll::Tool::time); };
    toolCutBtn.onClick   = [this] { roll.setTool (PianoRoll::Tool::cut); };
    roll.setTool (PianoRoll::Tool::pitch);

    // --- escala
    for (int i = 0; i < 12; ++i)
        keyBox.addItem (pauta::noteNames()[i], i + 1);
    keyBox.onChange = [this] { proc.key = keyBox.getSelectedId() - 1; roll.repaint(); };
    keyBox.setTooltip (T ("Tonalidad del beat"));

    const auto& types = pauta::scaleTypes();
    for (size_t i = 0; i < types.size(); ++i)
        scaleBox.addItem (T (types[i].name), (int) i + 1);
    scaleBox.onChange = [this] { proc.scaleType = scaleBox.getSelectedId() - 1; roll.repaint(); };
    scaleBox.setTooltip (T ("Escala del beat: las voces se llevan a las notas de esta escala"));

    lockBtn.setClickingTogglesState (true);
    lockBtn.setTooltip (T ("Bloquea las notas que no están en la escala: no podrás arrastrar una nota hasta ellas"));
    lockBtn.onClick = [this] { proc.lockScale = lockBtn.getToggleState(); roll.repaint(); };

    centerBtn.setTooltip (T ("Lleva las notas (las seleccionadas, o todas) a la nota más cercana de la escala"));
    centerBtn.onClick = [this]
    {
        if (! proc.hasProject())
            return;
        proc.pushUndo();
        proc.centerNotes();
        roll.repaint();
    };

    resetBtn.setTooltip (T ("Devuelve las notas a como se cantaron"));
    resetBtn.onClick = [this]
    {
        if (! proc.hasProject())
            return;
        proc.pushUndo();
        proc.resetNotes();
        refreshVariationKnob();
        roll.repaint();
    };

    undoBtn.onClick = [this] { proc.undo(); roll.repaint(); };

    // --- rejilla / bpm / zoom
    const char* gridNames[7] = { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "Sin rejilla" };
    for (int i = 0; i < 7; ++i)
        gridBox.addItem (gridNames[i], i + 1);
    gridBox.onChange = [this] { proc.gridIndex = gridBox.getSelectedId() - 1; roll.repaint(); };
    gridBox.setTooltip (T ("Rejilla para mover y cortar notas"));

    bpmSlider.setSliderStyle (juce::Slider::LinearBar);
    bpmSlider.setRange (40.0, 300.0, 0.01);
    bpmSlider.setNumDecimalPlacesToDisplay (1);
    bpmSlider.setTextBoxIsEditable (true);
    bpmSlider.setTooltip (T ("Tempo del proyecto (se toma del DAW si lo envía). Doble clic para escribirlo"));
    bpmSlider.onValueChange = [this] { proc.bpm.store (bpmSlider.getValue()); roll.repaint(); };

    zoomSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    zoomSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    zoomSlider.setRange (15.0, 600.0, 0.1);
    zoomSlider.setSkewFactorFromMidPoint (100.0);
    zoomSlider.setValue (100.0, juce::dontSendNotification);
    zoomSlider.onValueChange = [this] { roll.setPixelsPerSecond (zoomSlider.getValue()); };
    roll.onZoomChanged = [this] (double pps) { zoomSlider.setValue (pps, juce::dontSendNotification); };

    // --- perillas
    centerKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    centerKnob.setRange (0.0, 100.0, 1.0);
    centerKnob.setValue (100.0, juce::dontSendNotification);
    centerKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 16);
    centerKnob.setTextValueSuffix (" %");
    centerKnob.setTooltip (T ("Fuerza del botón Centrar: 100 % = nota exacta de la escala, menos = más natural"));
    centerKnob.onValueChange = [this] { proc.centerAmount = (float) centerKnob.getValue() / 100.0f; };

    variationKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    variationKnob.setRange (0.0, 100.0, 1.0);
    variationKnob.setValue (100.0, juce::dontSendNotification);
    variationKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 16);
    variationKnob.setTextValueSuffix (" %");
    variationKnob.setTooltip (T ("Variación dentro de la nota (vibrato, deriva). 0 % = nota plana. Afecta a las notas seleccionadas o a todas"));
    variationKnob.onDragStart = [this] { proc.pushUndo(); };
    variationKnob.onValueChange = [this]
    {
        proc.applyVariation ((float) variationKnob.getValue() / 100.0f);
        roll.repaint();
    };

    roll.onSelectionChanged = [this] { refreshVariationKnob(); };

    setResizable (true, true);
    setResizeLimits (940, 480, 2600, 1500);
    setSize (1060, 640);

    proc.addChangeListener (this);
    startTimerHz (30);
    refreshFromProcessor();
    roll.projectChanged();
    updateStatus();
}

LaPautaTuneEditor::~LaPautaTuneEditor()
{
    stopTimer();
    proc.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

//==============================================================================
void LaPautaTuneEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff262f38));

    g.setColour (juce::Colour (0xff10151b));
    g.fillRect (0, 111, getWidth(), 1);

    g.setColour (juce::Colour (0xffe5953d));
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText ("LA PAUTA  TUNE", getWidth() - 140, 2, 130, 14, juce::Justification::centredRight);
}

void LaPautaTuneEditor::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (112).reduced (10, 6);
    roll.setBounds (r);

    auto right = header.removeFromRight (290);
    header.removeFromRight (10);

    auto row1 = header.removeFromTop (28);
    header.removeFromTop (4);
    auto row2 = header.removeFromTop (28);
    header.removeFromTop (4);
    auto row3 = header.removeFromTop (28);

    auto place = [] (juce::Rectangle<int>& row, juce::Component& c, int w, int gap = 6)
    {
        c.setBounds (row.removeFromLeft (w));
        row.removeFromLeft (gap);
    };

    place (row1, scanBtn, 100);
    place (row1, fileBtn, 70);
    place (row1, playBtn, 54, 4);
    place (row1, stopBtn, 54);
    place (row1, exportBtn, 110, 22);
    place (row1, toolPitchBtn, 66, 2);
    place (row1, toolTimeBtn, 66, 2);
    place (row1, toolCutBtn, 66);

    place (row2, keyLabel, 64, 4);
    place (row2, keyBox, 64, 10);
    place (row2, scaleLabel, 46, 4);
    place (row2, scaleBox, 150, 14);
    place (row2, lockBtn, 124);
    place (row2, centerBtn, 72);
    place (row2, resetBtn, 92);
    place (row2, undoBtn, 76);

    place (row3, gridLabel, 46, 4);
    place (row3, gridBox, 100, 14);
    place (row3, bpmLabel, 34, 4);
    place (row3, bpmSlider, 86, 14);
    place (row3, zoomLabel, 40, 4);
    place (row3, zoomSlider, 130, 14);
    statusLabel.setBounds (row3);

    centerKnob.setBounds (right.getX(), right.getY(), 84, 78);
    variationKnob.setBounds (right.getX() + 90, right.getY(), 84, 78);
    centerLabel.setBounds (right.getX(), right.getY() + 80, 84, 18);
    variationLabel.setBounds (right.getX() + 90, right.getY() + 80, 84, 18);
    dragIcon.setBounds (right.getRight() - 104, right.getY() + 14, 104, 90);
}

//==============================================================================
void LaPautaTuneEditor::chooseFile()
{
    chooser = std::make_unique<juce::FileChooser> (T ("Elige la voz"),
                                                   juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
                                                   "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (f.existsAsFile())
            proc.loadFile (f);
    });
}

bool LaPautaTuneEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
    {
        const auto ext = juce::File (f).getFileExtension().toLowerCase();
        if (ext == ".wav" || ext == ".aif" || ext == ".aiff" || ext == ".flac" || ext == ".mp3" || ext == ".ogg")
            return true;
    }
    return false;
}

void LaPautaTuneEditor::filesDropped (const juce::StringArray& files, int, int)
{
    for (const auto& f : files)
        if (isInterestedInFileDrag (juce::StringArray (f)))
        {
            proc.loadFile (juce::File (f));
            return;
        }
}

//==============================================================================
void LaPautaTuneEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    roll.projectChanged();
    refreshFromProcessor();
    updateStatus();
}

void LaPautaTuneEditor::refreshFromProcessor()
{
    keyBox.setSelectedId (proc.key + 1, juce::dontSendNotification);
    scaleBox.setSelectedId (proc.scaleType + 1, juce::dontSendNotification);
    gridBox.setSelectedId (proc.gridIndex + 1, juce::dontSendNotification);
    lockBtn.setToggleState (proc.lockScale, juce::dontSendNotification);
    centerKnob.setValue ((double) proc.centerAmount * 100.0, juce::dontSendNotification);
    bpmSlider.setValue (proc.bpm.load(), juce::dontSendNotification);
    refreshVariationKnob();
}

void LaPautaTuneEditor::refreshVariationKnob()
{
    auto& notes = proc.getNotes();
    const bool onlySel = proc.anySelected();

    double sum = 0.0;
    int count = 0;
    for (const auto& n : notes)
        if (! onlySel || n.selected)
        {
            sum += (double) n.variation;
            ++count;
        }

    variationKnob.setValue (count > 0 ? 100.0 * sum / (double) count : 100.0, juce::dontSendNotification);
}

void LaPautaTuneEditor::updateStatus()
{
    const auto scan = proc.getScanState();
    if (scan == LaPautaTuneProcessor::ScanState::armed)
        statusLabel.setText (T ("Dale Play en tu DAW sobre la voz"), juce::dontSendNotification);
    else if (scan == LaPautaTuneProcessor::ScanState::capturing)
        statusLabel.setText (T ("Escaneando… ") + juce::String (proc.getScanSeconds(), 1) + T (" s (detén el DAW)"),
                             juce::dontSendNotification);
    else if (proc.isLoading())
        statusLabel.setText (T ("Analizando la voz… ") + juce::String ((int) (proc.getLoadProgress() * 100.0f)) + " %",
                             juce::dontSendNotification);
    else
        statusLabel.setText (proc.getMessageText(), juce::dontSendNotification);
}

void LaPautaTuneEditor::timerCallback()
{
    roll.setPlayheadSeconds (proc.getPlayPositionSeconds());

    const auto wanted = proc.isPlayingInternal() ? juce::String ("Pausa") : juce::String ("Play");
    if (playBtn.getButtonText() != wanted)
        playBtn.setButtonText (wanted);

    const bool scanning = proc.getScanState() != LaPautaTuneProcessor::ScanState::idle;
    if (scanBtn.getToggleState() != scanning)
        scanBtn.setToggleState (scanning, juce::dontSendNotification);
    const auto scanText = scanning ? juce::String ("Detener") : juce::String ("Escanear");
    if (scanBtn.getButtonText() != scanText)
        scanBtn.setButtonText (scanText);

    if (proc.isLoading() || scanning)
        updateStatus();

    const double b = proc.bpm.load();
    if (std::abs (bpmSlider.getValue() - b) > 0.02 && ! bpmSlider.isMouseButtonDown())
    {
        bpmSlider.setValue (b, juce::dontSendNotification);
        roll.repaint();
    }
}
