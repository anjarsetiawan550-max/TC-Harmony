#pragma once
#include <vector>
#include <cstdint>

/**
    PsolaPitchShifter
    ------------------
    Pitch shifter berbasis PSOLA (pitch-synchronous overlap-add) untuk
    suara vokal. Berbeda dengan shifter granular lama (jendela tetap
    1024 sampel -> dengung ~43 Hz, terdengar "robot"), di sini:

      * setiap grain panjangnya tepat 2 periode nada penyanyi dan
        diambil sinkron dengan fase gelombangnya (dikunci lewat
        korelasi antar-grain),
      * grain disusun ulang dengan jarak periode/rasio, sehingga nada
        berubah TAPI formant (warna vokal "a/i/u/e/o") tidak ikut
        bergeser -> tidak terdengar seperti chipmunk / robot,
      * bagian tak-bernada (napas, "s", "t", "k") tidak digeser sama
        sekali, hanya diteruskan (overlap-add 50%),
      * ada detune halus + delay kecil per suara (diatur dari luar)
        supaya terdengar seperti penyanyi kedua, bukan salinan sempurna.

    Pemakaian: panggil pushDrySample(sampel, periodeSampel) lalu
    renderOutputSample() sekali per sampel (selalu berpasangan).
    periodeSampel = 0 berarti tidak bernada (unvoiced).
    Latensi tetap = getLatencySamples().
*/
class PsolaPitchShifter
{
public:
    void prepare (double sampleRate, int maxHalfPeriodSamples = 512);
    void reset();

    // Semitone, positif = naik. Dihaluskan ~8 ms secara internal.
    void setPitchShiftSemitones (float semitones) noexcept { targetSemitones = semitones; }
    void setDetuneCents (float cents) noexcept { detuneCents = cents; }
    void setExtraDelayMs (float ms) noexcept;

    void pushDrySample (float sample, float periodSamples) noexcept;
    float renderOutputSample() noexcept;

    int getLatencySamples() const noexcept { return latency; }

private:
    void placeGrain();
    float inAt (int64_t absIndex) const noexcept;

    static constexpr int kInLen  = 16384;
    static constexpr int kOutLen = 4096;
    static constexpr int64_t kInMask  = kInLen - 1;
    static constexpr int64_t kOutMask = kOutLen - 1;

    std::vector<float> inBuf, periodBuf, outBuf;
    std::vector<float> refSegment, candSegment;

    double sampleRate = 44100.0;
    int maxHalfPeriod = 512;
    int minHalfPeriod = 36;
    int unvoicedHalf = 256;
    int latency = 1168;

    float extraDelayMs = 0.0f;
    int64_t extraDelaySamples = 0;

    int64_t inCount = 0;
    int64_t outCount = 0;
    double  nextSynth = 0.0;

    bool    lastMarkValid = false;
    int64_t lastMark = 0;

    float targetSemitones = 0.0f;
    float currentSemitones = 0.0f;
    float detuneCents = 0.0f;
    float glideCoeff = 0.003f;
};
