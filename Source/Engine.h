#pragma once
#include <vector>
#include <cmath>
#include <functional>
#include <algorithm>

namespace pauta {
struct F0Track {
    std::vector<float> f0; // Hz, 0 = silencio, por hop
    float hopSeconds = 0.01f;
    double sampleRate = 44100.0;
};
struct Note {
    int start = 0, end = 0;
    float detectedMidi = 60.f, targetMidi = 60.f;
    float variation = 1.f;
    int shift = 0;
    bool selected = false;
};
bool analyzePitch(const std::vector<float>& mono, double sr, F0Track& out, std::function<bool(float)> progress);
std::vector<Note> segmentNotes(const F0Track& t, int totalSamples);
bool splitNote(std::vector<Note>& notes, int idx, int atSample, const F0Track& t);
std::vector<float> render(const std::vector<float>& mono, const F0Track& t, const std::vector<Note>& notes);
inline float hzToMidi(float hz){ return hz<=0?60.f:69.f+12.f*std::log2(hz/440.f); }
inline float midiToHz(float m){ return 440.f*std::pow(2.f,(m-69.f)/12.f); }
}
