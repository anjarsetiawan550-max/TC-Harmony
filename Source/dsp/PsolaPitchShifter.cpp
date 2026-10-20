#include "PsolaPitchShifter.h"
#include <cmath>
#include <algorithm>

namespace
{
    constexpr float kTwoPi = 6.28318530717958647692f;
}

void PsolaPitchShifter::prepare (double newSampleRate, int maxHalf)
{
    sampleRate = newSampleRate;
    maxHalfPeriod = maxHalf;
    minHalfPeriod = std::max (16, static_cast<int> (sampleRate / 1200.0));
    unvoicedHalf = 256;

    // Cukup untuk: 2 periode terpanjang + ruang geser pencarian fase (T/4) + sedikit cadangan.
    latency = 2 * maxHalfPeriod + maxHalfPeriod / 4 + 16;

    inBuf.assign (static_cast<size_t> (kInLen), 0.0f);
    periodBuf.assign (static_cast<size_t> (kInLen), 0.0f);
    outBuf.assign (static_cast<size_t> (kOutLen), 0.0f);
    refSegment.assign (static_cast<size_t> (maxHalfPeriod * 2 + 16), 0.0f);
    candSegment.assign (static_cast<size_t> (maxHalfPeriod * 4 + 16), 0.0f);

    glideCoeff = 1.0f - std::exp (-1.0f / (0.008f * static_cast<float> (sampleRate)));
    setExtraDelayMs (extraDelayMs);
    reset();
}

void PsolaPitchShifter::setExtraDelayMs (float ms) noexcept
{
    extraDelayMs = ms;
    extraDelaySamples = static_cast<int64_t> (std::lround (static_cast<double> (ms) * 0.001 * sampleRate));
}

void PsolaPitchShifter::reset()
{
    std::fill (inBuf.begin(), inBuf.end(), 0.0f);
    std::fill (periodBuf.begin(), periodBuf.end(), 0.0f);
    std::fill (outBuf.begin(), outBuf.end(), 0.0f);
    inCount = 0;
    outCount = 0;
    nextSynth = 0.0;
    lastMarkValid = false;
    lastMark = 0;
    currentSemitones = targetSemitones;
}

float PsolaPitchShifter::inAt (int64_t a) const noexcept
{
    if (a < 0 || a >= inCount)
        return 0.0f;
    return inBuf[static_cast<size_t> (a & kInMask)];
}

void PsolaPitchShifter::pushDrySample (float sample, float periodSamples) noexcept
{
    const size_t i = static_cast<size_t> (inCount & kInMask);
    inBuf[i] = sample;
    periodBuf[i] = periodSamples;
    ++inCount;
}

float PsolaPitchShifter::renderOutputSample() noexcept
{
    currentSemitones += glideCoeff * (targetSemitones - currentSemitones);

    // Semua grain yang menyentuh sampel keluaran ini harus sudah ditaruh sebelum dibaca.
    while (nextSynth - static_cast<double> (maxHalfPeriod) <= static_cast<double> (outCount))
        placeGrain();

    const size_t idx = static_cast<size_t> (outCount & kOutMask);
    const float y = outBuf[idx];
    outBuf[idx] = 0.0f;
    ++outCount;
    return y;
}

void PsolaPitchShifter::placeGrain()
{
    const double s = nextSynth;                                   // pusat grain di waktu keluaran
    const int64_t c0 = static_cast<int64_t> (std::floor (s)) - latency - extraDelaySamples; // posisi analisis yang dituju

    // Periode yang berlaku di sekitar c0: estimasi pitch yang jendelanya berpusat di c0
    // tersimpan di c0 + (setengah jendela analisis).
    const int64_t look = std::min<int64_t> (c0 + maxHalfPeriod, inCount - 1);
    const float period = look >= 0 ? periodBuf[static_cast<size_t> (look & kInMask)] : 0.0f;

    const bool voiced = period >= static_cast<float> (minHalfPeriod)
                     && period <= static_cast<float> (maxHalfPeriod - 1);

    int half;
    int64_t c;
    float gain = 1.0f;
    double spacing;

    if (! voiced)
    {
        // Tak bernada: teruskan apa adanya (Hann 2H, hop H -> jumlah jendela = 1).
        half = unvoicedHalf;
        c = c0;
        spacing = static_cast<double> (half);
        lastMarkValid = false;
    }
    else
    {
        const float semis = currentSemitones + detuneCents * 0.01f;
        const float ratio = std::exp2 (semis / 12.0f);

        const int T = std::clamp (static_cast<int> (std::lround (period)), minHalfPeriod, maxHalfPeriod - 1);
        half = T;

        if (! lastMarkValid || c0 - lastMark > 6 * static_cast<int64_t> (T))
        {
            // Grain pertama pada segmen bernada: kunci ke puncak terkuat dalam satu periode.
            int64_t best = c0;
            float bestV = -1.0e30f;
            for (int64_t p = c0 - T; p <= c0; ++p)
            {
                const float v = inAt (p);
                if (v > bestV) { bestV = v; best = p; }
            }
            c = best;
        }
        else if (c0 <= lastMark + T - 1)
        {
            // Belum waktunya maju satu periode penuh (rasio > 1): ulang grain yang sama.
            c = lastMark;
        }
        else
        {
            const int64_t k = (c0 - lastMark) / T;               // jumlah periode utuh, tidak melewati c0
            const int64_t cand = lastMark + k * T;
            const int range = T / 4;
            const int win = T / 2;

            for (int n = -win; n < win; ++n)
                refSegment[static_cast<size_t> (n + win)] = inAt (lastMark + n);

            const int segLen = 2 * win + 2 * range + 1;
            for (int n = 0; n < segLen; ++n)
                candSegment[static_cast<size_t> (n)] = inAt (cand - range - win + n);

            double bestScore = -1.0e30;
            int bestShift = 0;
            for (int shift = -range; shift <= range; ++shift)
            {
                const float* seg = candSegment.data() + (shift + range);
                float num = 0.0f, den = 0.0f;
                for (int m = 0; m < 2 * win; ++m)
                {
                    num += seg[m] * refSegment[static_cast<size_t> (m)];
                    den += seg[m] * seg[m];
                }
                const double score = static_cast<double> (num) / std::sqrt (static_cast<double> (den) + 1.0e-9);
                if (score > bestScore) { bestScore = score; bestShift = shift; }
            }
            c = cand + bestShift;
        }

        lastMark = c;
        lastMarkValid = true;

        gain = std::clamp (1.0f / ratio, 0.5f, 2.0f);             // jumlah Hann berlapis = rasio -> dinormalkan
        spacing = static_cast<double> (T) / static_cast<double> (ratio);
    }

    // --- taruh grain (Hann 2*half) di keluaran, dengan posisi pecahan (interpolasi linear) ---
    const int len = 2 * half;
    const double startPos = s - static_cast<double> (half);
    const int64_t si = static_cast<int64_t> (std::floor (startPos));
    const float frac = static_cast<float> (startPos - static_cast<double> (si));

    for (int n = 0; n < len; ++n)
    {
        const int64_t p = si + n;
        if (p < outCount)
            continue;

        const float w = 0.5f - 0.5f * std::cos (kTwoPi * (static_cast<float> (n) + 0.5f) / static_cast<float> (len));
        const float v = inAt (c - half + n) * w * gain;

        outBuf[static_cast<size_t> (p & kOutMask)] += v * (1.0f - frac);
        outBuf[static_cast<size_t> ((p + 1) & kOutMask)] += v * frac;
    }

    nextSynth = s + spacing;
}
