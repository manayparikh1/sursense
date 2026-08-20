# SurSense Production Files

This folder contains everything needed to manufacture the device.

## Folder structure

- `gerbers/` — PCB manufacturing files (Gerber format, ready for JLCPCB)
- `STEP/` — 3D enclosure models (ready for 3D printing or machining)
- `firmware/` — Compiled firmware (if binary build is needed)

## To manufacture

1. **PCB:** Send files in `gerbers/` to JLCPCB. 2-layer, 70mm round, 1.6mm thick.
2. **Case:** Print `STEP/upper-shell.step` and `STEP/lower-shell.step` in PLA or PETG on a 0.4mm nozzle.
3. **Firmware:** Flash the compiled binary to the ESP32-S3 using the Arduino IDE or esptool.

## Files pending

These need to be generated from the design in `hardware/` and the CAD in `cad/`:

- [ ] gerbers/*.gbr — Export from KiCad
- [ ] STEP/upper-shell.step — Export from CadQuery/Fusion
- [ ] STEP/lower-shell.step
- [ ] firmware/*.bin — Compile sur.cpp + index.html for the ESP32-S3

## Assembly notes

1. Solder components to the PCB following the placement table
2. Glue the display lens (PETG) into the upper shell with clear epoxy
3. Assemble the bezel coupling (choose O-ring or silicone sleeve variant)
4. Insert four M2 heat-set inserts into the case
5. Flash firmware via USB-C
6. Screw shells together with M2 × 6 screws
