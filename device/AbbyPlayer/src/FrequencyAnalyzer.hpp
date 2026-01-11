#pragma once
#include <vector>
#include <complex>
#include <mutex>

class FrequencyAnalyzer {
public:
    FrequencyAnalyzer();
    
    // Updates the analyzer with new PCM samples
    void pushSamples(const float* mySamples, int count);
    
    // Retrieves the current frequency spectrum (normalized 0.0 - 1.0)
    // bands: number of output bands desired
    // bands: number of output bands desired
    std::vector<float> getSpectrum(int bands);
    
    // Returns the frequency (Hz) with the highest magnitude
    float getDominantFrequency(float sampleRate);

private:
    void fft(std::vector<std::complex<float>>& x);
    
    std::mutex m_mutex;
    std::vector<float> m_inputBuffer;
    std::vector<float> m_currentSpectrum;
    static const int FFT_SIZE = 512;
};
