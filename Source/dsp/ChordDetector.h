#pragma once
#include <vector>
#include <cstdint>

/**
    ChordDetector
    --------------
    Mendeteksi AKOR dari sinyal polifonik (keyboard / gitar / musik) pada input 2,
    mirip "guitar/keyboard-controlled harmony" TC-Helicon:

      jendela 8192 sampel (Hann) -> energi tiap nada C3..C6 (Goertzel) -> chroma 12 kelas nada
      -> dicocokkan ke template akor (maj, min, 7, maj7, m7, dim) pada 12 akar.

    Ada hysteresis (akor baru harus menang dua jendela berturut-turut) dan masa tahan
    saat sinyal hening, supaya harmoni tidak loncat-loncat dan tidak hilang di jeda singkat.
*/
class ChordDetector
{
public:
    enum Quality { Major = 0, Minor, Dominant7, Major7, Minor7, Diminished, NumQualities };

    void prepare (double sampleRate);
    void reset();

    void pushSamples (const float* mono, int numSamples) noexcept;

    bool hasChord() const noexcept        { return chordValid; }
    int  getRoot() const noexcept         { return chordRoot; }        // 0=C .. 11=B
    int  getQuality() const noexcept      { return chordQuality; }
    uint16_t getChordMask() const noexcept { return chordMask; }       // bit pc = nada akor

    static uint16_t maskFor (int root, int quality) noexcept;

private:
    void analyse();

    static constexpr int kWindow = 8192;
    static constexpr int kHop    = 2048;
    static constexpr int kFirstMidi = 43;   // G2
    static constexpr int kLastMidi  = 84;   // C6
    static constexpr int kNumBins   = kLastMidi - kFirstMidi + 1;

    double sampleRate = 44100.0;

    std::vector<float> ring;      // kWindow
    std::vector<float> window;    // Hann
    std::vector<float> frame;     // salinan terurut untuk dianalisis
    float binCoeff[kNumBins] = {};
    int    binPitchClass[kNumBins] = {};
    float  binWeight[kNumBins] = {};

    int writePos = 0;
    int sinceAnalysis = 0;

    float chroma[12] = {};        // dihaluskan antar jendela

    bool chordValid = false;
    int  chordRoot = 0;
    int  chordQuality = 0;
    uint16_t chordMask = 0;
    float chordScore = 0.0f;

    int  pendingRoot = -1, pendingQuality = -1, pendingCount = 0;
    int  silentHops = 0;
};
