#pragma once
#include <JuceHeader.h>

// Lightweight autocorrelation pitch estimator foundation.
// Returns an estimated fundamental frequency in Hz, or 0 when no reliable
// periodic signal is detected. This is analysis only, not pitch correction.
class PitchAnalysis
{
public:
    static float estimateFundamental(const float* samples,
                                     int numSamples,
                                     double sampleRate,
                                     float minHz = 65.0f,
                                     float maxHz = 1000.0f);
};
