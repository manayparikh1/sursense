# Sur — hardware build plan

A palm-sized puck that sits on your music stand and does the four things a practicing
musician actually needs: **find the scale, tune, keep time, and hold a drone.**

The pitch in one line: *phone apps do these separately, badly, and interrupt you with
notifications. This is one object with a knob that does all four and never rings.*

---

## 1. Features

| # | Feature | Why it earns its place |
|---|---|---|
| 1 | **Scale finder** | The hero feature. Play a melody, it names the tonic and scale in Western + sargam. Nothing else on a music stand does this. |
| 2 | **Chromatic tuner** | Round screen becomes a needle dial. Adjustable A4 (415–466) for early-music and Indian tunings. |
| 3 | **Metronome** | Tap tempo, subdivisions, and taal cycles (teental 16, jhaptaal 10, rupak 7) — not just 4/4 like every cheap metronome. |
| 4 | **Haptic beat** | Silent mode: the puck pulses instead of clicking. Use it in an ensemble or with headphones on. This is the feature people will actually talk about. |
| 5 | **Tanpura / shruti drone** | Sa, Sa+Pa, Sa+Ma, at any pitch. Auto-tunes to whatever the scale finder just detected. |
| 6 | **Practice timer** | Logs minutes per session. Small feature, big for anyone practicing daily. |

Features 1 and 4 are the ones that make it fundable. 2, 3, 5 make it something you'd keep
on the stand.

---

## 2. Parts

| Part | Choice | ~Cost | Why this one |
|---|---|---|---|
| MCU | **ESP32-S3-WROOM-1** (N8R2) | $3.50 | Dual core 240 MHz, PSRAM, two I2S peripherals (mic in *and* audio out at once), and `esp-dsp` has a hand-optimized FFT. Cheapest part that can actually run this DSP. |
| Mic | **INMP441** I2S MEMS | $2.50 | Digital out — no op-amps, no analog noise, no ADC tuning. Wires straight to I2S. 24-bit, flat enough for chroma work. |
| Display | **GC9A01 1.28" round IPS** 240×240 | $7.00 | Round is the whole industrial-design idea: it reads as a tuner dial, not a tiny computer. SPI, fast enough for a 30 fps needle. |
| Audio out | **MAX98357A** I2S class-D amp | $2.50 | I2S in, speaker out, no DAC needed. Enough for a drone and a click. |
| Speaker | 8Ω 1W, 28mm | $1.50 | |
| Input | **EC11 rotary encoder** w/ push | $0.80 | One knob does BPM, menu, tonic. A knob is what makes it feel like an instrument instead of a gadget. |
| Buttons | 2× tactile 6mm | $0.20 | Mode, and tap-tempo. |
| Haptics | Coin vibration motor + MOSFET | $1.00 | Upgrade to a DRV2605L + LRA later if you want a sharp tick instead of a buzz. |
| Battery | 1000 mAh LiPo | $5.00 | ~8h runtime. |
| Charging | MCP73831 + USB-C receptacle | $2.00 | |
| PCB | 2-layer, JLCPCB, qty 5 | ~$4/ea | |
| Enclosure | 3D printed, 2 shells | ~$1 | |
| | **Total** | **~$31/unit** | |

At qty 5 you're around $130 for five working units, which is a sane funding ask.

**Substitutions if you want to move faster:** a Teensy 4.1 ($32) has 600 MHz and an audio
library that hands you FFT and I2S for free — much less firmware work, much worse cost story.
Use ESP32-S3 unless the DSP fights you.

---

## 3. Physical form

```
        ╭───────────────────╮
       ╱                     ╲          70mm diameter puck, 22mm tall
      │    ╭───────────╮      │         Round LCD centered under a
      │   ╱             ╲     │         circular acrylic/PETG lens
      │  │      C#       │    │
      │  │  Kafi / Dorian│    │         Knurled ring around the edge
      │  │   S R g m P   │    │         IS the encoder — you turn the
      │   ╲             ╱     │         whole bezel. This is the detail
      │    ╰───────────╯      │         that makes it feel expensive.
      │  ●                 ●  │
       ╲   mode        tap   ╱          Speaker grille + mic port on
        ╰───────────────────╯           the underside edge
              USB-C ─┘
```

Building the encoder into a knurled bezel is one printed part and one gear/friction coupling,
and it's the difference between "student project in a 3D printed box" and "product." If it
proves fiddly, fall back to a knob on the face — but try it.

**Enclosure:** print in matte PLA or PETG. Two shells, four M2 heat-set inserts. Put the mic
port on the edge with a small acoustic mesh, and keep it as far from the speaker as the
geometry allows.

---

## 4. The feedback problem (design around this early)

Your mic and your speaker are in the same 70mm object. If the drone is playing while the
scale finder is listening, the device hears itself and locks onto the drone's own pitch —
which will look like it's working perfectly and is actually a loop.

Three defenses, use all three:

1. **Mode exclusivity.** Detection and drone don't run at the same time. Simplest and it's
   also what the user expects.
2. **Spectral notch.** When the drone is on, you know exactly which pitch classes you're
   emitting — zero those bins before accumulating chroma.
3. **Physical.** Mic and speaker on opposite sides, gasket the mic port so it isn't coupled
   to the enclosure cavity.

Mention this in your writeup. Noticing a feedback path before building is exactly the kind
of thinking that separates a real hardware project from a parts list.

---

## 5. Firmware

Same algorithm as the web version — chroma → Krumhansl-Schmuckler → lock state machine.
Port it, don't redesign it. See `BLUEPRINT.md` §4 for the DSP spec.

**Will it fit?** Yes, with room to spare:

| Task | Cost | Budget |
|---|---|---|
| 2048-pt real FFT (`esp-dsp`, S3 optimized) | ~1.5 ms | at 20/sec = **3% CPU** |
| Chroma fold + accumulate | ~0.2 ms | negligible |
| KS correlation, 24 keys | ~0.05 ms | run at 5 Hz, free |
| Autocorrelation pitch (tuner) | ~8 ms naive | downsample 4× first → ~0.5 ms |
| Drone synthesis, 3 voices | per-sample | ~8% of one core |
| Display, 240×240 SPI @ 30 fps | DMA | mostly free |

**Core split:**

```
Core 0 (audio, hard real-time)          Core 1 (everything else)
────────────────────────────            ────────────────────────
I2S RX  → ring buffer                   UI state machine
FFT     → chroma accumulate             GC9A01 rendering
autocorr → pitch estimate               encoder + button handling
I2S TX  ← drone + click synthesis       practice timer
                                        settings in NVS
```

Talk between them with a FreeRTOS queue. Never touch SPI from core 0.

**Metronome timing — do this right, it's a differentiator.** Do *not* use `delay()` or a
software timer. Count samples in the I2S TX callback: at 44100 Hz, a beat at 90 BPM is
exactly 29400 samples. Schedule the click by sample index and your timing error is under
0.02 ms, permanently, with zero drift. Phone metronomes drift audibly over a few minutes
because of scheduler jitter. Yours won't, and that's a measurable claim you can put in the
README.

---

## 6. Build order

The classic hardware mistake is designing a PCB before the firmware works. Don't. Each phase
here ends with something that functions, so a failure at any point still leaves you with a
demo.

**Phase 1 — Breadboard (1 week).** ESP32-S3 dev board, INMP441, round LCD, MAX98357A, all on
jumper wires. Get I2S mic capture working, then the FFT, then chroma bars on the LCD. Ugly
and functional. *Checkpoint: it detects a scale you play into it.*

**Phase 2 — Prove it (3 days).** Port the self-test idea to hardware: play known scales at it
from a speaker and log accuracy over serial. Tune thresholds against that number. *Checkpoint:
a real accuracy figure, ≥10/12.*

**Phase 3 — All four modes (1 week).** Metronome with sample-accurate scheduling, tuner
needle, drone, mode switching on the encoder. *Checkpoint: usable as an actual practice tool,
still a mess of wires.*

**Phase 4 — PCB (1 week).** KiCad. 2-layer is fine. Route the I2S lines short and keep a
solid ground pour under them; keep the switching amp away from the mic traces. Order from
JLCPCB, expect ~2 weeks to arrive. *Checkpoint: gerbers submitted.*

**Phase 5 — Enclosure (parallel with PCB shipping).** Model in Fusion or OnShape around the
PCB outline you just finalized. Print, test fit, iterate. Expect three revisions — everyone
does.

**Phase 6 — Assemble and demo.** Solder, flash, film it working.

Total ≈ 4–5 weeks of evenings, plus PCB lead time. Order the PCB the moment Phase 3 is done
so it ships while you're printing the case.

---

## 7. What to submit

Hardware programs generally want to see the design files, not just a video — check your
program's exact requirements, but plan on:

- **KiCad schematic + PCB** in the repo, with gerbers
- **CAD files** for the enclosure (STEP, not just STL — STEP is editable)
- **BOM** with real part numbers and links
- **Firmware** repo with a README
- **A devlog** written as you go. Write down the things that went wrong — the mic port that
  coupled to the speaker cavity, the third case revision. Reviewers have seen a hundred
  polished summaries and very few honest ones.
- **Video** of it detecting a scale you play live, in one unbroken take.

Lead with the browser prototype. "I wrote the algorithm in JS, built a self-test harness that
scored it against synthesized scales in all 12 keys, tuned it to 11/12, then ported it to an
ESP32-S3" is a much stronger story than "I built a device." It shows you validated before you
committed money to hardware, which is the actual skill.
