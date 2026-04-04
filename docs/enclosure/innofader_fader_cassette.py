"""
Innofader Pro SC fader cassette for ScratchTJ MK2.

ADAPTER design -- mounts to the EXISTING base shell with no shell
changes.  The cassette bolts onto the stock 4-hole rectangular boss
pattern (M2, 62mm x 18mm) and internally holds the Innofader Pro SC
on its own 2-hole inline pattern (M3, 65mm).

The only manual mod needed on the shell is widening the fader stem
slot from 3.2mm to ~8mm with a file or dremel (2 minutes).

>>> PLACEHOLDER VALUES -- measure your actual fader with calipers and
>>> update before printing.  Every dimension marked # MEASURE is a
>>> best-guess from specs / community data that you must verify.
"""

from __future__ import annotations

from dataclasses import dataclass

try:
    import cadquery as cq
except ImportError:
    raise SystemExit(
        "cadquery is required -- install with:  pip install cadquery"
    )

from scratchtj_turntable_cadquery import (
    box_from_z,
    cut_all,
    cylinder_from_z,
    fuse_all,
    rounded_rect_prism,
)


# ---------------------------------------------------------------------------
# Parameters
# ---------------------------------------------------------------------------
@dataclass
class InnofaderCassetteParams:
    """All dims in mm.  Items marked MEASURE are placeholders."""

    # --- position on the enclosure (same as stock) -------------------------
    fader_center_x: float = 114.0
    fader_center_y: float = 18.0

    # --- stock shell boss pattern (DO NOT CHANGE -- matches base_shell) ----
    shell_boss_offset_x: float = 31.0     # from EnclosureParams.fader_boss_offset_x
    shell_boss_offset_y: float = 9.0      # from EnclosureParams.fader_boss_offset_y
    shell_hole_dia: float = 4.4           # from EnclosureParams.fader_flange_hole_d

    # --- Innofader PCB body ------------------------------------------------
    pcb_length: float = 73.0          # MEASURE  (along fader travel axis)
    pcb_width: float = 17.0           # MEASURE  (perpendicular to travel)
    pcb_height: float = 11.0          # MEASURE  (total component height)

    # --- Innofader mounting holes (2 inline, M3) ---------------------------
    innofader_hole_spacing: float = 65.0  # MEASURE  center-to-center
    innofader_hole_dia: float = 3.2       # M3 clearance hole
    innofader_standoff_od: float = 6.0    # standoff outer diameter
    innofader_standoff_h: float = 2.0     # MEASURE  height to lift PCB off floor

    # --- fader stem slot (top opening for the knob) ------------------------
    stem_slot_length: float = 42.0    # MEASURE  fader travel + clearance
    stem_slot_width: float = 8.0      # MEASURE  stem width + clearance
    stem_slot_r: float = 2.0          # corner radius

    # --- cassette outer shell ----------------------------------------------
    cassette_h: float = 15.0          # MEASURE  total cassette height
    flange_th: float = 3.0            # top flange thickness
    flange_x: float = 78.0            # flange length (same as stock)
    flange_y: float = 24.0            # flange width  (same as stock)
    floor_th: float = 2.0             # floor below the PCB cavity

    # --- inner cavity (PCB sits here) --------------------------------------
    cavity_length: float = 75.0       # MEASURE  pcb_length + 2mm clearance
    cavity_width: float = 19.0        # MEASURE  pcb_width + 2mm clearance
    cavity_corner_r: float = 2.0

    # --- tray (outer walls around cavity) ----------------------------------
    tray_length: float = 78.0         # outer tray length
    tray_width: float = 22.0          # outer tray width
    tray_corner_r: float = 3.0

    # --- cable slot --------------------------------------------------------
    cable_slot_x: float = 12.0        # width of cable exit
    cable_slot_h: float = 8.0         # height of cable exit
    cable_slot_z: float = 4.0         # offset from tray floor

    @property
    def shell_hole_positions(self) -> list[tuple[float, float]]:
        """Stock 4-hole rectangular pattern -- matches base_shell bosses."""
        cx, cy = self.fader_center_x, self.fader_center_y
        ox, oy = self.shell_boss_offset_x, self.shell_boss_offset_y
        return [
            (cx - ox, cy - oy),
            (cx + ox, cy - oy),
            (cx - ox, cy + oy),
            (cx + ox, cy + oy),
        ]

    @property
    def innofader_hole_positions(self) -> list[tuple[float, float]]:
        """Innofader 2-hole inline pattern along the travel axis."""
        half = self.innofader_hole_spacing / 2.0
        return [
            (self.fader_center_x - half, self.fader_center_y),
            (self.fader_center_x + half, self.fader_center_y),
        ]


# ---------------------------------------------------------------------------
# Builder
# ---------------------------------------------------------------------------
def build_innofader_cassette(p: InnofaderCassetteParams | None = None) -> "cq.Workplane":
    if p is None:
        p = InnofaderCassetteParams()

    body_h = p.cassette_h - p.flange_th

    # -- flange (top plate) -------------------------------------------------
    flange = rounded_rect_prism(
        p.fader_center_x,
        p.fader_center_y,
        body_h,
        p.flange_x,
        p.flange_y,
        4.0,
        p.flange_th,
    )

    # -- tray (main body below flange) --------------------------------------
    tray = rounded_rect_prism(
        p.fader_center_x,
        p.fader_center_y,
        0.0,
        p.tray_length,
        p.tray_width,
        p.tray_corner_r,
        body_h,
    )

    cassette = flange.union(tray)

    # -- M3 standoffs inside cavity for Innofader mounting ------------------
    for x, y in p.innofader_hole_positions:
        standoff = cylinder_from_z(
            x, y, p.floor_th, p.innofader_standoff_h, p.innofader_standoff_od
        )
        cassette = cassette.union(standoff)

    # -- cuts ---------------------------------------------------------------
    cuts: list[cq.Workplane] = []

    # 1) inner cavity for Innofader PCB
    cuts.append(
        rounded_rect_prism(
            p.fader_center_x,
            p.fader_center_y,
            p.floor_th,
            p.cavity_length,
            p.cavity_width,
            p.cavity_corner_r,
            p.cassette_h + 0.2,
        )
    )

    # 2) stem slot (through the flange for the fader knob)
    cuts.append(
        rounded_rect_prism(
            p.fader_center_x,
            p.fader_center_y,
            body_h - 0.1,
            p.stem_slot_length,
            p.stem_slot_width + 4.0,
            p.stem_slot_r,
            p.flange_th + 0.2,
        )
    )

    # 3) cable slot (exit at the back of the tray)
    cuts.append(
        box_from_z(
            p.fader_center_x,
            p.fader_center_y + p.tray_width / 2.0 - 1.2,
            p.cable_slot_z,
            p.cable_slot_x,
            4.0,
            p.cable_slot_h,
        )
    )

    # 4) stock shell mounting holes (4-hole rectangular, M2 clearance)
    for x, y in p.shell_hole_positions:
        cuts.append(
            cylinder_from_z(x, y, -0.1, p.cassette_h + 0.2, p.shell_hole_dia)
        )

    # 5) Innofader M3 mounting holes (through standoffs and floor)
    for x, y in p.innofader_hole_positions:
        cuts.append(
            cylinder_from_z(
                x, y, -0.1,
                p.floor_th + p.innofader_standoff_h + 0.2,
                p.innofader_hole_dia,
            )
        )

    return cut_all(cassette, cuts)


# ---------------------------------------------------------------------------
# CLI -- render + export STL
# ---------------------------------------------------------------------------
if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description="Render Innofader fader cassette")
    parser.add_argument("-o", "--output", default="stls/innofader_cassette.stl",
                        help="Output STL path (default: stls/innofader_cassette.stl)")
    args = parser.parse_args()

    params = InnofaderCassetteParams()
    result = build_innofader_cassette(params)

    from pathlib import Path
    out = Path(args.output)
    out.parent.mkdir(parents=True, exist_ok=True)
    cq.exporters.export(result, str(out))
    print(f"Exported: {out}  ({out.stat().st_size / 1024:.1f} KB)")
    print()
    print("Shell mounting (stock 4-hole pattern, no shell changes):")
    for i, (x, y) in enumerate(params.shell_hole_positions):
        print(f"  shell hole {i+1}: ({x:.1f}, {y:.1f})  -- M2 clearance {params.shell_hole_dia}mm")
    print()
    print("Innofader mounting (2-hole inline, M3 standoffs inside cavity):")
    for i, (x, y) in enumerate(params.innofader_hole_positions):
        print(f"  fader hole {i+1}: ({x:.1f}, {y:.1f})  -- M3 clearance {params.innofader_hole_dia}mm")
    print()
    print("POST-PRINT: widen the shell fader slot from 3.2mm to ~8mm with a file.")
    print("Remember: measure your fader and update MEASURE values before printing!")
