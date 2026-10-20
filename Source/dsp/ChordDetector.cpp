#include "ChordDetector.h"
#include <cmath>
#include <algorithm>

namespace
{
    constexpr float kTwoPi = 6.28318530717958647692f;

    struct Template { int n; int iv[4]; float w[4]; float complexityPenalty; };

    // bobot: akar & terts paling menentukan, kuint sedikit lebih ringan, septim paling ringan
    constexpr Template kTemplates[ChordDetector::NumQualities] =
    {
        /* Major     */ { 3, { 0, 4, 7, 0  }, { 1.0f, 1.0f, 0.8f, 0.0f }, 1.00f },
        /* Minor     */ { 3, { 0, 3, 7, 0  }, { 1.0f, 1.0f, 0.8f, 0.0f }, 1.00f },
        /* Dominant7 */ { 4, { 0, 4, 7, 10 }, { 1.0f, 1.0f, 0.8f, 0.7f }, 0.96f },
        /* Major7    */ { 4, { 0, 4, 7, 11 }, { 1.0f, 1.0f, 0.8f, 0.7f }, 0.96f },
        /* Minor7    */ { 4, { 0, 3, 7, 10 }, { 1.0f, 1.0f, 0.8f, 0.7f }, 0.96f },
        /* Dim       */ { 3, { 0, 3, 6, 0  }, { 1.0f, 1.0f, 0.9f, 0.0f }, 0.95f },
    };
}

uint16_t ChordDetector::maskFor (int root, int quality) noexcept
{
    uint16_t m = 0;
    const auto& t = kTemplates[quality];
    for (int i = 0; i < t.n; ++i)
        m = static_cast<uint16_t> (m | (1u << ((root + t.iv[i]) % 12)));
    return m;
}

void ChordDetector::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;

    ring.assign (static_cast<size_t> (kWindow), 0.0f);
    frame.assign (static_cast<size_t> (kWindow), 0.0f);
    window.resize (static_cast<size_t> (kWindow));
    for (int n = 0; n < kWindow; ++n)
        window[static_cast<size_t> (n)] = 0.5f - 0.5f * std::cos (kTwoPi * (static_cast<float> (n) + 0.5f) / static_cast<float> (kWindow));

    for (int b = 0; b < kNumBins; ++b)
    {
        const int midi = kFirstMidi + b;
        const double f = 440.0 * std::pow (2.0, (midi - 69) / 12.0);
        binCoeff[b] = static_cast<float> (2.0 * std::cos (2.0 * 3.14159265358979323846 * f / sampleRate));
        binPitchClass[b] = midi % 12;
        // nada rendah (bawah C3) kurang terpisah di jendela ini -> bobot lebih kecil
        binWeight[b] = midi < 48 ? 0.5f : 1.0f;
    }
    reset();
}

void ChordDetector::reset()
{
    std::fill (ring.begin(), ring.end(), 0.0f);
    std::fill (std::begin (chroma), std::end (chroma), 0.0f);
    writePos = 0;
    sinceAnalysis = 0;
    chordValid = false;
    chordRoot = 0; chordQuality = 0; chordMask = 0; chordScore = 0.0f;
    pendingRoot = -1; pendingQuality = -1; pendingCount = 0;
    silentHops = 0;
}

void ChordDetector::pushSamples (const float* mono, int numSamples) noexcept
{
    for (int i = 0; i < numSamples; ++i)
    {
        ring[static_cast<size_t> (writePos)] = mono[i];
        writePos = (writePos + 1) % kWindow;

        if (++sinceAnalysis >= kHop)
        {
            sinceAnalysis = 0;
            analyse();
        }
    }
}

void ChordDetector::analyse()
{
    // salin jendela terbaru berurutan, kalikan Hann, hitung RMS
    double energy = 0.0;
    for (int n = 0; n < kWindow; ++n)
    {
        const float v = ring[static_cast<size_t> ((writePos + n) % kWindow)];
        energy += static_cast<double> (v) * v;
        frame[static_cast<size_t> (n)] = v * window[static_cast<size_t> (n)];
    }
    const float rms = static_cast<float> (std::sqrt (energy / kWindow));

    // sinyal hening -> tahan akor terakhir beberapa saat, lalu lepas (~1.5 detik)
    if (rms < 0.002f)
    {
        const int holdHops = static_cast<int> (1.5 * sampleRate / kHop);
        if (++silentHops > holdHops)
            chordValid = false;
        return;
    }
    silentHops = 0;

    // --- energi tiap nada (Goertzel) -> chroma ---
    float frameChroma[12] = {};
    for (int b = 0; b < kNumBins; ++b)
    {
        const float coeff = binCoeff[b];
        float s1 = 0.0f, s2 = 0.0f;
        for (int n = 0; n < kWindow; ++n)
        {
            const float s0 = frame[static_cast<size_t> (n)] + coeff * s1 - s2;
            s2 = s1;
            s1 = s0;
        }
        const float power = s1 * s1 + s2 * s2 - coeff * s1 * s2;
        frameChroma[binPitchClass[b]] += std::sqrt (std::max (0.0f, power)) * binWeight[b];
    }

    // haluskan antar jendela
    for (int i = 0; i < 12; ++i)
        chroma[i] = 0.5f * chroma[i] + 0.5f * frameChroma[i];

    float norm = 0.0f;
    for (float v : chroma) norm += v * v;
    norm = std::sqrt (norm);
    if (norm < 1.0e-6f)
        return;

    float c[12];
    for (int i = 0; i < 12; ++i) c[i] = chroma[i] / norm;

    // --- cocokkan dengan template (kemiripan kosinus) ---
    float bestScore = -1.0f, currentScore = -1.0f;
    int bestRoot = 0, bestQuality = 0;

    for (int q = 0; q < NumQualities; ++q)
    {
        const auto& t = kTemplates[q];
        float tNorm = 0.0f;
        for (int i = 0; i < t.n; ++i) tNorm += t.w[i] * t.w[i];
        tNorm = std::sqrt (tNorm);

        for (int root = 0; root < 12; ++root)
        {
            float dot = 0.0f;
            for (int i = 0; i < t.n; ++i)
                dot += t.w[i] * c[(root + t.iv[i]) % 12];

            const float score = (dot / tNorm) * t.complexityPenalty;
            if (score > bestScore) { bestScore = score; bestRoot = root; bestQuality = q; }
            if (chordValid && root == chordRoot && q == chordQuality) currentScore = score;
        }
    }

    if (bestScore < 0.72f)
        return; // tidak yakin: biarkan keadaan sebelumnya

    if (! chordValid)
    {
        chordRoot = bestRoot; chordQuality = bestQuality; chordScore = bestScore;
        chordMask = maskFor (chordRoot, chordQuality);
        chordValid = true;
        pendingCount = 0;
        return;
    }

    if (bestRoot == chordRoot && bestQuality == chordQuality)
    {
        chordScore = bestScore;
        pendingCount = 0;
        return;
    }

    // akor lain menang: harus unggul tipis dan konsisten dua jendela berturut-turut
    if (bestScore > currentScore + 0.04f)
    {
        if (bestRoot == pendingRoot && bestQuality == pendingQuality)
            ++pendingCount;
        else
        {
            pendingRoot = bestRoot; pendingQuality = bestQuality; pendingCount = 1;
        }

        if (pendingCount >= 2)
        {
            chordRoot = bestRoot; chordQuality = bestQuality; chordScore = bestScore;
            chordMask = maskFor (chordRoot, chordQuality);
            pendingCount = 0;
        }
    }
    else
    {
        pendingCount = 0;
    }
}
