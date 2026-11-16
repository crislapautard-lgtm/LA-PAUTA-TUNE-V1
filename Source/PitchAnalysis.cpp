#include "PitchAnalysis.h"

float PitchAnalysis::estimateFundamental(const float* samples,
                                         int numSamples,
                                         double sampleRate,
                                         float minHz,
                                         float maxHz)
{
    if (samples == nullptr || numSamples < 128 || sampleRate <= 0.0
        || minHz <= 0.0f || maxHz <= minHz)
        return 0.0f;

    const int minLag = juce::jmax(2, static_cast<int>(sampleRate / maxHz));
    const int maxLag = juce::jmin(numSamples / 2,
                                  static_cast<int>(sampleRate / minHz));
    if (maxLag <= minLag)
        return 0.0f;

    double bestCorrelation = 0.0;
    int bestLag = 0;

    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        double cross = 0.0;
        double energyA = 0.0;
        double energyB = 0.0;

        for (int i = 0; i < numSamples - lag; ++i)
        {
            const double a = samples[i];
            const double b = samples[i + lag];
            cross += a * b;
            energyA += a * a;
            energyB += b * b;
        }

        const double denominator = std::sqrt(energyA * energyB) + 1.0e-12;
        const double correlation = cross / denominator;

        if (correlation > bestCorrelation)
        {
            bestCorrelation = correlation;
            bestLag = lag;
        }
    }

    if (bestLag == 0 || bestCorrelation < 0.55)
        return 0.0f;

    return static_cast<float>(sampleRate / static_cast<double>(bestLag));
}
