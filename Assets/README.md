# Harmony Singer VST3 — Paket Asset GUI

Asset GUI dibuat langsung dari foto referensi (`reference/original_reference.jpg`, 1220x1175).
Foto asli TIDAK diubah. Semua knop, tombol, dan LED dipotong dari foto itu, lalu background
dibuat bersih (tanpa kontrol) supaya kontrol bisa digambar ulang oleh plugin.
Satu-satunya tambahan: knop **MIX IN 1/L** (klon knop Level, skala 0.62) beserta tick dan label-nya.

Resolusi 1x (680 x 934 px). Karena sumbernya foto, jangan diperbesar; kalau perlu GUI lebih kecil,
skalakan ke bawah saat render.

## Isi paket

```
layout.json                      koordinat semua asset (relatif terhadap background)
background/
  pedal_background_clean.png     BACKGROUND UTAMA (tanpa kontrol, ada knop Mix In: tick + label)
  pedal_background_clean_no_mixin.png   sama, tanpa tambahan Mix In
  pedal_full_original.png        pedal asli utuh (kontrol masih menempel) - hanya untuk pembanding
knobs/
  knob_harmony_8pos_strip.png    8 frame, 200x200 per frame, tersusun vertikal
  knob_level_64f_strip.png       64 frame, 200x200
  knob_reverb_4pos_strip.png     4 frame, 200x200 (OFF, ROOM, CLUB, HALL) + cincin abu-abu
  knob_mix_in_1L_64f_strip.png   64 frame, 124x124
buttons/
  footswitch_up.png / footswitch_down.png                 168x168
  refout_switch_up.png / refout_switch_down.png           100x100
  tone_button_led_on.png / tone_button_led_off.png        104x104 (LED merah menyatu di tombol)
  tone_button_led_on_pressed.png / tone_button_led_off_pressed.png
leds/
  led_status_red_on.png / led_status_red_off.png          64x64 (LED merah tengah = efek ON/OFF)
  led_power_green_on.png / led_power_green_off.png        64x64
reference/
  original_reference.jpg                   foto asli, tidak diubah
  side_by_side_original_vs_assets.png      kiri: asli, kanan: hasil rakitan asset
  preview_composite_assets.png             rakitan asset (posisi awal)
  preview_state_alt.png                    rakitan dengan state lain (Tone off, FX off, dll.)
  layout_annotated.png                     kotak + koordinat tiap asset
```

## Cara memasang (urutan layer)

1. Gambar `background/pedal_background_clean.png` pada (0,0), ukuran 680x934. PNG punya alpha
   (sudut pedal membulat), jadi window plugin sebaiknya transparan atau berwarna putih.
2. Letakkan tiap asset di `top_left` yang tertulis di `layout.json` (x, y dari pojok kiri atas background).

| Kontrol | top_left (x, y) | Ukuran | Keterangan |
|---|---|---|---|
| Knop HARMONY | 78, 169 | 200x200 | 8 posisi |
| Knop LEVEL | 240, 171 | 200x200 | 64 frame |
| Knop REVERB | 401, 169 | 200x200 | 4 posisi |
| Knop MIX IN 1/L | 410, 474 | 124x124 | 64 frame |
| Tombol TONE | 448, 369 | 104x104 | LED merah ada di dalam gambar |
| Tombol REF OUT | 118, 380 | 100x100 | up / down |
| Footswitch | 250, 747 | 168x168 | up / down |
| LED status FX | 308, 500 | 64x64 | on / off |
| LED power | 508, 100 | 64x64 | on / off |

Pusat tiap kontrol (untuk hit-test) ada di `layout.json` pada field `center` dan `hit_radius`.

## Memetakan parameter ke frame

**HARMONY** (8 frame, urut searah jarum jam mulai dari titik kiri-bawah; sudut tiap posisi ada di layout.json):

| Frame | Mode (usulan) |
|---|---|
| 0 | High (3 di atas) |
| 1 | Higher (5 di atas) |
| 2 | High & Higher (3 + 5 di atas) |
| 3 | Low (4 di bawah) |
| 4 | Lower (6 di bawah) |
| 5 | Low & Lower (4 + 6 di bawah) |
| 6 | Higher & Lower (5 di atas + 6 di bawah) |
| 7 | High & Low (3 di atas + 4 di bawah) |

Simbol titik di panel adalah bagian dari background dan tidak bisa diubah per mode. Jika urutan
simbol di panel ingin dicocokkan dengan mode, cukup tukar urutan array mode di kodemu.

**LEVEL** dan **MIX IN 1/L**: nilai 0..1 -> frame = round(nilai * 63). Frame 0 = minimum (kiri-bawah),
frame 63 = maksimum (kanan-bawah), sapuan 270 derajat.

**REVERB**: frame 0 = OFF, 1 = ROOM, 2 = CLUB, 3 = HALL. Jika plugin hanya punya 3 mode,
pakai frame 1-3, atau pakai OFF sebagai bypass reverb.

**TONE**: klik = toggle Adaptive Tone. Aktif -> `tone_button_led_on`, mati -> `tone_button_led_off`.
Saat mouse ditekan pakai varian `_pressed`. Ctrl + klik footswitch = ganti Normal / Less Bright.

**REF OUT**: `refout_switch_down` = ON (suara referensi keluar di output 2/R),
`refout_switch_up` = OFF (output 2/R di-mute). Silakan dibalik jika ingin.

**Footswitch**: `footswitch_down` hanya saat ditekan. Status efek ditunjukkan LED tengah:
`led_status_red_on` = harmoni aktif, `led_status_red_off` = bypass.

**LED power**: biasanya selalu `led_power_green_on`.

**Routing audio** (sesuai spesifikasi): input 1 / mic hanya keluar di output 1/L;
input 2 / referensi hanya keluar di output 2/R. Knop MIX IN 1/L mengatur level input 1/L.

## Contoh kode JUCE (filmstrip knob)

```cpp
struct FilmstripLnF : juce::LookAndFeel_V4
{
    juce::Image strip; int frameW = 200, frameH = 200;

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                           float pos, float, float, juce::Slider&) override
    {
        const int frames = strip.getHeight() / frameH;
        const int idx = juce::jlimit (0, frames - 1, (int) std::round (pos * (frames - 1)));
        g.drawImage (strip, x, y, w, h, 0, idx * frameH, frameW, frameH);
    }
};

// Harmony: Slider 0..7 interval 1; Reverb: 0..3 interval 1; Level/Mix: 0..1
// levelLnF.strip = juce::ImageCache::getFromMemory (BinaryData::knob_level_64f_strip_png,
//                                                   BinaryData::knob_level_64f_strip_pngSize);
// Posisikan setBounds() sesuai top_left dan ukuran pada tabel di atas.
```

Tombol dan LED: pakai `juce::ImageButton::setImages (...)` atau `g.drawImageAt (...)` pada `paint()`.
Dengan `juce_add_binary_data` (CMake) atau Projucer, tiap PNG otomatis tersedia di `BinaryData::`.

## Catatan jujur

- Knop diputar dengan merotasi detail cap (bar dan pointer), sedangkan arah cahaya dibuat tetap.
  Ini mendekati tampilan asli, tetapi bukan render 3D.
- Knop Mix In adalah klon knop Level yang diperkecil; tick dan font label ("MIX IN 1/L") digambar
  ulang dan mirip, belum identik dengan font asli TC Helicon.
- State "off" LED (merah tengah, power) dan "pressed"/"down" tombol dibuat otomatis (gelap/mengecil),
  karena foto asli hanya menampilkan satu state.
- Gambar bersumber dari foto produk TC Helicon Harmony Singer. Pakai untuk keperluan pribadi/belajar;
  jangan didistribusikan komersial karena merek dan desain milik pemiliknya.
