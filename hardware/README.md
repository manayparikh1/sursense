# SurSense PCB Design

## To build this project:

1. Download KiCad (free, kicad.org)
2. Open `sursense.kicad_pro` in KiCad
3. Create a new schematic and PCB layout using the pin map and placement guide in the main README
4. Export Gerbers to `../production/gerbers/`

## Pin assignments

All pins are documented in the main README under "Pin map".

## Board specs

- Round 2-layer board, 70mm diameter, 1.6mm thick
- Placement coordinates from `docs/placement-table.png`
- Power tree from `docs/power-and-signal.png`

## Design notes

- Mic port at 180°, speaker at 0° (opposite sides)
- EC11 encoder friction coupling at 90°
- WROOM-1 antenna clearance at 210°
- Ground pour under I2S traces
- Class-D amp isolated from MCU power rail

Send Gerbers to JLCPCB or similar 2-layer fab.
