# SurSense

A palm-sized puck that listens to you play, works out what scale you're in, and drones
along with you.

Point it at an instrument and it names the tonic and the scale in both Western and Indian
notation, like "C# Kafi / Dorian", then plays a tanpura drone tuned to that tonic. It's
also a chromatic tuner and a metronome that knows taal cycles like teental, with a silent
haptic mode so you can use it in an ensemble.

Every tuner tells you one note. Almost nothing tells you what key the whole piece is in,
and nothing I've found does it with sargam and a drone attached.

## Status

The detection algorithm is written and tested. The hardware is designed but not built yet,
so right now this repo is the design plus the software that will run on it.

The self-test scores 24 out of 24 on tonic detection. It generates twelve synthesized
scales per mode, one for each tonic, runs them through the pipeline and checks the answer.

## Test it

```bash
cd sur && clang++ -std=c++17 -O2 -Wall -Wextra -o selftest sur.cpp selftest.cpp && ./selftest
```

Takes a few seconds. No hardware, no libraries, no build system. Compiles clean under
-Wall -Wextra.

## How it works

1. FFT the incoming audio and find the peaks, with parabolic interpolation so low notes
   land on the right semitone instead of smearing across two.
2. Fold those peaks into 12 pitch classes, weighted by how close each peak sits to the
   centre of a semitone. Accumulate over a few seconds.
3. Correlate against the Krumhansl-Schmuckler probe-tone profile at all 12 rotations to
   find the tonic.
4. Break major/minor ties with a second accumulator that decays more slowly. Whichever
   pitch class stays lit longest is the tonic.
5. Match what's left against the scale candidates.
6. Hold the answer with a locking state machine. Lock easily, unlock reluctantly, so the
   display isn't flickering between two guesses.

Step 4 is the bit I didn't get from a paper. C major and A minor use identical notes, so
correlation on its own can't tell them apart. What separates them is which note is being
*sustained* rather than just present.

I wanted to check that actually mattered, so I measured it. The self-test holds the tonic
for 0.6s and everything else for 0.3s, and that difference is exactly what the sustain bias
reads. As written it scores 24/24, and turning the bias off only drops it to 23/24, because
the timing cue is still there in other ways. Equalise every note to 0.3s and it falls to
21/24 with the bias on, and 14/24 with it off. So the bias is doing real work, but it's a
seven-point difference, not the difference between working and not working.

## What it can't do

The self-test is synthetic and I wrote both the detector and the grader, so it proves my
implementation matches my intent. It doesn't prove the thing survives a real room.

Short phrases are undecidable no matter how good the code is. Five notes can't tell Dorian
from natural minor if the note that distinguishes them never got played.

Percussion dumps broadband energy across every bin and will drag accuracy down.

And nothing has run on hardware yet.

## Hardware

70mm puck, 22mm tall. The knurled metal bezel is the rotary encoder, so the whole ring
turns and you can set tempo by feel without looking down while you're holding an
instrument. It's the same idea as the dial on my AC unit at home.

BOM.csv is also in a sheet:
https://docs.google.com/spreadsheets/d/1nKPMvtGOhtMLgeP8bByzdkwNMy8-5KtI2vqGwreh8vg/edit?usp=sharing

Total is $154.26 USD, converted from Amazon.ca listings at 1 CAD = 0.72 USD. Every link was
checked in stock on 14 Aug 2026.

One ESP32-S3, not three. The old BOM linked a 3-pack listing so it looked like the build
needed three processors. It needs one. A few other parts only sell in multipacks, so the
BOM lists pack size and board count separately now.

### Pin map

One ESP32-S3-WROOM-1-N8R2, 20 GPIO out of 44. Both I2S peripherals are in use, which is the
whole reason for the S3 over an S2 or a classic ESP32: the mic has to capture while the amp
drives the drone. Strapping pins 0, 3, 45 and 46 are left alone.

```
INMP441 mic  (I2S0)   SCK 4    WS 5     SD 6     L/R to GND
MAX98357A    (I2S1)   BCLK 15  LRC 16   DIN 7    SD 38
GC9A01       (SPI)    SCL 12   SDA 11   CS 10    DC 9   RST 8   BLK 14
EC11 encoder          A 17     B 18     SW 21
Buttons               Mode 13  Tap 2
Coin motor            41 (through a MOSFET)
VBAT sense            1  (through a divider)
```

SD on the amplifier runs off a GPIO instead of being hard-wired, so the amp can be muted
while detection is running. That's the main thing stopping the device from hearing its own
drone and locking onto it.

VBAT sense needs a divider. A full cell sits at 4.2V and the ADC tops out at 3.3V, so GPIO1
can't go straight to the battery. Two 100k resistors and a 100nF cap.

## Design

### Power and signal

![Power and signal](docs/power-and-signal.png)

### PCB placement

![PCB placement](docs/pcb-placement.png)

![Placement table](docs/placement-table.png)

### Enclosure

![Enclosure section](docs/enclosure-section.png)

### Bezel coupling

![Bezel coupling](docs/bezel-coupling.png)

![Tolerances](docs/tolerances.png)

## What I think will go wrong

Feedback. The mic and the speaker live in the same shell. If the drone plays while
detection is running the device can hear itself and lock onto its own output, which looks
like it's working perfectly and is actually a loop. Mode exclusivity plus notching out the
drone's own pitches should handle it.

The bezel. Driving an EC11 from a printed knurled ring is the part most likely to feel
rough in v1. I'm budgeting three revisions and printing two different couplings to compare.

The first PCB. Even odds it needs a respin, which is why it gets ordered the day the
breadboard version works rather than before.

## Files

BOM.csv is the parts list. sur/ has the detector and its self-test. docs/ has the design
drawings.

I really think this would help a lot of musicians and I'd love to actually build it.
