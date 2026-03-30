from __future__ import annotations

"""
ScratchTJ enclosure, rebuilt from zero as a modular CadQuery concept.

Key constraints from the current brief:
- brand new layout
- external footprint capped below 170 x 170 mm
- only the platter system, the screen module, and the fader appear on the outside
- Raspberry Pi 2, Audio Injector, and Arduino Nano stay fully inside the base
- enclosure support parts are printable as separate pieces and then assembled into the base

Printable parts in this draft:
- base_shell
- bottom_cover
- display_clamp
- button_bar_clamp
- fader_cassette

The platter interface assumes an existing printed carrier provided by the user.
This script models the enclosure-side mounting and a simple carrier reference.
"""

import argparse
import importlib.util
import math
import subprocess
import sys
import time
import types
from dataclasses import dataclass
from pathlib import Path


def _install_ivtk_stubs_if_needed() -> None:
    missing = set()
    for module_name in ("OCP.IVtkOCC", "OCP.IVtkVTK"):
        try:
            if importlib.util.find_spec(module_name) is None:
                missing.add(module_name)
        except ModuleNotFoundError:
            missing.add(module_name)

    if "OCP.IVtkOCC" in missing:
        mod = types.ModuleType("OCP.IVtkOCC")

        class IVtkOCC_Shape:
            def __init__(self, *args, **kwargs):
                pass

        class IVtkOCC_ShapeMesher:
            def __init__(self, *args, **kwargs):
                pass

            def Build(self, *args, **kwargs):
                pass

        mod.IVtkOCC_Shape = IVtkOCC_Shape
        mod.IVtkOCC_ShapeMesher = IVtkOCC_ShapeMesher
        sys.modules["OCP.IVtkOCC"] = mod

    if "OCP.IVtkVTK" in missing:
        mod = types.ModuleType("OCP.IVtkVTK")

        class IVtkVTK_ShapeData:
            pass

        mod.IVtkVTK_ShapeData = IVtkVTK_ShapeData
        sys.modules["OCP.IVtkVTK"] = mod


_install_ivtk_stubs_if_needed()

import cadquery as cq
from cadquery import exporters
from cadquery.occ_impl import shapes as cq_shapes


def _patch_shape_hash() -> None:
    def _safe_hash_code(self) -> int:
        try:
            return self.wrapped.HashCode(2147483647)
        except Exception:
            return hash(self.wrapped)

    cq_shapes.Shape.hashCode = _safe_hash_code
    cq_shapes.Shape.__hash__ = lambda self: _safe_hash_code(self)


_patch_shape_hash()


@dataclass(frozen=True)
class EnclosureParams:
    wall: float = 3.0
    top_th: float = 3.2
    shell_h: float = 58.0
    outer_x: float = 168.0
    outer_y: float = 168.0
    corner_r: float = 12.0

    bottom_cover_th: float = 3.0
    bottom_cover_gap: float = 0.45
    bottom_lip_w: float = 2.3
    bottom_boss_h: float = 10.0
    bottom_boss_od: float = 10.0
    bottom_mount_core_h: float = 4.0
    bottom_screw_tap_d: float = 2.8
    bottom_screw_clear_d: float = 3.4
    bottom_screw_head_d: float = 6.5
    bottom_screw_head_depth: float = 2.2
    bottom_screw_inset: float = 14.0

    platter_center_x: float = 112.0
    platter_center_y: float = 82.0
    platter_opening_d: float = 100.0
    platter_surround_d: float = 108.0
    platter_surround_h: float = 0.0

    carrier_total_h: float = 49.0
    carrier_flange_th: float = 4.0
    carrier_flange_outer_d: float = 106.0
    carrier_flange_inner_d: float = 96.0
    carrier_mount_radius: float = 47.0
    carrier_shell_boss_h: float = 12.0
    carrier_shell_boss_od: float = 4.0
    carrier_shell_tap_d: float = 2.4
    carrier_hole_d: float = 4.4
    carrier_cover_standoff_od: float = 7.0
    carrier_bottom_tap_d: float = 2.8
    carrier_bottom_thread_depth: float = 10.0
    carrier_bottom_hole_d: float = 3.4
    carrier_bottom_head_d: float = 6.2
    carrier_bottom_head_depth: float = 1.8
    carrier_bottom_access_d: float = 7.2
    carrier_mount_boss_od: float = 6.0
    carrier_mount_pitch: float = 46.0
    carrier_shell_mount_offset: float = 24.0
    carrier_shell_mount_tap_d: float = 2.8
    carrier_reference_x: float = 54.0
    carrier_reference_y: float = 54.0
    carrier_reference_th: float = 6.0
    carrier_reference_corner_r: float = 8.0
    carrier_plate_x: float = 48.0
    carrier_plate_y: float = 48.0
    carrier_plate_th: float = 3.0
    carrier_plate_corner_r: float = 5.0
    carrier_post_offset: float = 21.0
    carrier_post_od: float = 6.0
    param_mount_hole_pitch: float = 28.6
    param_mount_hole_d: float = 3.2
    param_mount_center_hole_d: float = 18.0
    param_mount_stack_to_platter_top: float = 54.0

    display_center_x: float = 33.5
    display_center_y: float = 44.0
    display_board_w: float = 67.0
    display_board_h: float = 55.0
    display_rotated: bool = True
    display_board_th: float = 1.6
    display_hole_inset: float = 3.5
    display_pocket_clear: float = 0.5
    display_pocket_depth: float = 0.0
    display_open_margin: float = 3.6
    display_corner_r: float = 4.0
    display_board_screw_clear_d: float = 2.6
    display_clamp_th: float = 3.0
    display_clamp_border: float = 4.5
    display_clamp_hole_d: float = 2.2
    display_clamp_pad_d: float = 7.0

    button_bar_rotated: bool = True
    button_bar_center_x: float = 19.0
    button_bar_center_y: float = 121.5
    button_bar_board_x: float = 77.5
    button_bar_board_y: float = 17.9
    button_bar_board_th: float = 1.6
    button_bar_total_h: float = 6.5
    button_bar_pocket_clear: float = 0.4
    button_bar_pocket_depth: float = 0.0
    button_bar_open_x: float = 72.0
    button_bar_open_y: float = 12.0
    button_bar_hole_pitch_x: float = 74.0
    button_bar_shell_boss_h: float = 7.0
    button_bar_shell_boss_od: float = 6.0
    button_bar_shell_tap_d: float = 2.2
    button_bar_clamp_th: float = 3.0
    button_bar_clamp_border_x: float = 4.0
    button_bar_clamp_border_y: float = 0.6
    button_bar_clamp_hole_d: float = 2.3

    fader_center_x: float = 114.0
    fader_center_y: float = 18.0
    fader_slot_x: float = 66.0
    fader_slot_y: float = 3.2
    fader_slot_r: float = 1.6
    fader_boss_offset_x: float = 31.0
    fader_boss_offset_y: float = 9.0
    fader_shell_boss_h: float = 9.0
    fader_shell_boss_od: float = 4.0
    fader_shell_tap_d: float = 2.4
    fader_cassette_h: float = 17.0
    fader_flange_th: float = 3.0
    fader_flange_x: float = 78.0
    fader_flange_y: float = 24.0
    fader_flange_hole_d: float = 4.4
    fader_tray_x: float = 68.0
    fader_tray_y: float = 16.0
    fader_floor_th: float = 2.0
    fader_inner_x: float = 64.0
    fader_inner_y: float = 10.5
    fader_cable_slot_x: float = 10.0
    fader_cable_slot_h: float = 7.0
    fader_cable_slot_z: float = 4.0

    pi_center_x: float = 114.0
    pi_center_y: float = 134.2
    pi_board_x: float = 85.0
    pi_board_y: float = 56.0
    pi_hole_pitch_x: float = 58.0
    pi_hole_pitch_y: float = 49.0
    pi_standoff_h: float = 8.0
    pi_standoff_od: float = 5.5
    pi_standoff_hole_d: float = 2.8
    pi_stack_h: float = 29.0
    pi_dupont_keepout_x: float = 28.0
    pi_dupont_keepout_y: float = 20.0
    pi_dupont_keepout_h: float = 14.0
    pi_dupont_keepout_dx: float = 22.0
    pi_dupont_keepout_dy: float = 16.0
    pi_side_access_x: float = 90.0
    pi_side_access_y: float = 32.0
    pi_right_access_depth: float = 38.0
    pi_right_access_span_y: float = 46.0
    pi_side_access_z: float = 3.0
    pi_side_access_h: float = 36.0
    side_opening_relief: float = 3.4
    bottom_mount_support_w: float = 14.0

    nano_center_x: float = 39.0
    nano_center_y: float = 26.0
    nano_board_x: float = 45.0
    nano_board_y: float = 18.0
    nano_rail_t: float = 2.0
    nano_rail_h: float = 5.0
    nano_clear_y: float = 0.6
    nano_stop_t: float = 2.0
    nano_th: float = 8.0
    nano_dupont_keepout_x: float = 49.0
    nano_dupont_keepout_y: float = 24.0
    nano_dupont_keepout_h: float = 12.0

    display_clamp_drop: float = 4.0
    button_bar_clamp_drop: float = 4.0

    @property
    def cx(self) -> float:
        return self.outer_x / 2.0

    @property
    def cy(self) -> float:
        return self.outer_y / 2.0

    @property
    def inner_x(self) -> float:
        return self.outer_x - 2.0 * self.wall

    @property
    def inner_y(self) -> float:
        return self.outer_y - 2.0 * self.wall

    @property
    def inner_corner_r(self) -> float:
        return max(1.0, self.corner_r - self.wall)

    @property
    def bottom_open_x(self) -> float:
        return self.inner_x - 2.0 * self.bottom_lip_w

    @property
    def bottom_open_y(self) -> float:
        return self.inner_y - 2.0 * self.bottom_lip_w

    @property
    def bottom_open_corner_r(self) -> float:
        return max(1.0, self.inner_corner_r - self.bottom_lip_w)

    @property
    def bottom_cover_x(self) -> float:
        return self.bottom_open_x - 2.0 * self.bottom_cover_gap

    @property
    def bottom_cover_y(self) -> float:
        return self.bottom_open_y - 2.0 * self.bottom_cover_gap

    @property
    def bottom_cover_corner_r(self) -> float:
        return max(0.8, self.bottom_open_corner_r - self.bottom_cover_gap)

    @property
    def top_underside_z(self) -> float:
        return self.shell_h - self.top_th

    @property
    def platter_carrier_z(self) -> float:
        return self.bottom_cover_th

    @property
    def carrier_cover_support_h(self) -> float:
        return max(1.5, self.platter_carrier_z - self.bottom_cover_th)

    @property
    def display_board_x(self) -> float:
        return self.display_board_h if self.display_rotated else self.display_board_w

    @property
    def display_board_y(self) -> float:
        return self.display_board_w if self.display_rotated else self.display_board_h

    @property
    def display_pocket_x(self) -> float:
        return self.display_board_x + 2.0 * self.display_pocket_clear

    @property
    def display_pocket_y(self) -> float:
        return self.display_board_y + 2.0 * self.display_pocket_clear

    @property
    def display_open_x(self) -> float:
        return self.display_board_x - 2.0 * self.display_open_margin

    @property
    def display_open_y(self) -> float:
        return self.display_board_y - 2.0 * self.display_open_margin

    @property
    def display_board_z0(self) -> float:
        if self.display_pocket_depth > 0.0:
            return self.shell_h - self.display_pocket_depth
        return self.top_underside_z - self.display_board_th

    @property
    def display_clamp_x(self) -> float:
        return self.display_board_x + 2.0 * self.display_clamp_border

    @property
    def display_clamp_y(self) -> float:
        return self.display_board_y + 2.0 * self.display_clamp_border

    @property
    def display_hole_pitch_x(self) -> float:
        return self.display_board_x - 2.0 * self.display_hole_inset

    @property
    def display_hole_pitch_y(self) -> float:
        return self.display_board_y - 2.0 * self.display_hole_inset

    @property
    def display_hole_positions(self) -> list[tuple[float, float]]:
        dx = self.display_hole_pitch_x / 2.0
        dy = self.display_hole_pitch_y / 2.0
        return [
            (self.display_center_x - dx, self.display_center_y - dy),
            (self.display_center_x + dx, self.display_center_y - dy),
            (self.display_center_x - dx, self.display_center_y + dy),
            (self.display_center_x + dx, self.display_center_y + dy),
        ]

    @property
    def button_bar_pocket_x(self) -> float:
        return self.button_bar_board_span_x + 2.0 * self.button_bar_pocket_clear

    @property
    def button_bar_pocket_y(self) -> float:
        return self.button_bar_board_span_y + 2.0 * self.button_bar_pocket_clear

    @property
    def button_bar_board_span_x(self) -> float:
        return self.button_bar_board_y if self.button_bar_rotated else self.button_bar_board_x

    @property
    def button_bar_board_span_y(self) -> float:
        return self.button_bar_board_x if self.button_bar_rotated else self.button_bar_board_y

    @property
    def button_bar_open_span_x(self) -> float:
        return self.button_bar_open_y if self.button_bar_rotated else self.button_bar_open_x

    @property
    def button_bar_open_span_y(self) -> float:
        return self.button_bar_open_x if self.button_bar_rotated else self.button_bar_open_y

    @property
    def button_bar_clamp_x(self) -> float:
        return self.button_bar_board_span_x + 2.0 * self.button_bar_clamp_border_x

    @property
    def button_bar_clamp_y(self) -> float:
        return self.button_bar_board_span_y + 2.0 * self.button_bar_clamp_border_y

    @property
    def button_bar_board_z0(self) -> float:
        if self.button_bar_pocket_depth > 0.0:
            return self.shell_h - self.button_bar_pocket_depth
        return self.top_underside_z - self.button_bar_total_h

    @property
    def button_bar_hole_positions(self) -> list[tuple[float, float]]:
        dx = self.button_bar_hole_pitch_x / 2.0
        if self.button_bar_rotated:
            return [
                (self.button_bar_center_x, self.button_bar_center_y - dx),
                (self.button_bar_center_x, self.button_bar_center_y + dx),
            ]
        return [
            (self.button_bar_center_x - dx, self.button_bar_center_y),
            (self.button_bar_center_x + dx, self.button_bar_center_y),
        ]

    @property
    def bottom_screw_positions(self) -> list[tuple[float, float]]:
        return [
            (self.bottom_screw_inset, self.bottom_screw_inset),
            (self.outer_x - self.bottom_screw_inset, self.bottom_screw_inset),
            (self.bottom_screw_inset, self.outer_y - self.bottom_screw_inset),
            (self.outer_x - self.bottom_screw_inset, self.outer_y - self.bottom_screw_inset),
            (self.cx, self.bottom_screw_inset),
            (self.cx, self.outer_y - self.bottom_screw_inset),
        ]

    @property
    def carrier_shell_positions(self) -> list[tuple[float, float]]:
        positions: list[tuple[float, float]] = []
        for angle_deg in (0, 60, 120, 180, 240, 300):
            angle = angle_deg * 3.141592653589793 / 180.0
            positions.append(
                (
                    self.platter_center_x + self.carrier_mount_radius * math.cos(angle),
                    self.platter_center_y + self.carrier_mount_radius * math.sin(angle),
                )
            )
        return positions

    @property
    def fader_shell_positions(self) -> list[tuple[float, float]]:
        return [
            (self.fader_center_x - self.fader_boss_offset_x, self.fader_center_y - self.fader_boss_offset_y),
            (self.fader_center_x + self.fader_boss_offset_x, self.fader_center_y - self.fader_boss_offset_y),
            (self.fader_center_x - self.fader_boss_offset_x, self.fader_center_y + self.fader_boss_offset_y),
            (self.fader_center_x + self.fader_boss_offset_x, self.fader_center_y + self.fader_boss_offset_y),
        ]

    @property
    def carrier_param_mount_positions(self) -> list[tuple[float, float]]:
        d = self.param_mount_hole_pitch / 2.0
        return [
            (self.platter_center_x - d, self.platter_center_y - d),
            (self.platter_center_x + d, self.platter_center_y - d),
            (self.platter_center_x - d, self.platter_center_y + d),
            (self.platter_center_x + d, self.platter_center_y + d),
        ]

    @property
    def carrier_existing_mount_positions(self) -> list[tuple[float, float]]:
        d = self.carrier_mount_pitch / 2.0
        return [
            (self.platter_center_x - d, self.platter_center_y - d),
            (self.platter_center_x + d, self.platter_center_y - d),
            (self.platter_center_x - d, self.platter_center_y + d),
            (self.platter_center_x + d, self.platter_center_y + d),
        ]

    @property
    def carrier_shell_mount_positions(self) -> list[tuple[float, float]]:
        d = self.carrier_shell_mount_offset
        return [
            (self.platter_center_x - d, self.platter_center_y),
            (self.platter_center_x + d, self.platter_center_y),
            (self.platter_center_x, self.platter_center_y - d),
            (self.platter_center_x, self.platter_center_y + d),
        ]

    @property
    def pi_hole_positions(self) -> list[tuple[float, float]]:
        dx = self.pi_hole_pitch_x / 2.0
        dy = self.pi_hole_pitch_y / 2.0
        return [
            (self.pi_center_x - dx, self.pi_center_y - dy),
            (self.pi_center_x + dx, self.pi_center_y - dy),
            (self.pi_center_x - dx, self.pi_center_y + dy),
            (self.pi_center_x + dx, self.pi_center_y + dy),
        ]


def fuse_all(shapes: list[cq.Workplane]) -> cq.Workplane:
    if not shapes:
        raise ValueError("fuse_all() requires at least one shape")
    result = shapes[0]
    for shape in shapes[1:]:
        result = result.union(shape)
    return result


def cut_all(base: cq.Workplane, cuts: list[cq.Workplane]) -> cq.Workplane:
    result = base
    for cutter in cuts:
        result = result.cut(cutter)
    return result


def box_from_z(xc: float, yc: float, z0: float, sx: float, sy: float, sz: float) -> cq.Workplane:
    return cq.Workplane("XY", origin=(xc, yc, z0)).box(sx, sy, sz, centered=(True, True, False))


def cylinder_from_z(xc: float, yc: float, z0: float, height: float, diameter: float) -> cq.Workplane:
    return cq.Workplane("XY", origin=(xc, yc, z0)).circle(diameter / 2.0).extrude(height)


def rounded_rect_prism(xc: float, yc: float, z0: float, sx: float, sy: float, radius: float, height: float) -> cq.Workplane:
    r = max(0.0, min(radius, sx / 2.0, sy / 2.0))
    if r <= 0.0:
        return box_from_z(xc, yc, z0, sx, sy, height)

    pieces: list[cq.Workplane] = []
    if sx - 2.0 * r > 0.0:
        pieces.append(box_from_z(xc, yc, z0, sx - 2.0 * r, sy, height))
    if sy - 2.0 * r > 0.0:
        pieces.append(box_from_z(xc, yc, z0, sx, sy - 2.0 * r, height))

    corner_dx = sx / 2.0 - r
    corner_dy = sy / 2.0 - r
    for dx in (-corner_dx, corner_dx):
        for dy in (-corner_dy, corner_dy):
            pieces.append(cylinder_from_z(xc + dx, yc + dy, z0, height, 2.0 * r))

    return fuse_all(pieces)


def ring_from_z(xc: float, yc: float, z0: float, height: float, outer_d: float, inner_d: float) -> cq.Workplane:
    outer = cylinder_from_z(xc, yc, z0, height, outer_d)
    inner = cylinder_from_z(xc, yc, z0 - 0.1, height + 0.2, inner_d)
    return outer.cut(inner)


def support_free_side_cut_x(
    x_outer: float,
    yc: float,
    z0: float,
    depth: float,
    span_y: float,
    height: float,
    relief: float,
) -> cq.Workplane:
    base = box_from_z(x_outer - depth / 2.0, yc, z0, depth, span_y, height)
    profile = (
        cq.Workplane("XZ")
        .polyline(
            [
                (x_outer - relief, z0),
                (x_outer, z0 - relief),
                (x_outer, z0),
            ]
        )
        .close()
        .extrude(span_y)
    )
    bevel = profile.translate((0.0, yc - span_y / 2.0, 0.0))
    return base.union(bevel)


def support_free_side_cut_y(
    xc: float,
    y_outer: float,
    z0: float,
    span_x: float,
    depth: float,
    height: float,
    relief: float,
) -> cq.Workplane:
    base = box_from_z(xc, y_outer - depth / 2.0, z0, span_x, depth, height)
    profile = (
        cq.Workplane("YZ")
        .polyline(
            [
                (y_outer - relief, z0),
                (y_outer, z0 - relief),
                (y_outer, z0),
            ]
        )
        .close()
        .extrude(span_x)
    )
    bevel = profile.translate((xc - span_x / 2.0, 0.0, 0.0))
    return base.union(bevel)


def wall_supported_mount(
    p: EnclosureParams,
    xc: float,
    yc: float,
    z0: float,
    height: float,
    diameter: float,
) -> cq.Workplane:
    pieces: list[cq.Workplane] = [cylinder_from_z(xc, yc, z0, height, p.bottom_boss_od)]
    z_top = z0 + height
    boss_r = p.bottom_boss_od / 2.0
    half_w = diameter / 2.0
    wall_inner_x0 = p.wall
    wall_inner_x1 = p.outer_x - p.wall
    wall_inner_y0 = p.wall
    wall_inner_y1 = p.outer_y - p.wall

    if xc <= p.bottom_screw_inset + 0.1:
        run = max(0.0, xc - boss_r - wall_inner_x0)
        wall_z = min(p.top_underside_z, z_top + run)
        if run > 0.0:
            profile = (
                cq.Workplane("XZ")
                .polyline(
                    [
                        (wall_inner_x0, wall_z),
                        (xc - boss_r, z_top),
                        (wall_inner_x0, z_top),
                    ]
                )
                .close()
                .extrude(half_w)
            )
            pieces.append(profile.translate((0.0, yc, 0.0)))
    elif xc >= p.outer_x - p.bottom_screw_inset - 0.1:
        run = max(0.0, wall_inner_x1 - (xc + boss_r))
        wall_z = min(p.top_underside_z, z_top + run)
        if run > 0.0:
            profile = (
                cq.Workplane("XZ")
                .polyline(
                    [
                        (wall_inner_x1, wall_z),
                        (xc + boss_r, z_top),
                        (wall_inner_x1, z_top),
                    ]
                )
                .close()
                .extrude(half_w)
            )
            pieces.append(profile.translate((0.0, yc, 0.0)))

    if yc <= p.bottom_screw_inset + 0.1:
        run = max(0.0, yc - boss_r - wall_inner_y0)
        wall_z = min(p.top_underside_z, z_top + run)
        if run > 0.0:
            profile = (
                cq.Workplane("YZ")
                .polyline(
                    [
                        (wall_inner_y0, wall_z),
                        (yc - boss_r, z_top),
                        (wall_inner_y0, z_top),
                    ]
                )
                .close()
                .extrude(half_w)
            )
            pieces.append(profile.translate((xc, 0.0, 0.0)))
    elif yc >= p.outer_y - p.bottom_screw_inset - 0.1:
        run = max(0.0, wall_inner_y1 - (yc + boss_r))
        wall_z = min(p.top_underside_z, z_top + run)
        if run > 0.0:
            profile = (
                cq.Workplane("YZ")
                .polyline(
                    [
                        (wall_inner_y1, wall_z),
                        (yc + boss_r, z_top),
                        (wall_inner_y1, z_top),
                    ]
                )
                .close()
                .extrude(half_w)
            )
            pieces.append(profile.translate((xc, 0.0, 0.0)))

    return fuse_all(pieces)


def build_base_shell(p: EnclosureParams) -> cq.Workplane:
    outer = rounded_rect_prism(p.cx, p.cy, 0.0, p.outer_x, p.outer_y, p.corner_r, p.shell_h)
    main_cavity = rounded_rect_prism(
        p.cx,
        p.cy,
        p.bottom_cover_th,
        p.inner_x,
        p.inner_y,
        p.inner_corner_r,
        p.shell_h - p.top_th - p.bottom_cover_th + 0.2,
    )
    bottom_open = rounded_rect_prism(
        p.cx,
        p.cy,
        -0.1,
        p.bottom_open_x,
        p.bottom_open_y,
        p.bottom_open_corner_r,
        p.bottom_cover_th + 0.2,
    )
    shell = outer.cut(main_cavity).cut(bottom_open)

    solids: list[cq.Workplane] = [shell]

    for x, y in p.bottom_screw_positions:
        solids.append(
            wall_supported_mount(
                p,
                x,
                y,
                p.bottom_cover_th,
                p.bottom_boss_h,
                p.bottom_mount_support_w,
            )
        )

    fader_boss_z0 = p.top_underside_z - p.fader_shell_boss_h
    for x, y in p.fader_shell_positions:
        solids.append(cylinder_from_z(x, y, fader_boss_z0, p.fader_shell_boss_h, p.fader_shell_boss_od))

    shell = fuse_all(solids)

    cuts: list[cq.Workplane] = []

    for x, y in p.bottom_screw_positions:
        cuts.append(cylinder_from_z(x, y, -0.2, p.bottom_cover_th + p.bottom_boss_h + 0.4, p.bottom_screw_tap_d))

    for x, y in p.fader_shell_positions:
        cuts.append(
            cylinder_from_z(
                x,
                y,
                fader_boss_z0 - 0.1,
                p.fader_shell_boss_h + 0.2,
                p.fader_shell_tap_d,
            )
        )

    for x, y in p.button_bar_hole_positions:
        cuts.append(
            cylinder_from_z(
                x,
                y,
                -0.2,
                p.shell_h + 0.4,
                p.button_bar_clamp_hole_d,
            )
        )

    if p.display_pocket_depth > 0.0:
        cuts.append(
            rounded_rect_prism(
                p.display_center_x,
                p.display_center_y,
                p.shell_h - p.display_pocket_depth,
                p.display_pocket_x,
                p.display_pocket_y,
                p.display_corner_r,
                p.display_pocket_depth + 0.3,
            )
        )
    cuts.append(
        rounded_rect_prism(
            p.display_center_x,
            p.display_center_y,
            -0.2,
            p.display_open_x,
            p.display_open_y,
            max(1.0, p.display_corner_r - 1.8),
            p.shell_h + 0.4,
        )
    )

    if p.button_bar_pocket_depth > 0.0:
        cuts.append(
            rounded_rect_prism(
                p.button_bar_center_x,
                p.button_bar_center_y,
                p.shell_h - p.button_bar_pocket_depth,
                p.button_bar_pocket_x,
                p.button_bar_pocket_y,
                2.2,
                p.button_bar_pocket_depth + 0.3,
            )
        )
    cuts.append(
        rounded_rect_prism(
            p.button_bar_center_x,
            p.button_bar_center_y,
            -0.2,
            p.button_bar_open_span_x,
            p.button_bar_open_span_y,
            1.8,
            p.shell_h + 0.4,
        )
    )

    for x, y in p.display_hole_positions:
        cuts.append(cylinder_from_z(x, y, -0.2, p.shell_h + 0.4, p.display_board_screw_clear_d))

    cuts.append(cylinder_from_z(p.platter_center_x, p.platter_center_y, -0.2, p.shell_h + p.platter_surround_h + 0.4, p.platter_opening_d))
    cuts.append(
        rounded_rect_prism(
            p.fader_center_x,
            p.fader_center_y,
            -0.2,
            p.fader_slot_x,
            p.fader_slot_y,
            p.fader_slot_r,
            p.shell_h + 0.4,
        )
    )
    cuts.append(
        box_from_z(
            p.outer_x - p.pi_right_access_depth / 2.0,
            p.pi_center_y,
            p.pi_side_access_z,
            p.pi_right_access_depth,
            p.pi_right_access_span_y,
            p.pi_side_access_h,
        )
    )
    cuts.append(
        box_from_z(
            p.pi_center_x,
            p.outer_y - p.pi_side_access_y / 2.0,
            p.pi_side_access_z,
            p.pi_side_access_x,
            p.pi_side_access_y,
            p.pi_side_access_h,
        )
    )

    result = cut_all(shell, cuts)
    if p.platter_surround_h > 0.0:
        surround = ring_from_z(
            p.platter_center_x,
            p.platter_center_y,
            p.shell_h,
            p.platter_surround_h,
            p.platter_surround_d,
            p.platter_opening_d,
        )
        result = result.union(surround)
    return result


def build_bottom_cover(p: EnclosureParams) -> cq.Workplane:
    cover = rounded_rect_prism(
        p.cx,
        p.cy,
        0.0,
        p.bottom_cover_x,
        p.bottom_cover_y,
        p.bottom_cover_corner_r,
        p.bottom_cover_th,
    )

    solids: list[cq.Workplane] = [cover]

    for x, y in p.pi_hole_positions:
        solids.append(cylinder_from_z(x, y, p.bottom_cover_th, p.pi_standoff_h, p.pi_standoff_od))

    rail_y = p.nano_board_y / 2.0 + p.nano_clear_y + p.nano_rail_t / 2.0
    solids.append(
        box_from_z(
            p.nano_center_x,
            p.nano_center_y - rail_y,
            p.bottom_cover_th,
            p.nano_board_x,
            p.nano_rail_t,
            p.nano_rail_h,
        )
    )
    solids.append(
        box_from_z(
            p.nano_center_x,
            p.nano_center_y + rail_y,
            p.bottom_cover_th,
            p.nano_board_x,
            p.nano_rail_t,
            p.nano_rail_h,
        )
    )
    solids.append(
        box_from_z(
            p.nano_center_x + p.nano_board_x / 2.0 + p.nano_stop_t / 2.0,
            p.nano_center_y,
            p.bottom_cover_th,
            p.nano_stop_t,
            p.nano_board_y + 2.0 * rail_y,
            p.nano_rail_h,
        )
    )

    cover = fuse_all(solids)

    cuts: list[cq.Workplane] = []

    for x, y in p.bottom_screw_positions:
        cuts.append(cylinder_from_z(x, y, -0.2, p.bottom_cover_th + 0.4, p.bottom_screw_clear_d))
        cuts.append(
            cylinder_from_z(
                x,
                y,
                0.0,
                p.bottom_screw_head_depth + 0.1,
                p.bottom_screw_head_d,
            )
        )

    for x, y in p.carrier_existing_mount_positions:
        cuts.append(
            cylinder_from_z(
                x,
                y,
                -0.2,
                p.bottom_cover_th + 0.4,
                p.carrier_bottom_hole_d,
            )
        )
        cuts.append(
            cylinder_from_z(
                x,
                y,
                0.0,
                p.carrier_bottom_head_depth + 0.1,
                p.carrier_bottom_access_d,
            )
        )

    for x, y in p.pi_hole_positions:
        cuts.append(cylinder_from_z(x, y, p.bottom_cover_th - 0.1, p.pi_standoff_h + 0.2, p.pi_standoff_hole_d))

    return cut_all(cover, cuts)


def build_display_clamp(p: EnclosureParams) -> cq.Workplane:
    board_left = p.display_center_x - p.display_board_x / 2.0
    board_right = p.display_center_x + p.display_board_x / 2.0
    board_bottom = p.display_center_y - p.display_board_y / 2.0
    board_top = p.display_center_y + p.display_board_y / 2.0

    left_hole_x = p.display_center_x - p.display_hole_pitch_x / 2.0
    right_hole_x = p.display_center_x + p.display_hole_pitch_x / 2.0
    clamp_left = left_hole_x - p.display_clamp_pad_d / 2.0
    clamp_right = board_right + p.display_clamp_border
    clamp_bottom = board_bottom - p.display_clamp_border
    clamp_top = board_top + p.display_clamp_border

    bars = [
        rounded_rect_prism(
            (clamp_left + clamp_right) / 2.0,
            board_top + p.display_clamp_border / 2.0,
            0.0,
            clamp_right - clamp_left,
            p.display_clamp_border,
            1.6,
            p.display_clamp_th,
        ),
        rounded_rect_prism(
            (clamp_left + clamp_right) / 2.0,
            board_bottom - p.display_clamp_border / 2.0,
            0.0,
            clamp_right - clamp_left,
            p.display_clamp_border,
            1.6,
            p.display_clamp_th,
        ),
        rounded_rect_prism(
            board_right + p.display_clamp_border / 2.0,
            p.display_center_y,
            0.0,
            p.display_clamp_border,
            clamp_top - clamp_bottom,
            1.6,
            p.display_clamp_th,
        ),
        rounded_rect_prism(
            left_hole_x,
            p.display_center_y,
            0.0,
            p.display_clamp_pad_d,
            p.display_hole_pitch_y + p.display_clamp_pad_d + 0.8,
            1.6,
            p.display_clamp_th,
        ),
    ]
    pads = [
        cylinder_from_z(x, y, 0.0, p.display_clamp_th, p.display_clamp_pad_d)
        for x, y in p.display_hole_positions
    ]
    frame = fuse_all(bars + pads)
    window = rounded_rect_prism(
        p.display_center_x,
        p.display_center_y,
        -0.1,
        max(20.0, p.display_open_x - 6.0),
        max(24.0, p.display_open_y - 6.0),
        max(1.0, p.display_corner_r - 2.0),
        p.display_clamp_th + 0.2,
    )

    cuts = [window]
    for x, y in p.display_hole_positions:
        cuts.append(cylinder_from_z(x, y, -0.1, p.display_clamp_th + 0.2, p.display_clamp_hole_d))

    return cut_all(frame, cuts)


def build_button_bar_clamp(p: EnclosureParams) -> cq.Workplane:
    frame = rounded_rect_prism(
        p.button_bar_center_x,
        p.button_bar_center_y,
        0.0,
        p.button_bar_clamp_x,
        p.button_bar_clamp_y,
        2.0,
        p.button_bar_clamp_th,
    )
    window = rounded_rect_prism(
        p.button_bar_center_x,
        p.button_bar_center_y,
        -0.1,
        max(p.button_bar_open_span_x, p.button_bar_board_span_x - 10.0),
        max(p.button_bar_open_span_y + 3.5, p.button_bar_board_span_y - 2.5),
        1.4,
        p.button_bar_clamp_th + 0.2,
    )

    cuts = [window]
    for x, y in p.button_bar_hole_positions:
        cuts.append(cylinder_from_z(x, y, -0.1, p.button_bar_clamp_th + 0.2, p.button_bar_clamp_hole_d))

    return cut_all(frame, cuts)


def build_fader_cassette(p: EnclosureParams) -> cq.Workplane:
    body_h = p.fader_cassette_h - p.fader_flange_th
    flange = rounded_rect_prism(
        p.fader_center_x,
        p.fader_center_y,
        body_h,
        p.fader_flange_x,
        p.fader_flange_y,
        4.0,
        p.fader_flange_th,
    )
    tray = rounded_rect_prism(
        p.fader_center_x,
        p.fader_center_y,
        0.0,
        p.fader_tray_x,
        p.fader_tray_y,
        3.0,
        body_h,
    )
    cassette = flange.union(tray)

    cuts: list[cq.Workplane] = []
    cuts.append(
        rounded_rect_prism(
            p.fader_center_x,
            p.fader_center_y,
            p.fader_floor_th,
            p.fader_inner_x,
            p.fader_inner_y,
            2.0,
            p.fader_cassette_h + 0.2,
        )
    )
    cuts.append(
        rounded_rect_prism(
            p.fader_center_x,
            p.fader_center_y,
            body_h - 0.1,
            p.fader_slot_x,
            8.0,
            2.0,
            p.fader_flange_th + 0.2,
        )
    )
    cuts.append(
        box_from_z(
            p.fader_center_x,
            p.fader_center_y + p.fader_tray_y / 2.0 - 1.2,
            p.fader_cable_slot_z,
            p.fader_cable_slot_x,
            4.0,
            p.fader_cable_slot_h,
        )
    )

    for x, y in p.fader_shell_positions:
        cuts.append(cylinder_from_z(x, y, -0.1, p.fader_cassette_h + 0.2, p.fader_flange_hole_d))

    return cut_all(cassette, cuts)


def build_platter_carrier(p: EnclosureParams) -> cq.Workplane:
    carrier = rounded_rect_prism(
        p.platter_center_x,
        p.platter_center_y,
        0.0,
        p.carrier_reference_x,
        p.carrier_reference_y,
        p.carrier_reference_corner_r,
        p.carrier_reference_th,
    )

    cuts: list[cq.Workplane] = []
    for x, y in p.carrier_existing_mount_positions:
        cuts.append(cylinder_from_z(x, y, -0.1, p.carrier_reference_th + 0.2, p.carrier_bottom_tap_d))

    for x, y in p.carrier_shell_mount_positions:
        cuts.append(cylinder_from_z(x, y, -0.1, p.carrier_reference_th + 0.2, p.carrier_shell_mount_tap_d))

    cuts.append(
        cylinder_from_z(
            p.platter_center_x,
            p.platter_center_y,
            -0.1,
            p.carrier_reference_th + 0.2,
            p.param_mount_center_hole_d,
        )
    )
    for x, y in p.carrier_param_mount_positions:
        cuts.append(cylinder_from_z(x, y, -0.1, p.carrier_reference_th + 0.2, p.param_mount_hole_d))

    return cut_all(carrier, cuts)


def build_preview_models(p: EnclosureParams) -> dict[str, cq.Workplane]:
    previews: dict[str, cq.Workplane] = {}

    previews["preview_display_board"] = rounded_rect_prism(
        p.display_center_x,
        p.display_center_y,
        p.display_board_z0,
        p.display_board_x,
        p.display_board_y,
        p.display_corner_r,
        p.display_board_th,
    )
    previews["preview_button_bar"] = rounded_rect_prism(
        p.button_bar_center_x,
        p.button_bar_center_y,
        p.button_bar_board_z0,
        p.button_bar_board_span_x,
        p.button_bar_board_span_y,
        1.2,
        p.button_bar_total_h,
    )

    fader_knob = box_from_z(
        p.fader_center_x,
        p.fader_center_y,
        p.shell_h,
        10.0,
        4.0,
        9.0,
    )
    previews["preview_fader_knob"] = fader_knob

    platter_top_z = p.platter_carrier_z + p.carrier_reference_th + p.param_mount_stack_to_platter_top
    previews["preview_platter"] = cylinder_from_z(
        p.platter_center_x,
        p.platter_center_y,
        platter_top_z,
        1.5,
        95.0,
    )
    previews["preview_param_mount_envelope"] = rounded_rect_prism(
        p.platter_center_x,
        p.platter_center_y,
        p.platter_carrier_z + p.carrier_reference_th,
        35.6,
        35.6,
        3.0,
        46.4,
    )

    previews["preview_pi_stack"] = rounded_rect_prism(
        p.pi_center_x,
        p.pi_center_y,
        p.bottom_cover_th + p.pi_standoff_h,
        p.pi_board_x,
        p.pi_board_y,
        4.0,
        p.pi_stack_h,
    )
    previews["preview_pi_dupont_keepout"] = box_from_z(
        p.pi_center_x + p.pi_dupont_keepout_dx,
        p.pi_center_y + p.pi_dupont_keepout_dy,
        p.bottom_cover_th + p.pi_standoff_h + p.pi_stack_h,
        p.pi_dupont_keepout_x,
        p.pi_dupont_keepout_y,
        p.pi_dupont_keepout_h,
    )
    previews["preview_nano"] = box_from_z(
        p.nano_center_x,
        p.nano_center_y,
        p.bottom_cover_th + p.nano_rail_h,
        p.nano_board_x,
        p.nano_board_y,
        p.nano_th,
    )
    previews["preview_nano_dupont_keepout"] = box_from_z(
        p.nano_center_x,
        p.nano_center_y,
        p.bottom_cover_th + p.nano_rail_h + p.nano_th,
        p.nano_dupont_keepout_x,
        p.nano_dupont_keepout_y,
        p.nano_dupont_keepout_h,
    )

    return previews


def build_printable_models(p: EnclosureParams | None = None) -> dict[str, cq.Workplane]:
    params = p or EnclosureParams()
    return {
        "base_shell": build_base_shell(params),
        "bottom_cover": build_bottom_cover(params),
        "platter_carrier": build_platter_carrier(params),
        "display_clamp": build_display_clamp(params),
        "button_bar_clamp": build_button_bar_clamp(params),
        "fader_cassette": build_fader_cassette(params),
    }


def build_models(p: EnclosureParams | None = None) -> dict[str, cq.Workplane]:
    params = p or EnclosureParams()
    models = build_printable_models(params)
    models.update(build_preview_models(params))
    return models


def export_models(models: dict[str, cq.Workplane], outdir: Path, fmt: str) -> None:
    outdir.mkdir(parents=True, exist_ok=True)
    for name, model in models.items():
        if fmt in {"step", "both"}:
            exporters.export(model, str(outdir / f"{name}.step"))
        if fmt in {"stl", "both"}:
            exporters.export(model, str(outdir / f"{name}.stl"))


def models_for_viewer(models: dict[str, cq.Workplane], p: EnclosureParams) -> dict[str, cq.Workplane]:
    adjusted: dict[str, cq.Workplane] = {}
    for name, model in models.items():
        if name == "display_clamp":
            adjusted[name] = model.translate((0.0, 0.0, p.top_underside_z - p.display_clamp_th - p.display_clamp_drop))
        elif name == "button_bar_clamp":
            if p.button_bar_pocket_depth > 0.0:
                clamp_z0 = p.top_underside_z - p.button_bar_clamp_th - p.button_bar_clamp_drop
            else:
                clamp_z0 = p.button_bar_board_z0 - p.button_bar_clamp_th
            adjusted[name] = model.translate((0.0, 0.0, clamp_z0))
        elif name == "fader_cassette":
            adjusted[name] = model.translate((0.0, 0.0, p.top_underside_z - p.fader_cassette_h))
        elif name == "platter_carrier":
            adjusted[name] = model.translate((0.0, 0.0, p.platter_carrier_z))
        else:
            adjusted[name] = model
    return adjusted


def show_models(models: dict[str, cq.Workplane], p: EnclosureParams, port: int | None = None) -> None:
    from ocp_vscode import show

    adjusted = models_for_viewer(models, p)
    names = list(adjusted.keys())
    show_kwargs = {
        "names": names,
        "axes": True,
        "grid": True,
    }
    if port is not None:
        show_kwargs["port"] = port

    show(*adjusted.values(), **show_kwargs)


def print_model_bounds(models: dict[str, cq.Workplane]) -> None:
    for name, model in models.items():
        bb = model.val().BoundingBox()
        print(f"{name}: x={bb.xlen:.2f} y={bb.ylen:.2f} z={bb.zlen:.2f}")


def run_once(args: argparse.Namespace) -> int:
    params = EnclosureParams()
    printable = build_printable_models(params)
    viewer_models = build_models(params)

    if args.part != "all":
        printable = {args.part: printable[args.part]}
        viewer_models = {args.part: viewer_models[args.part]}

    if args.format != "none":
        export_models(printable, Path(args.outdir), args.format)

    if args.viewer:
        if args.part == "all":
            show_models(viewer_models, params, port=args.viewer_port)
        else:
            show_models(viewer_models, params, port=args.viewer_port)

    print_model_bounds(printable)
    return 0


def _snapshot_mtimes(paths: list[Path]) -> dict[Path, int | None]:
    mtimes: dict[Path, int | None] = {}
    for path in paths:
        try:
            mtimes[path] = path.stat().st_mtime_ns
        except FileNotFoundError:
            mtimes[path] = None
    return mtimes


def watch_loop(raw_argv: list[str], poll_interval: float) -> int:
    script_path = Path(__file__).resolve()
    watch_paths = [script_path]
    child_argv: list[str] = []

    skip_next = False
    for arg in raw_argv:
        if skip_next:
            skip_next = False
            continue
        if arg == "--watch":
            continue
        if arg.startswith("--watch-poll="):
            continue
        if arg == "--watch-poll":
            skip_next = True
            continue
        child_argv.append(arg)

    snapshot = _snapshot_mtimes(watch_paths)
    print(f"watching {script_path} (poll={poll_interval:.2f}s)", flush=True)
    print("press Ctrl-C to stop", flush=True)

    while True:
        result = subprocess.run([sys.executable, str(script_path), *child_argv], check=False)
        try:
            while True:
                time.sleep(poll_interval)
                current = _snapshot_mtimes(watch_paths)
                if current != snapshot:
                    snapshot = current
                    print("\nchange detected, rerunning...\n", flush=True)
                    break
        except KeyboardInterrupt:
            return result.returncode


def main() -> None:
    parser = argparse.ArgumentParser(description="ScratchTJ modular turntable enclosure in CadQuery.")
    parser.add_argument(
        "--part",
        choices=["base_shell", "bottom_cover", "platter_carrier", "display_clamp", "button_bar_clamp", "fader_cassette", "all"],
        default="all",
    )
    parser.add_argument("--format", choices=["none", "step", "stl", "both"], default="none")
    parser.add_argument("--outdir", default=str(Path(__file__).with_name("cadquery_exports")))
    parser.add_argument("--viewer", action="store_true", help="Send the model to OCP CAD Viewer.")
    parser.add_argument("--viewer-port", type=int, default=None, help="Optional OCP CAD Viewer port.")
    parser.add_argument("--watch", action="store_true", help="Rerun the script when this file changes.")
    parser.add_argument("--watch-poll", type=float, default=0.75, help="Watch polling interval in seconds.")
    args = parser.parse_args()

    if args.watch:
        raise SystemExit(watch_loop(sys.argv[1:], args.watch_poll))

    raise SystemExit(run_once(args))


if __name__ == "__main__":
    main()
