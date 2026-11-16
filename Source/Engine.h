#pragma once

// Motor de La Pauta Tune: detección de tono, segmentación en notas y
// re-afinación tipo PSOLA. C++ estándar puro (sin JUCE) para poder probarlo solo.

#include <cmath>
#include <functional>
#include <vector>

namespace pauta
{

struct Note
{
    int   start = 0;              // inicio en la voz original (muestras)
    int   end = 0;                // fin en la voz original (muestras, exclusivo)
    float detectedMidi = 60.0f;   // tono detectado (nota MIDI con decimales)
    float targetMidi = 60.0f;     // tono al que se quiere llevar la nota
    float variation = 1.0f;       // 1 = conserva la variación natural, 0 = nota plana
    int   shift = 0;              // desplazamiento en el tiempo (muestras)
    bool  selected = false;       // solo para la interfaz
};

struct F0Track
{
    double sampleRate = 44100.0;
    int hop = 256;                // muestras entre marcos de análisis
    int firstCenter = 0;          // muestra central del primer marco
    std::vector<float> hz;        // frecuencia por marco, 0 = sin voz

    int numFrames() const { return (int) hz.size(); }
};

inline float hzToMidi (float hz) { return 69.0f + 12.0f * std::log2 (hz / 440.0f); }
inline float midiToHz (float m)  { return 440.0f * std::pow (2.0f, (m - 69.0f) / 12.0f); }

// Devuelve false si se canceló (progress devuelve false).
bool analyzePitch (const std::vector<float>& mono, double sampleRate, F0Track& out,
                   const std::function<bool (float)>& progress);

std::vector<Note> segmentNotes (const F0Track& track, int numSamples);

float medianMidi (const F0Track& track, int start, int end, float fallback);
float midiAt (const F0Track& track, float samplePos, float fallback);

// Corta una nota en dos. Devuelve false si el corte queda demasiado cerca de un borde.
bool splitNote (std::vector<Note>& notes, int index, int atSample, const F0Track& track);

// Genera la voz afinada (misma longitud que la original).
std::vector<float> render (const std::vector<float>& mono, const F0Track& track,
                           const std::vector<Note>& notes);

} // namespace pauta
