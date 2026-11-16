#include "Engine.h"

#include <algorithm>

namespace pauta
{

namespace
{
    constexpr float kMinHz = 65.0f;
    constexpr float kMaxHz = 900.0f;
    constexpr float kPi = 3.14159265358979f;

    float medianOf (std::vector<float> v)
    {
        if (v.empty())
            return 0.0f;
        const size_t mid = v.size() / 2;
        std::nth_element (v.begin(), v.begin() + (long) mid, v.end());
        return v[mid];
    }

    // Filtro de media móvil centrado (sin retardo), solo para buscar marcas de tono.
    std::vector<float> smooth (const std::vector<float>& x, double sr)
    {
        const int n = (int) x.size();
        int len = std::max (1, (int) (sr / 2500.0));
        if (len % 2 == 0)
            ++len;
        const int h = len / 2;

        std::vector<double> cs ((size_t) n + 1, 0.0);
        for (int i = 0; i < n; ++i)
            cs[(size_t) i + 1] = cs[(size_t) i] + x[(size_t) i];

        std::vector<float> out ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            const int a = std::max (0, i - h);
            const int b = std::min (n, i + h + 1);
            out[(size_t) i] = (float) ((cs[(size_t) b] - cs[(size_t) a]) / (double) (b - a));
        }
        return out;
    }
}

//==============================================================================
bool analyzePitch (const std::vector<float>& x, double sr, F0Track& out,
                   const std::function<bool (float)>& progress)
{
    out = F0Track();
    out.sampleRate = sr;

    const int n = (int) x.size();
    const int D = std::max (1, (int) std::lround (sr / 11025.0));
    const double srd = sr / (double) D;
    const int hopD = 64;
    const int W = 384;
    const int tauMin = std::max (2, (int) std::floor (srd / kMaxHz));
    const int tauMax = (int) std::ceil (srd / kMinHz);

    out.hop = hopD * D;

    // Reducción de frecuencia de muestreo (ventana Hann de 2D muestras)
    const int nd = n / D - 1;
    if (nd < W + tauMax + hopD)
        return true; // audio demasiado corto: sin marcos

    std::vector<float> y ((size_t) nd);
    if (D == 1)
    {
        std::copy (x.begin(), x.begin() + nd, y.begin());
    }
    else
    {
        std::vector<float> w ((size_t) (2 * D));
        float ws = 0.0f;
        for (int k = 0; k < 2 * D; ++k)
        {
            w[(size_t) k] = 0.5f - 0.5f * std::cos (2.0f * kPi * ((float) k + 0.5f) / (float) (2 * D));
            ws += w[(size_t) k];
        }
        for (auto& v : w)
            v /= ws;

        for (int i = 0; i < nd; ++i)
        {
            float s = 0.0f;
            const float* p = x.data() + (size_t) i * (size_t) D;
            for (int k = 0; k < 2 * D; ++k)
                s += p[k] * w[(size_t) k];
            y[(size_t) i] = s;
        }
    }

    const int numFrames = (nd - W - tauMax) / hopD + 1;
    const int centerD = W / 2 + tauMax / 4;
    out.firstCenter = centerD * D + D;
    out.hz.assign ((size_t) numFrames, 0.0f);

    // Energía por marco
    std::vector<float> rms ((size_t) numFrames, 0.0f);
    float peakRms = 0.0f;
    for (int f = 0; f < numFrames; ++f)
    {
        const float* p = y.data() + (size_t) f * (size_t) hopD;
        float e = 0.0f;
        for (int j = 0; j < W; ++j)
            e += p[j] * p[j];
        rms[(size_t) f] = std::sqrt (e / (float) W);
        peakRms = std::max (peakRms, rms[(size_t) f]);
    }
    const float gate = std::max (5.0e-4f, 0.02f * peakRms);

    std::vector<float> d ((size_t) tauMax + 1), dn ((size_t) tauMax + 1);

    for (int f = 0; f < numFrames; ++f)
    {
        if ((f & 255) == 0 && progress && ! progress ((float) f / (float) numFrames))
            return false;

        if (rms[(size_t) f] < gate)
            continue;

        const float* p = y.data() + (size_t) f * (size_t) hopD;

        for (int tau = 1; tau <= tauMax; ++tau)
        {
            float s = 0.0f;
            const float* q = p + tau;
            for (int j = 0; j < W; ++j)
            {
                const float df = p[j] - q[j];
                s += df * df;
            }
            d[(size_t) tau] = s;
        }

        dn[0] = 1.0f;
        float running = 0.0f;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            running += d[(size_t) tau];
            dn[(size_t) tau] = running > 0.0f ? d[(size_t) tau] * (float) tau / running : 1.0f;
        }

        int best = -1;
        for (int tau = tauMin; tau <= tauMax; ++tau)
        {
            if (dn[(size_t) tau] < 0.15f)
            {
                while (tau + 1 <= tauMax && dn[(size_t) tau + 1] < dn[(size_t) tau])
                    ++tau;
                best = tau;
                break;
            }
        }

        if (best < 0)
        {
            int mn = tauMin;
            for (int tau = tauMin; tau <= tauMax; ++tau)
                if (dn[(size_t) tau] < dn[(size_t) mn])
                    mn = tau;
            if (dn[(size_t) mn] < 0.30f)
                best = mn;
        }

        if (best < 0)
            continue;

        float tauF = (float) best;
        if (best > 1 && best < tauMax)
        {
            const float a = dn[(size_t) best - 1], b = dn[(size_t) best], c = dn[(size_t) best + 1];
            const float denom = a - 2.0f * b + c;
            if (std::fabs (denom) > 1.0e-9f)
                tauF += 0.5f * (a - c) / denom;
        }

        const float hz = (float) (srd / (double) tauF);
        if (hz >= kMinHz && hz <= kMaxHz)
            out.hz[(size_t) f] = hz;
    }

    // Suavizado con mediana de 5 puntos sobre los marcos con voz
    {
        std::vector<float> midi ((size_t) numFrames, -1.0f);
        for (int f = 0; f < numFrames; ++f)
            if (out.hz[(size_t) f] > 0.0f)
                midi[(size_t) f] = hzToMidi (out.hz[(size_t) f]);

        std::vector<float> sm = midi;
        for (int f = 0; f < numFrames; ++f)
        {
            if (midi[(size_t) f] < 0.0f)
                continue;
            std::vector<float> nb;
            for (int k = -2; k <= 2; ++k)
            {
                const int g = f + k;
                if (g >= 0 && g < numFrames && midi[(size_t) g] >= 0.0f)
                    nb.push_back (midi[(size_t) g]);
            }
            sm[(size_t) f] = medianOf (nb);
        }
        for (int f = 0; f < numFrames; ++f)
            out.hz[(size_t) f] = sm[(size_t) f] >= 0.0f ? midiToHz (sm[(size_t) f]) : 0.0f;
    }

    // Quita tramos de voz demasiado cortos (< 4 marcos)
    {
        int f = 0;
        while (f < numFrames)
        {
            if (out.hz[(size_t) f] <= 0.0f)
            {
                ++f;
                continue;
            }
            const int a = f;
            while (f < numFrames && out.hz[(size_t) f] > 0.0f)
                ++f;
            if (f - a < 4)
                for (int k = a; k < f; ++k)
                    out.hz[(size_t) k] = 0.0f;
        }
    }

    return true;
}

//==============================================================================
float medianMidi (const F0Track& t, int start, int end, float fallback)
{
    if (t.hz.empty() || t.hop <= 0)
        return fallback;

    const int nf = t.numFrames();
    const int f0 = std::max (0, (int) std::ceil ((double) (start - t.firstCenter) / t.hop));
    const int f1 = std::min (nf - 1, (int) std::floor ((double) (end - 1 - t.firstCenter) / t.hop));

    std::vector<float> v;
    for (int f = f0; f <= f1; ++f)
        if (t.hz[(size_t) f] > 0.0f)
            v.push_back (hzToMidi (t.hz[(size_t) f]));

    return v.empty() ? fallback : medianOf (v);
}

float midiAt (const F0Track& t, float pos, float fallback)
{
    const int nf = t.numFrames();
    if (nf == 0 || t.hop <= 0)
        return fallback;

    const float fi = (pos - (float) t.firstCenter) / (float) t.hop;
    const int i0 = (int) std::floor (fi);
    const float fr = fi - (float) i0;

    auto get = [&] (int i) -> float
    {
        if (i < 0 || i >= nf || t.hz[(size_t) i] <= 0.0f)
            return -1.0f;
        return hzToMidi (t.hz[(size_t) i]);
    };

    const float a = get (i0), b = get (i0 + 1);
    if (a >= 0.0f && b >= 0.0f) return a + (b - a) * fr;
    if (a >= 0.0f) return a;
    if (b >= 0.0f) return b;

    for (int k = 1; k <= 3; ++k)
    {
        float c = get (i0 - k);
        if (c >= 0.0f) return c;
        c = get (i0 + 1 + k);
        if (c >= 0.0f) return c;
    }
    return fallback;
}

//==============================================================================
std::vector<Note> segmentNotes (const F0Track& t, int numSamples)
{
    std::vector<Note> notes;
    const int nf = t.numFrames();
    if (nf == 0)
        return notes;

    auto boundary = [&] (int f)
    {
        return std::clamp (t.firstCenter + f * t.hop - t.hop / 2, 0, numSamples);
    };
    auto midiOf = [&] (int f) { return hzToMidi (t.hz[(size_t) f]); };

    auto pushNote = [&] (int a, int b)
    {
        Note nt;
        nt.start = boundary (a);
        nt.end = boundary (b);
        if (nt.end <= nt.start)
            return;
        nt.detectedMidi = medianMidi (t, nt.start, nt.end, midiOf (a));
        nt.targetMidi = nt.detectedMidi;
        notes.push_back (nt);
    };

    int f = 0;
    while (f < nf)
    {
        if (t.hz[(size_t) f] <= 0.0f)
        {
            ++f;
            continue;
        }

        const int a = f;
        while (f < nf && t.hz[(size_t) f] > 0.0f)
            ++f;
        const int b = f;

        int noteStart = a;
        std::vector<float> cur;
        int dev = 0;

        for (int k = a; k < b; ++k)
        {
            const float m = midiOf (k);
            if (cur.empty())
            {
                cur.push_back (m);
                continue;
            }

            if (std::fabs (m - medianOf (cur)) > 0.8f)
            {
                if (++dev >= 4)
                {
                    const int split = k - dev + 1;
                    pushNote (noteStart, split);
                    noteStart = split;
                    cur.clear();
                    for (int q = split; q <= k; ++q)
                        cur.push_back (midiOf (q));
                    dev = 0;
                }
            }
            else
            {
                dev = 0;
                cur.push_back (m);
            }
        }
        pushNote (noteStart, b);
    }

    // Une las notas demasiado cortas (< 5 marcos) con su vecina si se tocan
    const int minLen = 5 * t.hop;
    for (size_t i = 0; i < notes.size();)
    {
        auto& nt = notes[i];
        if (nt.end - nt.start < minLen)
        {
            if (i > 0 && notes[i - 1].end == nt.start)
            {
                auto& pv = notes[i - 1];
                pv.end = nt.end;
                pv.detectedMidi = medianMidi (t, pv.start, pv.end, pv.detectedMidi);
                pv.targetMidi = pv.detectedMidi;
                notes.erase (notes.begin() + (long) i);
                continue;
            }
            if (i + 1 < notes.size() && notes[i + 1].start == nt.end)
            {
                auto& nx = notes[i + 1];
                nx.start = nt.start;
                nx.detectedMidi = medianMidi (t, nx.start, nx.end, nx.detectedMidi);
                nx.targetMidi = nx.detectedMidi;
                notes.erase (notes.begin() + (long) i);
                continue;
            }
        }
        ++i;
    }

    return notes;
}

bool splitNote (std::vector<Note>& notes, int idx, int at, const F0Track& t)
{
    if (idx < 0 || idx >= (int) notes.size())
        return false;

    Note a = notes[(size_t) idx];
    const int minLen = (int) (0.015 * t.sampleRate);
    if (at - a.start < minLen || a.end - at < minLen)
        return false;

    Note b = a;
    const bool untouched = std::fabs (a.targetMidi - a.detectedMidi) < 1.0e-3f;

    a.end = at;
    b.start = at;
    a.detectedMidi = medianMidi (t, a.start, a.end, a.detectedMidi);
    b.detectedMidi = medianMidi (t, b.start, b.end, b.detectedMidi);

    if (untouched)
    {
        a.targetMidi = a.detectedMidi;
        b.targetMidi = b.detectedMidi;
    }

    notes[(size_t) idx] = a;
    notes.insert (notes.begin() + idx + 1, b);
    return true;
}

//==============================================================================
std::vector<float> render (const std::vector<float>& x, const F0Track& tr, const std::vector<Note>& notes)
{
    const int N = (int) x.size();
    const double sr = tr.sampleRate;

    std::vector<float> out = x;
    if (N == 0 || notes.empty())
        return out;

    std::vector<float> lp;                       // se calcula solo si hace falta
    std::vector<float> y, origMask, wetMask;     // idem

    auto ensureBuffers = [&]
    {
        if (! y.empty())
            return;
        lp = smooth (x, sr);
        y.assign ((size_t) N, 0.0f);
        origMask.assign ((size_t) N, 0.0f);
        wetMask.assign ((size_t) N, 0.0f);
    };

    auto addMask = [&] (std::vector<float>& mask, int a, int b)
    {
        a = std::max (0, a);
        b = std::min (N, b);
        if (b <= a)
            return;
        const int fade = std::max (1, std::min (256, (b - a) / 2));
        for (int i = a; i < b; ++i)
        {
            const float r = std::min ({ 1.0f, (float) (i - a + 1) / (float) fade, (float) (b - i) / (float) fade });
            mask[(size_t) i] = std::max (mask[(size_t) i], r);
        }
    };

    for (const auto& nt : notes)
    {
        const bool untouched = std::fabs (nt.targetMidi - nt.detectedMidi) < 1.0e-3f
                               && std::fabs (nt.variation - 1.0f) < 1.0e-3f
                               && nt.shift == 0;
        if (untouched || nt.end - nt.start < 32)
            continue;

        ensureBuffers();

        auto periodAt = [&] (float pos)
        {
            const float m = midiAt (tr, pos, nt.detectedMidi);
            return (float) (sr / (double) midiToHz (m));
        };

        // Marcas de tono sincronizadas con el periodo
        const int s0 = std::max (0, nt.start - (int) (2.0f * periodAt ((float) nt.start)));
        const int s1 = std::min (N, nt.end + (int) (2.0f * periodAt ((float) nt.end - 1.0f)));

        float mx = 0.0f, mn = 0.0f;
        for (int i = std::max (0, nt.start); i < std::min (N, nt.end); ++i)
        {
            mx = std::max (mx, lp[(size_t) i]);
            mn = std::min (mn, lp[(size_t) i]);
        }
        const float sgn = (mx >= -mn) ? 1.0f : -1.0f;
        const float amp = std::max (mx, -mn);
        if (amp < 1.0e-6f)
            continue;

        std::vector<int> marks;
        {
            const float P0 = periodAt ((float) s0);
            const int hi = std::min (s1, s0 + (int) P0 + 1);
            int m = s0;
            float bs = -1.0e30f;
            for (int i = s0; i < hi; ++i)
                if (sgn * lp[(size_t) i] > bs)
                {
                    bs = sgn * lp[(size_t) i];
                    m = i;
                }
            marks.push_back (m);

            while (m < s1)
            {
                const float Pm = periodAt ((float) m);
                const int lo = std::max (m + 1, m + (int) (0.7f * Pm));
                const int hi2 = std::min (N, m + (int) (1.3f * Pm) + 1);
                if (lo >= hi2 || lo >= s1 + (int) Pm)
                    break;

                const float centre = (float) m + Pm;
                int best = lo;
                float bestScore = -1.0e30f;
                for (int i = lo; i < hi2; ++i)
                {
                    const float score = sgn * lp[(size_t) i] - 0.15f * amp * std::fabs ((float) i - centre) / (0.3f * Pm);
                    if (score > bestScore)
                    {
                        bestScore = score;
                        best = i;
                    }
                }
                marks.push_back (best);
                m = best;
            }
        }

        if (marks.size() < 3)
            continue;

        // Síntesis: una ventana Hann de 2 periodos por cada marca de salida
        size_t j = 0;
        double ts = (double) nt.start + (double) nt.shift;
        const double tsEnd = (double) nt.end + (double) nt.shift;

        while (ts < tsEnd)
        {
            const float ta = (float) (ts - (double) nt.shift);
            while (j + 1 < marks.size() && std::fabs ((float) marks[j + 1] - ta) <= std::fabs ((float) marks[j] - ta))
                ++j;

            const int m = marks[j];
            float Pa;
            if (j > 0 && j + 1 < marks.size())
                Pa = 0.5f * (float) (marks[j + 1] - marks[j - 1]);
            else if (j + 1 < marks.size())
                Pa = (float) (marks[j + 1] - marks[j]);
            else
                Pa = (float) (marks[j] - marks[j - 1]);
            Pa = std::max (Pa, 8.0f);

            const float tgt = nt.targetMidi + nt.variation * (midiAt (tr, ta, nt.detectedMidi) - nt.detectedMidi);
            const float Ps = (float) (sr / (double) midiToHz (tgt));
            const float gain = std::clamp (Ps / Pa, 0.5f, 2.0f);

            const int half = (int) Pa;
            const int tsi = (int) std::lround (ts);
            for (int k = -half; k <= half; ++k)
            {
                const int src = m + k, dst = tsi + k;
                if (src < 0 || src >= N || dst < 0 || dst >= N)
                    continue;
                const float w = 0.5f + 0.5f * std::cos (kPi * (float) k / (float) (half + 1));
                y[(size_t) dst] += x[(size_t) src] * w * gain;
            }

            ts += (double) std::max (Ps, 8.0f);
        }

        addMask (wetMask, nt.start + nt.shift, nt.end + nt.shift);
        addMask (origMask, nt.start, nt.end);
    }

    if (! y.empty())
        for (int i = 0; i < N; ++i)
            out[(size_t) i] = x[(size_t) i] * (1.0f - origMask[(size_t) i]) + y[(size_t) i] * wetMask[(size_t) i];

    return out;
}

} // namespace pauta
