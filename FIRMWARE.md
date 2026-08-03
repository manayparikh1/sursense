# Sur — firmware setup and feature roadmap

## 1. Where you write it

**VS Code + the PlatformIO extension.** Install PlatformIO IDE from the extensions panel and
that's the whole setup — it downloads the ESP32-S3 toolchain, manages libraries, flashes the
board, and gives you a serial monitor, all inside VS Code. No Arduino IDE, no manual SDK.

`platformio.ini`:

```ini
[env:sur]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
monitor_speed = 115200
build_flags =
    -DBOARD_HAS_PSRAM
    -DARDUINO_USB_CDC_ON_BOOT=1
    -O2
lib_deps =
    lovyan03/LovyanGFX
    madhephaestus/ESP32Encoder

[env:native]
platform = native
build_flags = -std=c++17 -Ilib/sur_dsp
test_framework = unity
```

That second environment is the most important thing on this page. See §3.

**Libraries:**
- `LovyanGFX` — drives the GC9A01 over SPI with DMA. Faster and less fussy than TFT_eSPI.
- `ESP32Encoder` — hardware pulse-counter based, so the knob never misses a detent.
- I2S: use the ESP-IDF `i2s_std` driver directly. It's exposed under Arduino and the wrapper
  libraries only get in the way for full-duplex.
- FFT: skip `esp-dsp` at first. It's an IDF component and wiring it into an Arduino-framework
  project is a side quest. A plain radix-2 float FFT at 2048 points takes ~6 ms on a 240 MHz
  S3 — at 20 per second that's 12% of one core. Only reach for the optimized version if the
  profiler says you need it.

---

## 2. Repo layout

```
sur/
├── platformio.ini
├── lib/
│   └── sur_dsp/            ← PURE C++. No Arduino.h, no ESP headers.
│       ├── chroma.cpp/h        spectrum → 12 pitch classes
│       ├── key.cpp/h           Krumhansl-Schmuckler + scale matching
│       ├── lock.cpp/h          the lock state machine
│       └── tables.h            KS profiles, scale sets, sargam names
├── src/
│   ├── main.cpp            task setup, core pinning
│   ├── audio_in.cpp        I2S RX → ring buffer → FFT
│   ├── audio_out.cpp       I2S TX, drone + click synthesis
│   ├── modes/
│   │   ├── mode.h              the Mode interface (see §4)
│   │   ├── scale_finder.cpp
│   │   ├── tuner.cpp
│   │   ├── metronome.cpp
│   │   └── drone.cpp
│   ├── ui.cpp              rendering, encoder, buttons
│   └── settings.cpp        NVS persistence
└── test/
    └── test_dsp/           runs on your laptop, not the board
```

The hard rule: **nothing in `lib/sur_dsp/` includes an Arduino or ESP header.** Break that and
you lose §3.

---

## 3. Test the DSP on your laptop

```bash
pio test -e native
```

Because `sur_dsp` is plain C++, it compiles for your Mac. So port your browser self-test into
a native unit test: synthesize a scale in each of the 12 keys as a float buffer, run it through
the real `chroma → key → lock` code, assert the tonic comes back right.

```cpp
TEST_CASE("detects all twelve tonics") {
    int correct = 0;
    for (int tonic = 0; tonic < 12; tonic++) {
        auto samples = synth_scale(tonic, MINOR, 44100);
        if (detect_tonic(samples) == tonic) correct++;
    }
    TEST_ASSERT_GREATER_OR_EQUAL(10, correct);
}
```

This runs in about a second and needs no hardware, no mic, no flashing. You can tune a
threshold and know within seconds whether it helped. Without it you're changing a constant,
flashing for 30 seconds, whistling at a device, and guessing — which is how people burn a
weekend and give up.

It's also the same test as the browser version, which means you can prove the port didn't
break anything.

---

## 4. The architecture that lets you have every feature

You want a lot of features. The way to actually get them is not a longer to-do list — it's
making each one cost almost nothing to add. One interface:

```cpp
struct Mode {
    const char* name;
    void (*enter)();
    void (*audio)(const float* fft, const float* chroma, float pitchHz);
    void (*draw)(LGFX_Sprite& g);
    void (*encoder)(int delta);
    void (*button)(Button b, bool longPress);
};

static const Mode* MODES[] = {
    &scaleFinder, &tuner, &metronome, &drone,
};
```

Every mode gets the same inputs, already computed once on core 0: the spectrum, the chroma,
and the current pitch estimate. The mode button walks the array. Adding a feature is **one new
file and one array entry** — no changes to audio, no changes to the display driver, no risk of
breaking what already works.

Draw into an `LGFX_Sprite` and push it once per frame, not straight to the panel. Otherwise
you get visible tearing on the round display and it looks cheap.

---

## 5. The full feature menu

### Tier 1 — ship these first (they justify the object)
1. **Scale finder** — tonic + scale, Western and sargam
2. **Chromatic tuner** — cents, adjustable A4 415–466
3. **Metronome** — tap tempo, subdivisions, taal cycles
4. **Drone** — Sa, Sa+Pa, Sa+Ma, auto-tuned to the detected tonic

### Tier 2 — free, once tier 1 works (no new hardware, small code)
5. **Haptic beat** — silent metronome you feel. Long-press tap to toggle.
6. **Practice timer** — minutes per session, streak counter, stored in NVS
7. **Reference tone** — sustained A at your chosen A4
8. **Scale browser** — turn the bezel through every scale and thaat, hear each one
9. **Transposition helper** — "detected C♯, you want D — tune up 1 semitone"
10. **Session history** — last 8 detected scales with timestamps

### Tier 3 — the interesting ones (real features, moderate work)
11. **Intonation trainer** — shows a target note, listens, scores you in cents over 10 seconds.
    The pitch detector already exists; this is mostly UI.
12. **Rhythm trainer** — metronome runs, you tap, it reports your average error in milliseconds
    and whether you rush or drag. Musicians love being told this and no cheap metronome does it.
13. **Ear trainer** — plays two notes, you name the interval with the bezel. Uses the synth
    you already wrote for the drone.
14. **Phrase capture** — keep the last 30 seconds of pitch track in PSRAM. Press a button and
    it shows the note sequence you just played. Not full transcription, but genuinely useful
    for "what did I just improvise?"
15. **Karplus-Strong tanpura** — replace the additive drone with a plucked-string model, four
    strings, staggered plucks. Sounds dramatically more like a real tanpura and it's about 30
    lines of DSP. This is the one that will make musicians want it.

### Tier 4 — connectivity (the ESP32-S3 already has the radios, so it's firmware only)
16. **BLE MIDI clock** — the device becomes a MIDI clock master. Tap a tempo on the puck and
    your DAW, looper, or drum machine locks to it. Genuinely useful, and "my hardware syncs
    Ableton" is a strong demo.
17. **Self-hosted practice log** — WiFi AP + a small web page served from the device showing
    your practice history and detected scales. No app, no account, no cloud.
18. **Ensemble sync** — two devices share a tempo over BLE, both pulse in time. One person taps
    the tempo, the whole section feels it.

### Tier 5 — needs hardware changes, so decide before you route the PCB
19. **3.5mm line-in** for electric instruments — one jack, a divider, and an ADC pin
20. **microSD** for recording clips — one SPI slot, share the display bus
21. **DRV2605L haptic driver + LRA** instead of a coin motor — a crisp tick instead of a buzz

Tiers 1–4 are all firmware. **Route pads for 19–21 even if you don't populate them.** An unused
footprint costs nothing on the PCB and saves a respin if you want the feature later.

---

## 6. Everything the parts can already do

Nothing below needs a part you aren't already buying. This is what's sitting unused in the
BOM if you only implement the obvious feature for each component.

### ESP32-S3

| Silicon block | Obvious use | What else it unlocks |
|---|---|---|
| **Native USB OTG** | Flashing | **USB MIDI device.** Plug into a laptop and it appears as a MIDI controller with no drivers. The bezel sends CC, detected notes send note-on. Also USB mass storage — drag recorded clips off like a flash drive. This is the single biggest unused capability in the BOM. |
| **WiFi** | — | Self-hosted practice log at `sur.local`, NTP clock, OTA firmware updates so you never open the case again |
| **BLE 5** | — | BLE MIDI clock to a DAW or pedal; two devices sharing a tempo; phone as a second screen |
| **14 capacitive touch GPIOs** | — | **Wire one to the aluminium bezel.** The metal ring becomes a touch sensor — palm it to mute, tap it to lock a reading, touch-and-turn for fine adjust. Free, because the ring is already conductive. |
| **Second core** | — | Runs all DSP without ever stuttering the display |
| **2 MB PSRAM** | — | ~30 s of 16-bit mono audio, or several minutes of pitch track, held in a rolling buffer |
| **ADC** | Battery sense | Line-in from an instrument jack if you add one |
| **RTC + deep sleep** | — | Wakes on button in microseconds; keeps the practice clock running while off |
| **ULP coprocessor** | — | Can watch the button while the main cores sleep, for months of standby |

### INMP441 microphone

Its job is the scale finder. It can also do:

- **Clap tempo** — set BPM by clapping four times, hands free, without touching the device. For
  anyone holding an instrument this is better than a tap button.
- **Tone quality meter** — you already compute the spectrum. The ratio of harmonic energy to the
  fundamental is a real measure of tone: thin and reedy vs full and centred. Show it as a bar.
  Wind and brass players will use this daily and nothing cheap offers it.
- **Vibrato analysis** — track the pitch estimate over time; its oscillation rate and depth are
  your vibrato in Hz and cents. Slow-and-wide vs fast-and-narrow, measured.
- **Attack timing** — onset detection against the metronome gives the rhythm trainer's numbers.
- **Auto-start** — device idles until it hears playing, then wakes into scale finder.
- **Room noise floor** — warn when it's too loud to detect reliably instead of silently failing.

### GC9A01 round display

Round is not a constraint here, it's the right shape:

- **Circle of fifths** — a wheel is literally what this screen is. Light up the detected key,
  dim the neighbours. Nothing renders more naturally on a 240×240 circle.
- **Chroma wheel** — the 12 pitch classes around the rim, brightness by energy. Your debug view
  becomes the most beautiful screen on the device.
- **Tuner dial** — a real needle sweeping an arc, the way a physical tuner looks.
- **Practice ring** — today's minutes filling a progress arc around the edge.
- **Taal cycle** — beats as dots around the circumference with sam marked, which is how a taal
  is actually conceived. A 16-beat teental as a ring is more legible than any grid.

### MAX98357A + speaker

- Drone, metronome click, reference tones (the obvious ones)
- **Playback** of the captured 30 s buffer — hear what you just played
- **Call and response** ear training — it plays a phrase, you play it back, the mic scores you
- **Pitch pipe** — sweep a continuous tone with the bezel to match by ear

### EC11 bezel and coin motor

- **Continuous pitch sweep** for shruti-box style tuning between semitones
- **Haptic subdivisions** — a strong pulse on sam, weak on the others, so you feel the whole
  taal cycle rather than just a beat
- **Silent countdown** — three pulses before the metronome starts
- **Haptic tuner** — pulses faster the further you are from the note, so you can tune with your
  eyes closed

---

## 7. What combinations unlock

The features nothing else has come from pairs, not parts:

- **Mic + PSRAM + display** → play a phrase, see it written out as notes
- **Mic + speaker** → call and response, and closed-loop intonation training
- **Round screen + chroma** → the circle of fifths, live, reacting to what you play
- **Native USB + detection** → detected notes stream into your DAW as MIDI while you improvise
- **BLE + haptics** → an ensemble where everyone feels the same click and the room stays silent
- **Touch bezel + mic** → palm the ring to freeze a reading mid-phrase without a button press

**If you want one more dollar of capability:** an LSM6DS3 IMU ($1.20, I2C). It gives you
conducting-gesture tempo (wave to set BPM), flip-to-mute, tap-to-wake, and auto-rotate. Add the
footprint even if you don't populate it.

---

## 8. The honest warning

"Most features possible" is the most common way hardware projects die. The failure isn't that
any one feature is hard — it's that with 20 half-finished modes there's no moment where the
thing works, so there's nothing to demo and nothing to feel good about.

Do it in this order instead:

1. Get **one** mode fully working end to end, on real hardware, in a case.
2. Then add modes, one at a time, each one finished before the next starts.
3. Keep `main` flashable and demoable at all times.

The mode registry in §4 is what makes this safe. At any point you can stop, and what you have
is a working product with N features rather than a broken one with N+8 half-features. If you
run out of time before the deadline, you ship tier 1 and a full tier-2 list of what's coming —
which reads far better than a device that boots into a crash.
