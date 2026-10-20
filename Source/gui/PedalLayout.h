#pragma once

// Mirrors Assets/layout.json (the authoritative source - if that file is
// ever regenerated with different coordinates, update these to match).
namespace PedalLayout
{
    constexpr int bgWidth  = 680;
    constexpr int bgHeight = 934;

    // Ukuran GUI: koordinat di bawah ini adalah koordinat desain (680x934, 100%).
    // Editor menskalakannya; default dibuka di 60% (~408x560).
    constexpr double defaultScale = 0.60;
    constexpr double minScale     = 0.40;
    constexpr double maxScale     = 1.20;

    struct Rect { int x, y, w, h; };

    constexpr Rect harmonyKnob   { 78,  169, 200, 200 };
    constexpr Rect levelKnob     { 240, 171, 200, 200 };
    constexpr Rect reverbKnob    { 401, 169, 200, 200 };
    constexpr Rect mixInLKnob    { 410, 474, 124, 124 };

    constexpr Rect toneButton    { 448, 369, 104, 104 };
    constexpr Rect refOutButton  { 118, 380, 100, 100 };
    constexpr Rect footswitch    { 250, 747, 168, 168 };

    constexpr Rect ledStatusFx   { 308, 500, 64, 64 };
    constexpr Rect ledPower      { 508, 100, 64, 64 };
    constexpr Rect ledRefOut     { 298, 400, 64, 64 };   // LED amber di kanan tulisan "REF OUT"
}
