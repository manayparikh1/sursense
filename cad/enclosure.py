#!/usr/bin/env python3
"""
SurSense enclosure. 70mm puck, 22mm tall.

Generates three STEP files plus STLs for printing:
  lower_shell  - holds the battery and the speaker, USB-C cutout
  upper_shell  - display recess, mic port, bezel drive window
  bezel        - the knurled ring that turns the encoder

Z datum is the PCB top face, matching docs/enclosure-section.png:
  +12.4  top of upper shell
  +3.9   underside of the upper shell lip
   0.00  PCB top face
  -9.6   bottom of lower shell

Run:
  pip install cadquery
  python3 enclosure.py
"""

import cadquery as cq

# ---------------------------------------------------------------- dimensions

OD = 70.0                # overall diameter
WALL = 2.0               # shell wall thickness
FLOOR = 1.4              # floor and ceiling thickness

UPPER_H = 12.4           # upper shell height above the PCB
LOWER_H = 9.6            # lower shell depth below the PCB

BEZEL_OD = 70.0
BEZEL_ID = 64.0          # drive land the friction wheel presses on
BEZEL_H = 4.6
SHELL_OD_UNDER_BAND = 63.4   # leaves 0.30-0.45 radial gap
KNURL_COUNT = 90
KNURL_DEPTH = 0.45

TOP_PLATE = 3.0          # thickness of the upper shell top plate
CAVITY_R = 33.0          # inner radius below the bezel band
BAND_CAVITY_R = 29.7     # inner radius inside the band, keeps a 2mm wall

LENS_OD = 40.3           # display lens rebate
LENS_DEPTH = 1.7
WINDOW_OD = 37.5         # the hole the display actually shows through

MIC_PORT_D = 1.2         # through hole in the top plate
MIC_SEAT_D = 6.2         # gasket seat around it
MIC_SEAT_DEPTH = 0.6
MIC_R = 25.0             # radius of the mic port, at 180 degrees

SPK_W, SPK_L = 14.0, 24.0    # driver is 24 x 14, fires down
SPK_R = 17.0                 # centre radius, at 0 degrees
SPK_SLOTS = 7

USB_W, USB_H = 9.4, 3.6      # USB-C cutout
USB_R = OD / 2               # on the rim, at 315 degrees

WHEEL_WIN_W, WHEEL_WIN_H = 24.0, 5.2   # window the friction wheel reaches through
WHEEL_R = OD / 2                        # at 90 degrees

BOSS_OD, BOSS_ID, BOSS_H = 5.6, 3.2, 4.2   # M2 heat-set bosses
BOSS_R = 29.0
BOSS_ANGLES = [40, 130, 250, 300]

STANDOFF_OD, STANDOFF_ID, STANDOFF_H = 4.4, 2.2, 6.0   # display standoffs
STANDOFF_R = 21.5
STANDOFF_ANGLES = [180, 50, 310]


def _polar(r, deg):
    """Cartesian coords for a radius and an angle in degrees."""
    import math
    a = math.radians(deg)
    return r * math.cos(a), r * math.sin(a)


# ------------------------------------------------------------- lower shell

def lower_shell():
    """Bowl below the PCB. Battery, speaker grille, USB-C."""

    body = cq.Workplane("XY").circle(OD / 2).extrude(-LOWER_H)

    # hollow it out, leaving a floor
    body = body.faces(">Z").workplane().circle(OD / 2 - WALL).cutBlind(-(LOWER_H - FLOOR))

    # speaker grille: seven slots in the floor at 0 degrees
    cx, cy = _polar(SPK_R, 0)
    slot_w = 1.6
    pitch = SPK_L / SPK_SLOTS
    for i in range(SPK_SLOTS):
        y = cy - SPK_L / 2 + pitch * (i + 0.5)
        body = (
            body.faces("<Z").workplane(origin=(0, 0, 0))
            .center(cx, y)
            .rect(SPK_W, slot_w)
            .cutThruAll()
        )

    # USB-C cutout through the rim at 315 degrees
    ux, uy = _polar(USB_R, 315)
    body = (
        body.faces(">Z").workplane(origin=(0, 0, -LOWER_H / 2))
        .center(ux, uy)
        .rect(USB_W, USB_W)
        .cutBlind(-USB_H)
    )

    # M2 heat-set bosses standing up from the floor
    for ang in BOSS_ANGLES:
        bx, by = _polar(BOSS_R, ang)
        boss = (
            cq.Workplane("XY", origin=(bx, by, -LOWER_H + FLOOR))
            .circle(BOSS_OD / 2).extrude(BOSS_H)
            .faces(">Z").workplane().circle(BOSS_ID / 2).cutBlind(-BOSS_H)
        )
        body = body.union(boss)

    return body


# ------------------------------------------------------------- upper shell

def upper_shell():
    """Cap above the PCB. Display lens rebate, mic port, bezel window.

    Built as a revolved cross-section so the wall thickness stays consistent
    where the shell steps in under the bezel band.
    """

    band_z = UPPER_H - BEZEL_H          # 7.8, where the shell steps in
    ceiling_z = UPPER_H - TOP_PLATE     # 9.4, underside of the top plate
    rebate_z = UPPER_H - LENS_DEPTH     # 10.7, floor of the lens rebate

    # cross-section of the solid, in the XZ half plane
    profile = [
        (CAVITY_R, 0.0),
        (OD / 2, 0.0),
        (OD / 2, band_z),
        (SHELL_OD_UNDER_BAND / 2, band_z),
        (SHELL_OD_UNDER_BAND / 2, UPPER_H),
        (LENS_OD / 2, UPPER_H),
        (LENS_OD / 2, rebate_z),
        (WINDOW_OD / 2, rebate_z),
        (WINDOW_OD / 2, ceiling_z),
        (BAND_CAVITY_R, ceiling_z),
        (BAND_CAVITY_R, band_z),
        (CAVITY_R, band_z),
    ]

    body = (
        cq.Workplane("XZ")
        .polyline(profile).close()
        .revolve(360, (0, 0, 0), (0, 1, 0))
    )

    # mic port at 180 degrees, through the top plate, with a gasket seat below
    mx, my = _polar(MIC_R, 180)
    body = (
        body.faces(">Z").workplane(origin=(0, 0, UPPER_H))
        .center(mx, my).circle(MIC_PORT_D / 2)
        .cutBlind(-(UPPER_H - ceiling_z))
    )
    seat = (
        cq.Workplane("XY", origin=(mx, my, ceiling_z))
        .circle(MIC_SEAT_D / 2).extrude(MIC_SEAT_DEPTH)
    )
    body = body.cut(seat)

    # window in the band wall for the friction wheel at 90 degrees.
    # cut only through the wall, not across the whole part
    wx, wy = _polar((SHELL_OD_UNDER_BAND / 2 + BAND_CAVITY_R) / 2, 90)
    window = (
        cq.Workplane("XY", origin=(wx, wy, band_z + (BEZEL_H - WHEEL_WIN_H) / 2))
        .rect(WHEEL_WIN_W, SHELL_OD_UNDER_BAND / 2 - BAND_CAVITY_R + 2.0)
        .extrude(WHEEL_WIN_H)
    )
    body = body.cut(window)

    # display standoffs hanging down from the ceiling
    for ang in STANDOFF_ANGLES:
        sx, sy = _polar(STANDOFF_R, ang)
        post = (
            cq.Workplane("XY", origin=(sx, sy, ceiling_z - STANDOFF_H))
            .circle(STANDOFF_OD / 2).extrude(STANDOFF_H)
        )
        body = body.union(post)
        bore = (
            cq.Workplane("XY", origin=(sx, sy, ceiling_z - STANDOFF_H))
            .circle(STANDOFF_ID / 2).extrude(STANDOFF_H)
        )
        body = body.cut(bore)

    # clearance holes for the M2 screws coming up from below
    for ang in BOSS_ANGLES:
        bx, by = _polar(BOSS_R, ang)
        hole = (
            cq.Workplane("XY", origin=(bx, by, 0))
            .circle(1.2).extrude(UPPER_H)
        )
        body = body.cut(hole)

    return body


# ------------------------------------------------------------------ bezel

def bezel():
    """Knurled ring. Turns freely, drives the EC11 through a friction wheel."""

    ring = (
        cq.Workplane("XY")
        .circle(BEZEL_OD / 2).circle(BEZEL_ID / 2)
        .extrude(BEZEL_H)
    )

    # knurl the outside: shallow flutes cut around the rim
    import math
    flute_r = 0.55
    for i in range(KNURL_COUNT):
        ang = 360.0 / KNURL_COUNT * i
        fx, fy = _polar(BEZEL_OD / 2 + flute_r - KNURL_DEPTH, ang)
        cutter = (
            cq.Workplane("XY", origin=(fx, fy, 0))
            .circle(flute_r).extrude(BEZEL_H)
        )
        ring = ring.cut(cutter)

    return ring


# ------------------------------------------------------------------- main

if __name__ == "__main__":
    import os
    out = os.path.dirname(os.path.abspath(__file__))
    prod = os.path.join(os.path.dirname(out), "production", "STEP")
    os.makedirs(prod, exist_ok=True)

    parts = {
        "lower_shell": lower_shell(),
        "upper_shell": upper_shell(),
        "bezel": bezel(),
    }

    for name, part in parts.items():
        step = os.path.join(prod, f"{name}.step")
        stl = os.path.join(prod, f"{name}.stl")
        cq.exporters.export(part, step)
        cq.exporters.export(part, stl)
        vol = part.val().Volume() / 1000.0
        print(f"{name:14s} {vol:8.2f} cm3   -> {os.path.basename(step)}, {os.path.basename(stl)}")

    print(f"\nWritten to {prod}")
