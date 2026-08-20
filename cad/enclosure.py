#!/usr/bin/env python3
"""
SurSense enclosure — 70mm puck, 22mm tall
Generates STEP files for 3D printing or machining

Requirements:
  pip install cadquery

Usage:
  python3 enclosure.py
  # outputs: upper_shell.step, lower_shell.step
"""

try:
    import cadquery as cq
except ImportError:
    print("Install cadquery: pip install cadquery")
    exit(1)

# Dimensions (mm)
OD = 70.0          # Outer diameter
HEIGHT = 22.0      # Total height
THICKNESS = 2.0   # Shell wall thickness
BEZEL_HEIGHT = 3.0 # Knurled bezel band height

# Split height
UPPER_H = 12.4
LOWER_H = HEIGHT - UPPER_H  # 9.6mm

# Round PCB diameter at 1.6mm thick
PCB_D = 70.0
PCB_CLEARANCE = 0.5

# Display
DISPLAY_OD = 40.3
DISPLAY_DEPTH = 1.7
LENS_DEPTH = 1.5
LENS_OD = DISPLAY_OD + 0.4

# Mic port (180° south)
MIC_PORT_D = 6.2
MIC_PORT_DEPTH = 0.6
MIC_PORT_R = OD / 2 - 2.0

# Speaker grille (0° north)
SPEAKER_W = 24.0
SPEAKER_H = 14.0
SPEAKER_DEPTH = 4.5
SPEAKER_R = OD / 2 - 2.0

# Bezel
BEZEL_OD = OD
BEZEL_ID = OD - 5.0  # 1.5mm walls on the bezel band itself
KNURL_PITCH = 1.0

def upper_shell():
    """Upper shell with display recess, lens, and mic port"""

    # Start with the main dome
    base = cq.Workplane("XY").cylinder(UPPER_H, OD / 2)

    # Cut out the inside (hollow)
    inner = cq.Workplane("XY").cylinder(UPPER_H - THICKNESS, (OD / 2) - THICKNESS)
    shell = base.cut(inner)

    # Display recess on top (centered)
    shell = shell.faces(">Z").workplane().hole(LENS_OD, LENS_DEPTH, True)

    # Mic port at 180° (bottom edge of shell)
    shell = shell.faces(">Z").workplane().transformed(
        offset=(0, -MIC_PORT_R, 0)
    ).hole(MIC_PORT_D, MIC_PORT_DEPTH, True)

    # Bezel cutout (rectangular window at 90° for encoder shaft)
    # 24 x 5.2mm window at r 21.5
    shell = shell.faces(">Z").workplane().transformed(
        offset=(MIC_PORT_R, 0, 0)
    ).box(24.0, 5.2, THICKNESS * 2, centered=True)

    return shell

def lower_shell():
    """Lower shell with speaker grille and USB-C cutout"""

    # Main bowl
    base = cq.Workplane("XY").cylinder(LOWER_H, OD / 2)

    # Hollow out
    inner = cq.Workplane("XY").cylinder(LOWER_H - THICKNESS, (OD / 2) - THICKNESS)
    shell = base.cut(inner)

    # Speaker grille cutout at 0° (north, top of lower shell)
    # Seven vertical slots
    for i in range(7):
        x_offset = -SPEAKER_W/2 + (SPEAKER_W / 6) * i
        slot = cq.Workplane("XY").box(2, SPEAKER_H, THICKNESS * 2, centered=True)
        shell = shell.faces(">Z").workplane().transformed(
            offset=(SPEAKER_R - 1, x_offset, 0)
        ).rect(2, SPEAKER_H).extrude(-THICKNESS)

    # USB-C cutout at 315° (northwest, on the rim)
    # 16mm wide, 10mm tall, 3.4mm deep
    usb_w, usb_h = 16.0, 10.0
    shell = shell.faces(">Z").workplane().transformed(
        offset=(-OD/2 + 2, OD/2 - 2, 0)
    ).rect(usb_w, usb_h).extrude(-3.4)

    return shell

def bezel():
    """Knurled bezel ring — spins freely"""

    # Main ring
    ring = cq.Workplane("XY").cylinder(BEZEL_HEIGHT, BEZEL_OD / 2)
    inner = cq.Workplane("XY").cylinder(BEZEL_HEIGHT, BEZEL_ID / 2)
    ring = ring.cut(inner)

    # Knurl pattern (radial grooves, 90 teeth at 4° spacing)
    for angle in range(0, 360, 4):
        # Each tooth is a small wedge removed
        wedge = cq.Workplane("XY").box(0.5, 70, 5, centered=True)
        ring = ring.rotate((0, 0, 1), (0, 0, 0), angle).cut(wedge)

    # Center hole for EC11 shaft coupling (Ø6 D-shaft, Ø5.85)
    ring = ring.faces("<Z").workplane().hole(6.0, 2.0, True)

    return ring

if __name__ == "__main__":
    print("Generating SurSense enclosure...")

    upper = upper_shell()
    lower = lower_shell()
    bezel_part = bezel()

    cq.exporters.export(upper, "upper_shell.step")
    cq.exporters.export(lower, "lower_shell.step")
    cq.exporters.export(bezel_part, "bezel.step")

    print("✓ upper_shell.step")
    print("✓ lower_shell.step")
    print("✓ bezel.step")
    print("\nOpen in FreeCAD, Fusion 360, or Slic3r to review before printing")
