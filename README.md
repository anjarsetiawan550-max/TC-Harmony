# TC HELICON by LR

Plugin harmoni vokal real-time (VST3 / AU / Standalone), dibangun dengan
[JUCE](https://juce.com) 7, dan GUI-nya memakai paket aset foto yang kamu
kirim (`Assets/harmony_singer_gui_assets`).

Proyek ini **dibuild lewat GitHub Actions**, bukan di sandbox ini — karena
mem-build plugin JUCE butuh men-download framework JUCE + compiler native
(MSVC/Xcode/GCC) yang tidak tersedia offline di sini. Begitu kamu push
folder ini ke sebuah repo GitHub, workflow `.github/workflows/build.yml`
otomatis jalan dan menghasilkan file VST3 (Windows/macOS/Linux) + AU
(macOS) sebagai *artifact* yang bisa di-download.

## Cara pakai (GitHub)

1. Buat repo baru di GitHub (public atau private).
2. Upload/push **seluruh isi folder `HarmonySingerVST/`** (bukan cuma
   sebagian) ke root repo tersebut.
3. Buka tab **Actions** di repo → workflow "Build TC HELICON by LR" akan
   otomatis berjalan setiap push ke `main`/`master`. Bisa juga dipicu
   manual lewat tombol **Run workflow**.
4. Setelah selesai (~5-10 menit), buka run tersebut → bagian
   **Artifacts** → download `TC-HELICON-by-LR-Windows`,
   `TC-HELICON-by-LR-macOS`, atau `TC-HELICON-by-LR-Linux`.
5. Ekstrak, lalu salin file `.vst3` (folder) ke:
   - Windows: `C:\Program Files\Common Files\VST3`
   - macOS: `~/Library/Audio/Plug-Ins/VST3` (atau `/Library/Audio/Plug-Ins/VST3`)
   - Linux: `~/.vst3`
   File `.component` (AU, khusus macOS) disalin ke
   `~/Library/Audio/Plug-Ins/Components`.
6. (Opsional) Beri tag `v1.0.0` lalu push tag itu → workflow juga akan
   membuat **GitHub Release** berisi hasil build yang sudah di-zip.

## Build manual / lokal (opsional)

Kalau kamu punya CMake + compiler native terpasang:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

Hasilnya ada di `build/HarmonySingerByLR_artefacts/Release/`.

## Ukuran GUI & ikon

- GUI dibuka di ukuran 60% (~408x560 px). Bisa diperbesar/diperkecil
  dengan menarik sudut kanan-bawah jendela (rentang 40%-120%, rasio tetap).
  Ukuran terakhir ikut tersimpan di state plugin.
  Ubah default/min/max di `Source/gui/PedalLayout.h`
  (`defaultScale`, `minScale`, `maxScale`).
- Ikon aplikasi Standalone dibuat dari gambar hasil jadi GUI
  (`Icons/icon_big.png`, `Icons/icon_small.png`; versi `.ico` ada di
  `Icons/icon.ico`), dipasang lewat `ICON_BIG`/`ICON_SMALL` di `CMakeLists.txt`.

## Routing audio (sesuai spesifikasi)

- **Input 1 (kiri)** = mic / vokal kamu → diproses (Adaptive Tone +
  Harmony) → **hanya keluar di Output 1 / L**.
- **Input 2 (kanan)** = referensi/gitar → (di-gate oleh REF OUT) →
  **hanya keluar di Output 2 / R**.
- Pasang plugin ini pada track/bus **stereo** di DAW-mu, dengan input 1
  dari mic dan input 2 dari gitar/instrumen referensi (mis. lewat
  interface audio 2 channel, atau dua track mono yang di-bounce ke satu
  track stereo sebelum masuk plugin ini).

## Kontrol

| Kontrol | Fungsi |
|---|---|
| **HARMONY** (8 posisi) | High / Higher / High & Higher / Low / Lower / Low & Lower / Higher & Lower / High & Low |
| **LEVEL** | Volume keluaran suara harmoni |
| **REVERB** (4 posisi) | Off / Room / Club / Hall |
| **MIX IN 1/L** | Crossfade di Output L: 0% = hanya suara asli mic, **100% = hanya suara harmoni (tanpa suara asli / tanpa nada "DO")**. Default 100%. Footswitch OFF = bypass (suara asli saja) |
| **TONE** (tombol + LED merah) | Nyala/mati **Adaptive Tone** (EQ adaptif + Kompresor + De-esser + Noise-gate) pada vokal asli, bukan suara harmoni |
| **Footswitch** | Nyala/mati efek harmoni (bypass) |
| **Ctrl + klik Footswitch** (Mac: Cmd + klik; atau **klik kanan**) | Ganti voicing Tone: Normal ⇄ Less Bright. Muncul pesan singkat "TONE: ..." di panel. Hanya terdengar kalau tombol TONE menyala |
| **REF OUT** | Nyala = suara referensi terdengar di Output 2/R, mati = Output 2/R senyap. **LED amber** di kanan tulisan REF OUT ikut menyala/mati |

## Cara kerja singkat

1. **YIN pitch detector** (jendela ~23 ms) membaca nada vokal kamu (dan,
   terpisah, nada dari input referensi). Nada yang dipegang penyanyi
   dikunci dengan hysteresis, jadi vibrato tidak membuat interval harmoni
   loncat antara mayor/minor.
2. Nada dari input referensi dipakai untuk menaksir **kunci (key)** lagu
   secara statistik (histogram pitch-class yang meluruh perlahan; melodi
   penyanyi ikut jadi bukti tambahan, jadi kunci tetap terbentuk walau
   input 2 kosong), supaya
   interval harmoni (3/4/5/6) jatuh pada nada yang *diatonically correct*
   di kunci tersebut — bukan selalu interval kromatik tetap. Sebelum
   kunci "terbentuk" (data belum cukup), engine memakai interval
   kromatik standar (C mayor) sebagai fallback supaya efek tetap bisa
   langsung dipakai.
3. **PSOLA pitch shifter** (pitch-synchronous overlap-add) menggeser
   nada vokal sesuai interval mode HARMONY. Tiap grain = 2 periode nada
   dan dikunci ke fase gelombang, lalu disusun ulang dengan jarak baru —
   nada berubah tapi *formant* (warna vokal) tetap, jadi terdengar seperti
   orang bernyanyi, bukan robot. Bagian tak-bernada (napas, "s", "t")
   tidak digeser. Tiap suara harmoni punya detune halus yang melayang
   pelan (+-6 sen +- 4) dan delay kecil (9 / 15 ms) seperti penyanyi
   kedua. Latensi 1168 sampel (~26 ms @ 44.1 kHz) dilaporkan ke DAW (PDC).
4. **Adaptive Tone** memproses vokal asli sebelum di-harmonisasi:
   Noise-gate → EQ adaptif → Compressor → De-esser.
5. Reverb (Room/Club/Hall) diterapkan pada bus suara harmoni saja, lalu
   semuanya dicampur lewat MIX IN 1/L ke Output L. Output R murni dari
   input referensi (on/off lewat REF OUT).

## Status proyek / catatan jujur

- Kode ini ditulis lengkap dan konsisten dengan JUCE 7 API, tapi **belum
  pernah dikompilasi di lingkungan ini** (sandbox tidak punya akses
  internet/compiler native untuk mem-build JUCE). GitHub Actions akan
  jadi build pertama yang benar-benar mengkompilasinya — kalau ada error
  kompilasi kecil (typo, perbedaan versi JUCE, dsb.), cek log di tab
  Actions dan laporkan ke saya baris errornya, saya bantu perbaiki.
- Pitch shifter memakai PSOLA monofonik: sangat natural untuk interval
  kecil-menengah (3/4/5/6), tapi tetap bukan algoritma studio berbayar;
  pada nada yang sangat serak/bernapas atau deteksi pitch yang salah
  oktaf, kualitasnya bisa turun.
- Deteksi kunci dari input referensi mengasumsikan melodi/kunci mayor
  sederhana dan tidak mengenali akor kompleks — ini pengganti sederhana
  untuk fitur "guitar-controlled harmony" TC-Helicon yang sesungguhnya
  jauh lebih canggih.
- Gambar panel berasal dari foto produk TC Helicon Harmony Singer
  (lihat `Assets/README.md`) — untuk pemakaian pribadi/belajar, jangan
  didistribusikan komersial karena merek & desain milik pemiliknya.

## Struktur folder

```
CMakeLists.txt                  konfigurasi build (JUCE via FetchContent)
.github/workflows/build.yml     CI: build Win/macOS/Linux otomatis
Icons/                          ikon aplikasi Standalone (dari hasil jadi GUI)
Resources/                      PNG yang dikompilasi jadi BinaryData (dipakai plugin)
Assets/                         paket aset asli kamu (dokumentasi, referensi, layout.json)
Source/
  PluginProcessor.*             audio engine utama + routing + parameter
  PluginEditor.*                GUI, menempatkan kontrol sesuai Assets/layout.json
  Params.h                      definisi parameter (APVTS)
  dsp/
    YinPitchDetector.*          deteksi pitch (YIN)
    PsolaPitchShifter.*         pitch shifter PSOLA (formant terjaga)
    HarmonyEngine.*             mode harmoni, estimasi kunci, reverb
    AdaptiveTone.*              noise-gate, EQ, compressor, de-esser
  gui/
    FilmstripLookAndFeel.h      render knob dari filmstrip PNG
    ImageStateButton.h          tombol/LED berbasis state gambar
    PedalLayout.h                koordinat GUI (mirror Assets/layout.json)
```
