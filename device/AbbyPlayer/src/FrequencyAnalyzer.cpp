#include "FrequencyAnalyzer.hpp"
#include <cmath>
#include <algorithm>

const float PI = 3.141592653589793238460;

FrequencyAnalyzer::FrequencyAnalyzer() {
    m_inputBuffer.reserve(FFT_SIZE);
}

void FrequencyAnalyzer::pushSamples(const float* mySamples, int count) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Add new samples to buffer
    for (int i = 0; i < count; ++i) {
        if (m_inputBuffer.size() >= FFT_SIZE) {
            m_inputBuffer.erase(m_inputBuffer.begin());
        }
        m_inputBuffer.push_back(mySamples[i]);
    }
    
    // Only compute if we have enough data
    if (m_inputBuffer.size() == FFT_SIZE) {
        // Prepare complex buffer for FFT
        std::vector<std::complex<float>> data(FFT_SIZE);
        
        // Apply Hanning window to reduce spectral leakage
        for (int i = 0; i < FFT_SIZE; ++i) {
            float window = 0.5f * (1.0f - cos(2.0f * PI * i / (FFT_SIZE - 1)));
            data[i] = std::complex<float>(m_inputBuffer[i] * window, 0.0f);
        }
        
        // Perform FFT
        fft(data);
        
        // Compute magnitudes (only first half is useful for real input)
        std::vector<float> magnitudes;
        magnitudes.resize(FFT_SIZE / 2);
        
        float maxVal = 0.0001f; // Prevent div by zero
        for (int i = 0; i < FFT_SIZE / 2; ++i) {
            float mag = std::abs(data[i]);
            magnitudes[i] = mag;
            if (mag > maxVal) maxVal = mag;
        }
        
        // Normalize
        for (float& val : magnitudes) {
            val /= maxVal;
        }
        
        m_currentSpectrum = magnitudes;
    }
}

std::vector<float> FrequencyAnalyzer::getSpectrum(int bands) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_currentSpectrum.empty()) return std::vector<float>(bands, 0.0f);
    
    std::vector<float> result(bands, 0.0f);
    
    // Map FFT bins to requested bands
    // This is a simple linear interpolation, can be improved to logarithmic later
    int binsPerBand = m_currentSpectrum.size() / bands;
    if (binsPerBand < 1) binsPerBand = 1;

    for (int i = 0; i < bands; ++i) {
        float sum = 0;
        int count = 0;
        int startBin = i * binsPerBand;
        for (int j = 0; j < binsPerBand && (startBin + j) < m_currentSpectrum.size(); ++j) {
            sum += m_currentSpectrum[startBin + j];
            count++;
        }
        result[i] = (count > 0) ? (sum / count) : 0.0f;
    }
    
    return result;
}

float FrequencyAnalyzer::getDominantFrequency(float sampleRate) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_currentSpectrum.empty()) return 0.0f;
    
    float maxMag = -1.0f;
    int maxBin = -1;
    
    // Find bin with max magnitude
    for (size_t i = 1; i < m_currentSpectrum.size(); ++i) { // Skip DC component at i=0
        if (m_currentSpectrum[i] > maxMag) {
            maxMag = m_currentSpectrum[i];
            maxBin = i;
        }
    }
    
    if (maxBin <= 0) return 0.0f;
    
    // Frequency = binIndex * SampleRate / FFT_SIZE
    return (float)maxBin * sampleRate / FFT_SIZE;
}

// Simple recursive Cooley-Tukey FFT
void FrequencyAnalyzer::fft(std::vector<std::complex<float>>& x) {
    int n = x.size();
    if (n <= 1) return;
    
    std::vector<std::complex<float>> even(n / 2), odd(n / 2);
    for (int i = 0; i < n / 2; ++i) {
        even[i] = x[2 * i];
        odd[i] = x[2 * i + 1];
    }
    
    fft(even);
    fft(odd);
    
    for (int k = 0; k < n / 2; ++k) {
        std::complex<float> t = std::polar(1.0f, -2.0f * PI * k / n) * odd[k];
        x[k] = even[k] + t;
        x[k + n / 2] = even[k] - t;
    }
}
