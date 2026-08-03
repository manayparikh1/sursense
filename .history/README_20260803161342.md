# SurSense

A small palm-sized device that listens to you when you play and it works out the scale and also gives you a drone of that scale to practice against.

Point it at an instrument and will name the tonic and the scale in both Western and Indian notation - "C# Kafi/Dorian" - then plays a tanpura drone tuned to that tonic. It also works as a chromatic tuner and a metronome with taal cycles like teental etc and a silient haptic mode is also integrated.

Every tuner tells you one nore. Almost nothing free tells you what key the whole piece is in and nothing does it with sargam and a drone that is attached.


## Status

The detection algorithm is written, builds, and is tested. The hardware is designed and specified but not yet built - this repo is the design and the working software that will run on it.

tonic 24/24

This is the self test for `sur/`: twelve synthesized scales per mode, one for each 
tonic, passed through the detection pipeline and verified against the solution.
## Test it
cd sur
clang++ -std=c++17 -O2 -Wall -Wextra -o selftest sur.cpp selftest.cpp
./selftest

Takes only a few seconds. Requires no special hardware, no libraries, no build system.

## How it works

1. FFT the input sound, detect peaks in its spectrum, with parabolic interpolation
   to ensure that the low tones fall on the correct semitones.
2. Map those peaks into 12 pitch classes, depending on the proximity of each peak
   to the center of a semitone. Summarize for a few seconds.
3. Compare the result with the Krumhansl-Schmuckler probe-tone profile for all 12
   rotations in order to determine the tonic pitch.
4. Resolve ambiguities of major-minor modality with a second accumulator,
   decaying more slowly: the tone class that remains active longest will be tonic.
5. Compare the rest of the spectrum with scale candidates.
6. Make the output decisions persistent with a locking state machine: lock
   easily but unlock reluctantly to avoid oscillations on the display screen.

   Step 4 is the part that is not from a paper. C major and A minor contain indentical notes so the correlation alone cannot really seperate them.

Measuring which pitch class is *sustained* rather than merely present resolves it. With the bias
disabled and note durations equalised, accuracy drops from 20/24 to 3/24 — so
it's carrying real weight, not decoration.

## Honest Limits to this device

- The self-test is synthetic, and I wrote both the detector and the grader. It
  proves the implementation is correct, not that it survives real room audio.
- Short phrases are undecidable in principle. Five notes cannot distinguish
  Dorian from natural minor — the distinguishing degree was never played.
- Percussion adds broadband energy and will reduce accuracy.
- Nothing has run on hardware yet.

## HARDWARE

A 70mm puck will be used so basically a knurled metal bezel is the rotary encoder — the whole ring turns so you can set the tempo without looking while holding an instrument. It is kinda like my AC system which turns and you can set the temperature from it like that. 

BOM.csv is in sheets: https://docs.google.com/spreadsheets/d/1nKPMvtGOhtMLgeP8bByzdkwNMy8-5KtI2vqGwreh8vg/edit?usp=sharing

Pin assignments are in `HARDWARE.md`. The microphone and amplifier are on
separate I2S peripherals — the S3 has two, and this needs both.

## Known possible risks

-- **Feedback.** Mic and speaker are 70mm apart. If the drone plays while the detection runs the device may hear itself and lock onto its own output and this will be handled by mode exclusivity plus notching the known drone pitches.
- **Bezel coupling.** Driving an EC11 from a printed knurled ring is the most
  likely thing to feel rough in v1. Budget three revisions.
- **First PCB.** Even odds it needs a respin, which is why it gets ordered the
  day the breadboard prototype works.

  ## Documents Generated files architectured with the help of Claude AI.
- `HARDWARE.md` — BOM, form factor, build schedule
- `FIRMWARE.md` — toolchain, firmware architecture, feature roadmap
- `sur/` — the detector and its self-test

**OVERALL THIS WILL REALLY HELP MUSICIANS AND I HOPE I GET THE PARTS SO I CAN MAKE IT INTO A REAL PRODUCT**