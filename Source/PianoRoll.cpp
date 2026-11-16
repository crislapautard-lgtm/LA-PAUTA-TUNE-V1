#include "PianoRoll.h"

namespace
{
    const juce::Colour kBg       { 0xff1b232b };
    const juce::Colour kOrange   { 0xffe5953d };
    const juce::Colour kRuler    { 0xff121820 };

    bool isBlackKey (int midi)
    {
        const int p = pauta::mod12 (midi);
        return p == 1 || p == 3 || p == 6 || p == 8 || p == 10;
    }

    juce::String midiName (float m)
    {
        const int r = (int) std::lround (m);
        const int cents = (int) std::lround ((m - (float) r) * 100.0f);
        juce::String s = juce::String (pauta::noteNames()[pauta::mod12 (r)]) + juce::String (r / 12 - 1);
        if (cents != 0)
            s << (cents > 0 ? " +" : " ") << cents << "c";
        return s;
    }
}

//==============================================================================
PianoRoll::PianoRoll (LaPautaTuneProcessor& p) : proc (p)
{
    setWantsKeyboardFocus (true);
    addAndMakeVisible (hBar);
    addAndMakeVisible (vBar);
    hBar.addListener (this);
    vBar.addListener (this);
    hBar.setAutoHide (false);
    vBar.setAutoHide (false);
}

void PianoRoll::setTool (Tool t)
{
    tool = t;
    cutHoverIndex = -1;
    setMouseCursor (t == Tool::cut ? juce::MouseCursor::CrosshairCursor
                    : t == Tool::time ? juce::MouseCursor::LeftRightResizeCursor
                    : juce::MouseCursor::UpDownResizeCursor);
    repaint();
}

//==============================================================================
double PianoRoll::totalSeconds() const
{
    return proc.hasProject() ? (double) proc.getNumSourceSamples() / sr() : 10.0;
}

juce::Rectangle<int> PianoRoll::contentArea() const
{
    return { keyW, rulerH, std::max (1, getWidth() - keyW - barSize), std::max (1, getHeight() - rulerH - barSize) };
}

int PianoRoll::visibleRows() const
{
    return std::max (1, contentArea().getHeight() / rowH);
}

void PianoRoll::clampScroll()
{
    const double visSec = contentArea().getWidth() / pps;
    scrollX = juce::jlimit (0.0, std::max (0.0, totalSeconds() + 2.0 - visSec), scrollX);

    const double lo = (double) (midiLo + visibleRows());
    const double hi = (double) (midiHi + 1);
    topMidi = std::round (juce::jlimit (std::min (lo, hi), hi, topMidi));
}

void PianoRoll::updateScrollbars()
{
    const auto c = contentArea();
    const double visSec = c.getWidth() / pps;
    hBar.setRangeLimits (0.0, std::max (totalSeconds() + 2.0, visSec));
    hBar.setCurrentRange (scrollX, visSec, juce::dontSendNotification);

    vBar.setRangeLimits (0.0, (double) (midiHi - midiLo + 1));
    vBar.setCurrentRange ((double) (midiHi + 1) - topMidi, (double) visibleRows(), juce::dontSendNotification);
}

void PianoRoll::scrollBarMoved (juce::ScrollBar* b, double newStart)
{
    if (b == &hBar)
        scrollX = newStart;
    else
        topMidi = (double) (midiHi + 1) - newStart;
    clampScroll();
    repaint();
}

void PianoRoll::resized()
{
    hBar.setBounds (keyW, getHeight() - barSize, std::max (1, getWidth() - keyW - barSize), barSize);
    vBar.setBounds (getWidth() - barSize, rulerH, barSize, std::max (1, getHeight() - rulerH - barSize));
    clampScroll();
    updateScrollbars();
}

void PianoRoll::setPixelsPerSecond (double v, double anchorX)
{
    v = juce::jlimit (15.0, 1500.0, v);
    const double ax = anchorX >= 0.0 ? anchorX : (double) keyW + contentArea().getWidth() * 0.5;
    const double tAnchor = (ax - keyW) / pps + scrollX;
    pps = v;
    scrollX = tAnchor - (ax - keyW) / pps;
    clampScroll();
    updateScrollbars();
    repaint();
    if (onZoomChanged)
        onZoomChanged (pps);
}

void PianoRoll::setPlayheadSeconds (double s)
{
    if (std::abs (s - playheadSec) > 1.0e-4)
    {
        playheadSec = s;
        repaint();
    }
}

void PianoRoll::projectChanged()
{
    const int n = proc.getNumSourceSamples();
    if (n != lastProjectSamples || proc.getSourceSampleRate() != lastProjectRate)
    {
        lastProjectSamples = n;
        lastProjectRate = proc.getSourceSampleRate();

        if (n > 0)
        {
            // ajusta el zoom a la voz y centra la altura en las notas
            const double dur = totalSeconds();
            pps = juce::jlimit (40.0, 200.0, contentArea().getWidth() / std::max (0.5, dur));
            scrollX = 0.0;

            std::vector<float> m;
            for (const auto& nt : proc.getNotes())
                m.push_back (nt.detectedMidi);
            double centre = 60.0;
            if (! m.empty())
            {
                std::sort (m.begin(), m.end());
                centre = (double) m[m.size() / 2];
            }
            topMidi = std::round (centre + visibleRows() * 0.5);
            clampScroll();
            updateScrollbars();
            if (onZoomChanged)
                onZoomChanged (pps);
        }
    }
    repaint();
}

double PianoRoll::gridSeconds() const
{
    const double div = LaPautaTuneProcessor::gridDivision (proc.gridIndex);
    if (div <= 0.0)
        return 0.0;
    const double beat = 60.0 / std::max (20.0, proc.bpm.load());
    return beat * 4.0 / div;
}

double PianoRoll::snapTimelineSample (double s) const
{
    const double g = gridSeconds() * sr();
    if (g < 1.0)
        return s;
    return std::round (s / g) * g;
}

//==============================================================================
juce::Rectangle<float> PianoRoll::noteRect (const pauta::Note& n) const
{
    const float x0 = xOfSample ((double) n.start + (double) n.shift);
    const float x1 = xOfSample ((double) n.end + (double) n.shift);
    const float h = (float) rowH - 2.0f;
    return { x0, yOfMidi (n.targetMidi) - h * 0.5f, std::max (3.0f, x1 - x0), h };
}

int PianoRoll::hitTest (juce::Point<float> p) const
{
    const auto c = contentArea().toFloat();
    if (! c.contains (p))
        return -1;

    auto& notes = proc.getNotes();
    for (int i = (int) notes.size() - 1; i >= 0; --i)
        if (noteRect (notes[(size_t) i]).expanded (0.0f, 2.0f).contains (p))
            return i;
    return -1;
}

//==============================================================================
void PianoRoll::paint (juce::Graphics& g)
{
    g.fillAll (kBg);
    const auto content = contentArea();

    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (content);
        drawRows (g, content);
        drawGrid (g, content);
        drawNotes (g, content);

        // vista previa del corte
        if (tool == Tool::cut && cutHoverIndex >= 0 && cutHoverIndex < (int) proc.getNotes().size())
        {
            const auto r = noteRect (proc.getNotes()[(size_t) cutHoverIndex]);
            const float x = xOfSample (cutHoverSample);
            g.setColour (juce::Colours::white);
            g.drawLine (x, r.getY() - 4.0f, x, r.getBottom() + 4.0f, 1.5f);
        }

        // cabezal de reproducción
        const float px = xOfSeconds (playheadSec);
        if (px >= (float) keyW && px <= (float) content.getRight())
        {
            g.setColour (juce::Colour (0xffff5a3c));
            g.drawVerticalLine ((int) px, (float) rulerH, (float) getHeight());
        }

        if (! proc.hasProject())
        {
            g.setColour (juce::Colours::white.withAlpha (0.45f));
            g.setFont (juce::FontOptions (18.0f));
            g.drawFittedText (proc.isLoading() ? T ("Analizando la voz…")
                                               : T ("Arrastra una voz aquí o pulsa “Cargar voz”"),
                              content, juce::Justification::centred, 2);
        }
    }

    drawKeys (g);
    drawRuler (g);
}

void PianoRoll::drawRows (juce::Graphics& g, juce::Rectangle<int> content)
{
    const auto mask = proc.getScaleMask();
    const bool lock = proc.lockScale;

    for (int m = midiLo; m <= midiHi; ++m)
    {
        const int y = (int) std::lround (yTopOfRow (m));
        if (y + rowH < content.getY() || y > content.getBottom())
            continue;

        const bool inScale = pauta::isInScale (m, mask);
        juce::Colour c = isBlackKey (m) ? juce::Colour (0xff18202a) : juce::Colour (0xff202a35);
        if (inScale)
            c = c.brighter (0.25f);
        if (pauta::mod12 (m) == proc.key)
            c = c.interpolatedWith (kOrange, 0.16f);

        g.setColour (c);
        g.fillRect (content.getX(), y, content.getWidth(), rowH);

        if (lock && ! inScale)
        {
            g.setColour (juce::Colour (0xb00a0e13));
            g.fillRect (content.getX(), y, content.getWidth(), rowH);
            g.setColour (juce::Colour (0x1fffffff));
            for (int x = content.getX() - rowH; x < content.getRight(); x += 9)
                g.drawLine ((float) x, (float) (y + rowH), (float) (x + rowH), (float) y, 1.0f);
        }

        g.setColour (pauta::mod12 (m) == 0 ? juce::Colour (0x44ffffff) : juce::Colour (0x14ffffff));
        g.drawHorizontalLine (y + rowH - 1, (float) content.getX(), (float) content.getRight());
    }
}

void PianoRoll::drawGrid (juce::Graphics& g, juce::Rectangle<int> content)
{
    const double beatSec = 60.0 / std::max (20.0, proc.bpm.load());
    const double barSec = beatSec * 4.0;
    double step = gridSeconds() > 0.0 ? gridSeconds() : beatSec;
    while (step * pps < 6.0)
        step *= 2.0;

    const double t0 = scrollX, t1 = scrollX + content.getWidth() / pps;
    const auto k0 = (long long) std::floor (t0 / step);

    for (long long k = k0; (double) k * step <= t1; ++k)
    {
        const double t = (double) k * step;
        const float x = xOfSeconds (t);

        const double nb = t / barSec, nbt = t / beatSec;
        const bool isBar = std::abs (nb - std::round (nb)) < 1.0e-6;
        const bool isBeat = std::abs (nbt - std::round (nbt)) < 1.0e-6;

        g.setColour (isBar ? juce::Colour (0x66ffffff) : isBeat ? juce::Colour (0x34ffffff) : juce::Colour (0x16ffffff));
        g.drawVerticalLine ((int) x, (float) content.getY(), (float) content.getBottom());
    }
}

void PianoRoll::drawNotes (juce::Graphics& g, juce::Rectangle<int> content)
{
    auto& notes = proc.getNotes();
    const auto& peaks = proc.getPeaks();
    const auto mask = proc.getScaleMask();

    for (const auto& n : notes)
    {
        const auto r = noteRect (n);
        if (r.getRight() < (float) content.getX() || r.getX() > (float) content.getRight()
            || r.getBottom() < (float) content.getY() || r.getY() > (float) content.getBottom())
            continue;

        // fantasma en la posición original si la nota fue modificada
        if (std::abs (n.targetMidi - n.detectedMidi) > 0.05f || n.shift != 0)
        {
            auto ghost = r;
            ghost.setY (yOfMidi (n.detectedMidi) - r.getHeight() * 0.5f);
            ghost.setX (xOfSample ((double) n.start));
            ghost.setWidth (std::max (3.0f, xOfSample ((double) n.end) - ghost.getX()));
            g.setColour (kOrange.withAlpha (0.28f));
            g.drawRoundedRectangle (ghost, 2.0f, 1.0f);
        }

        g.setColour (n.selected ? juce::Colour (0xff5b7690) : juce::Colour (0xff384a5b));
        g.fillRoundedRectangle (r, 3.0f);

        // forma de onda dentro de la nota
        if (! peaks.empty())
        {
            const int xa = std::max ((int) std::floor (r.getX()), content.getX());
            const int xb = std::min ((int) std::ceil (r.getRight()), content.getRight());
            const float cy = r.getCentreY();
            const float half = r.getHeight() * 0.5f - 1.5f;
            g.setColour (juce::Colour (0xffdbe2e8));

            for (int x = xa; x < xb; ++x)
            {
                const double sA = sampleOfX ((double) x) - (double) n.shift;
                const double sB = sampleOfX ((double) x + 1.0) - (double) n.shift;
                if (sB < 0.0)
                    continue;
                const size_t b0 = (size_t) std::max (0.0, sA / LaPautaTuneProcessor::peakBlock);
                size_t b1 = (size_t) std::max (0.0, sB / LaPautaTuneProcessor::peakBlock);
                if (b0 >= peaks.size())
                    continue;
                b1 = std::min (b1, peaks.size() - 1);

                float m = 0.0f;
                for (size_t b = b0; b <= b1; ++b)
                    m = std::max (m, peaks[b]);
                const float a = std::sqrt (m) * half;
                g.drawVerticalLine (x, cy - a, cy + a + 1.0f);
            }
        }

        const bool outOfScale = proc.lockScale && ! pauta::isInScale ((int) std::lround (n.targetMidi), mask);
        g.setColour (n.selected ? juce::Colours::white : outOfScale ? juce::Colour (0xffd9534f) : kOrange);
        g.drawRoundedRectangle (r, 3.0f, n.selected ? 2.0f : 1.4f);

        if (dragging && n.selected)
        {
            g.setColour (juce::Colours::white);
            g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
            g.drawText (midiName (n.targetMidi), (int) r.getX(), (int) r.getY() - 15, 80, 14, juce::Justification::left);
        }
    }
}

void PianoRoll::drawKeys (juce::Graphics& g)
{
    const auto mask = proc.getScaleMask();
    const auto keys = juce::Rectangle<int> (0, rulerH, keyW, contentArea().getHeight());

    juce::Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (keys);
    g.fillAll (juce::Colour (0xff10151b));

    for (int m = midiLo; m <= midiHi; ++m)
    {
        const int y = (int) std::lround (yTopOfRow (m));
        if (y + rowH < rulerH || y > getHeight())
            continue;

        g.setColour (juce::Colour (0xffe6e9ec));
        g.fillRect (0, y, keyW, rowH);
        if (isBlackKey (m))
        {
            g.setColour (juce::Colour (0xff1a1d21));
            g.fillRect (0, y + 1, (int) (keyW * 0.64f), rowH - 2);
        }
        g.setColour (juce::Colour (0xff8a9199));
        g.drawHorizontalLine (y + rowH - 1, 0.0f, (float) keyW);

        if (pauta::isInScale (m, mask))
        {
            g.setColour (pauta::mod12 (m) == proc.key ? kOrange : juce::Colour (0xff3fb6d6));
            g.fillEllipse ((float) keyW - 10.0f, (float) y + (float) rowH * 0.5f - 3.0f, 6.0f, 6.0f);
        }

        if (pauta::mod12 (m) == 0)
        {
            g.setColour (juce::Colour (0xff222831));
            g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
            g.drawText ("C" + juce::String (m / 12 - 1), 3, y, keyW - 14, rowH, juce::Justification::centredLeft);
        }
    }
}

void PianoRoll::drawRuler (juce::Graphics& g)
{
    juce::Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (keyW, 0, getWidth() - keyW, rulerH);
    g.fillAll (kRuler);

    const double barSec = 60.0 / std::max (20.0, proc.bpm.load()) * 4.0;
    const double t1 = scrollX + (getWidth() - keyW) / pps;
    const auto k0 = (long long) std::floor (scrollX / barSec);

    g.setFont (juce::FontOptions (12.0f));
    for (long long k = k0; (double) k * barSec <= t1; ++k)
    {
        const float x = xOfSeconds ((double) k * barSec);
        g.setColour (juce::Colour (0x88ffffff));
        g.drawVerticalLine ((int) x, 4.0f, (float) rulerH);
        g.setColour (juce::Colour (0xffc9d1d9));
        g.drawText (juce::String (k + 1), (int) x + 4, 0, 40, rulerH, juce::Justification::centredLeft);
    }

    // marca de reproducción
    const float px = xOfSeconds (playheadSec);
    if (px >= (float) keyW)
    {
        juce::Path tri;
        tri.addTriangle (px - 5.0f, 2.0f, px + 5.0f, 2.0f, px, 11.0f);
        g.setColour (juce::Colour (0xffff5a3c));
        g.fillPath (tri);
    }

    g.setColour (juce::Colour (0xff000000).withAlpha (0.5f));
    g.drawHorizontalLine (rulerH - 1, (float) keyW, (float) getWidth());
}

//==============================================================================
void PianoRoll::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    dragging = false;

    if (e.x < keyW)
        return;

    if (e.y < rulerH)
    {
        proc.setPlayPositionSeconds (std::max (0.0, sampleOfX ((double) e.x) / sr()));
        return;
    }

    if (! proc.hasProject())
        return;

    auto& notes = proc.getNotes();
    const int idx = hitTest (e.position);

    if (tool == Tool::cut)
    {
        if (idx >= 0)
        {
            const double snapped = snapTimelineSample (sampleOfX ((double) e.x));
            const int at = (int) std::lround (snapped - (double) notes[(size_t) idx].shift);
            if (proc.cutNote (idx, at))
            {
                cutHoverIndex = -1;
                if (onSelectionChanged)
                    onSelectionChanged();
            }
            repaint();
        }
        return;
    }

    if (idx < 0)
    {
        if (! e.mods.isShiftDown())
            proc.selectAll (false);
        if (onSelectionChanged)
            onSelectionChanged();
        repaint();
        return;
    }

    auto& hit = notes[(size_t) idx];
    if (e.mods.isShiftDown())
        hit.selected = ! hit.selected;
    else if (! hit.selected)
    {
        proc.selectAll (false);
        hit.selected = true;
    }

    if (onSelectionChanged)
        onSelectionChanged();

    // guardar para deshacer y recordar el estado inicial
    proc.pushUndo();
    if (onBeginEdit)
        onBeginEdit();

    initTarget.assign (notes.size(), 0.0f);
    initShift.assign (notes.size(), 0);
    for (size_t i = 0; i < notes.size(); ++i)
    {
        initTarget[i] = notes[i].targetMidi;
        initShift[i] = notes[i].shift;
    }

    downPos = e.getPosition();
    dragging = true;
    repaint();
}

void PianoRoll::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging)
        return;

    auto& notes = proc.getNotes();
    if (initTarget.size() != notes.size())
        return;

    const auto mask = proc.getScaleMask();
    const int N = proc.getNumSourceSamples();

    if (tool == Tool::pitch)
    {
        const double dyRows = -(double) (e.y - downPos.y) / rowH;
        for (size_t i = 0; i < notes.size(); ++i)
        {
            if (! notes[i].selected)
                continue;

            if (std::abs (dyRows) < 0.5)
            {
                notes[i].targetMidi = initTarget[i];
                continue;
            }

            int m = (int) std::lround ((double) initTarget[i] + dyRows);
            if (proc.lockScale)
                m = pauta::nearestInScale ((float) m, mask);
            notes[i].targetMidi = (float) m;
        }
    }
    else if (tool == Tool::time)
    {
        const double dxSamples = (double) (e.x - downPos.x) / pps * sr();
        for (size_t i = 0; i < notes.size(); ++i)
        {
            auto& n = notes[i];
            if (! n.selected)
                continue;

            double desiredStart = (double) n.start + (double) initShift[i] + dxSamples;
            if (gridSeconds() > 0.0)
                desiredStart = snapTimelineSample (desiredStart);

            double shift = desiredStart - (double) n.start;
            shift = juce::jlimit ((double) -n.start, (double) (N - n.end), shift);
            n.shift = (int) std::lround (shift);
        }
    }

    proc.requestRender();
    repaint();
}

void PianoRoll::mouseUp (const juce::MouseEvent&)
{
    if (dragging)
    {
        dragging = false;
        proc.requestRender();
        if (onSelectionChanged)
            onSelectionChanged();
        repaint();
    }
}

void PianoRoll::mouseMove (const juce::MouseEvent& e)
{
    if (tool != Tool::cut)
        return;

    const int idx = hitTest (e.position);
    const int old = cutHoverIndex;
    const double oldS = cutHoverSample;
    cutHoverIndex = idx;
    if (idx >= 0)
        cutHoverSample = snapTimelineSample (sampleOfX ((double) e.x));

    if (old != cutHoverIndex || oldS != cutHoverSample)
        repaint();
}

void PianoRoll::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (tool != Tool::pitch || ! proc.hasProject())
        return;

    const int idx = hitTest (e.position);
    if (idx < 0)
        return;

    // doble clic = llevar esa nota a la nota más cercana de la escala
    proc.pushUndo();
    auto& n = proc.getNotes()[(size_t) idx];
    n.targetMidi = (float) pauta::nearestInScale (n.targetMidi, proc.getScaleMask());
    proc.requestRender();
    repaint();
}

void PianoRoll::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
    {
        if (w.deltaY != 0.0f)
            setPixelsPerSecond (pps * (w.deltaY > 0.0f ? 1.18 : 1.0 / 1.18), (double) e.x);
        return;
    }

    const double span = contentArea().getWidth() / pps;

    if (e.mods.isShiftDown())
        scrollX -= (double) (w.deltaY != 0.0f ? w.deltaY : w.deltaX) * span * 0.6;
    else
    {
        topMidi += std::round ((double) w.deltaY * 10.0);
        scrollX -= (double) w.deltaX * span * 0.6;
    }

    clampScroll();
    updateScrollbars();
    repaint();
}

bool PianoRoll::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress ('z', juce::ModifierKeys::commandModifier, 0))
    {
        proc.undo();
        if (onSelectionChanged)
            onSelectionChanged();
        repaint();
        return true;
    }

    if (k == juce::KeyPress ('a', juce::ModifierKeys::commandModifier, 0))
    {
        proc.selectAll (true);
        if (onSelectionChanged)
            onSelectionChanged();
        repaint();
        return true;
    }

    return false;
}
