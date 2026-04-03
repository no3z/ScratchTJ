# ScratchTJ v2.0 — Enclosure Design Document

## Overview

Complete redesign of the ScratchTJ DJ controller enclosure, replacing the single-piece Tinkercad box with a modular, parametric OpenSCAD design optimized for the MT6701 magnetic encoder upgrade.

### Key Changes from v1

| Feature | v1 (Current) | v2 (New) |
|---------|-------------|----------|
| Enclosure | Single box (Tinkercad) | 2-part: base + top plate |
| Encoder | 600 PPR optical rotary | MT6701 magnetic (14-bit, I2C) |
| Bearing | None (encoder shaft) | 625 bearing (5mm/16mm/5mm) |
| Display | 1602A LCD (I2C 0x27) | SSD1306 0.96" OLED (I2C 0x3C) |
| Touch sensing | Wire through encoder shaft | Slip ring + spring contact |
| Controls | Front panel (LCD sideways) | Top surface (DJ deck style) |
| Design tool | Tinkercad | OpenSCAD (parametric) |

---

## Architecture

### Two-Part Stack

```
        ┌─────────────────────────────────────────────────────┐
        │              TOP PLATE (Part B)                      │
        │  ┌────────┐     ┌─────────────────────────────┐    │
        │  │  OLED  │     │      PLATTER OPENING        │    │
        │  │ (up)   │     │       (always exposed)      │    │
        │  └────────┘     │                             │    │
        │  (ENC)          │         HDD disc            │    │
        │  [A] [B]        │        spins here           │    │
        │                 └─────────────────────────────┘    │
        │  ═══════════ FADER ═══════════                     │
        ├─CLIPS──────────────────────────────────────CLIPS───┤
        │            ELECTRONICS BASE (Part A)                │
        │                                                     │
        │  ┌──────────────────────┐   ┌─────────────────┐   │
        │  │  Raspberry Pi 2/3    │   │  MT6701 sensor  │   │
        │  │  + AudioInjector HAT │   │  on pedestal    │   │
        │  │                      │   │  + 625 bearing  │   │
        │  └──────────────────────┘   │  + rotor hub    │   │
        │  ┌───────────┐              └─────────────────┘   │
        │  │Arduino Nano│                                    │
        │  └───────────┘                                     │
        └─────────────────────────────────────────────────────┘
```

### Top Plate Layout (DJ Deck Style)

All controls face UP for natural DJ interaction:

```
     ┌──────────────────────────────────────────────┐
     │                                               │
     │  ┌────────┐      ┌───────────────────────┐   │
     │  │  OLED  │      │                       │   │
     │  │ 0.96"  │      │                       │   │
     │  └────────┘      │    HDD PLATTER        │   │
     │                   │     Ø95mm             │   │
     │  (🔘 ENC)        │   always exposed      │   │
     │  menu knob        │   touch to scratch    │   │
     │                   │                       │   │
     │  [🔴] [🔵]      │                       │   │
     │  Enter  Back      └───────────────────────┘   │
     │                                               │
     │  ══════════════════════════════════════════   │
     │          60mm CROSSFADER                      │
     │                                               │
     └──────────────────────────────────────────────┘
       FRONT
```

---

## Printable Parts (7 total)

### Part A: Electronics Base — `v2_base.scad`

The main enclosure box. Open top, all electronics mount inside.

- **Dimensions:** ~145mm x 175mm x 60mm (outer) — matches v1 footprint
- Pi standoffs (4x M2.5, 6mm height)
- Arduino Nano friction-fit cradle
- MT6701 sensor pedestal with bearing seat (rises from floor to correct height)
- Fader mounting rails
- 6x clip receivers on inner walls
- Back panel: 4x 3.5mm audio jack holes, micro-USB, HDMI, USB-A cutouts
- Left side: Arduino USB programming access
- Ventilation slots on both sides

### Part B: Top Plate — `v2_top_plate.scad`

DJ deck surface. Clips onto base rim.

- **Thickness:** 4mm (structural)
- Platter opening (Ø100mm) with raised cosmetic surround — fits 95mm HDD platter
- OLED window with bezel (screen faces up)
- OLED PCB pocket (recessed from below)
- EC11 encoder hole with raised ring surround
- 2x button holes with surrounds
- Fader slot (66mm x 4mm)
- 6x clip hooks (snap onto base receivers)
- Cable pass-through slot near platter

### Rotor Hub — `v2_rotor_hub.scad`

The spinning part. Press-fits onto 625 bearing inner race.

- **Dimensions:** Ø26mm body, Ø32mm flange, 12mm tall
- Bottom: 6x2.5mm magnet pocket (press-fit)
- Middle: Slip ring groove (3mm wide, 0.8mm deep for copper tape)
- Wire channel from ring to flange (for platter connection)
- Top: Platter flange with 3x M2 mounting holes
- Radial M2.5 set screw hole
- **Print: 4 walls, 80% infill, 0.16mm layers**

### Fixed Shaft — `v2_shaft.scad`

Stationary shaft through the bearing center.

- Ø4.8mm shaft (clearance fit in 5mm bore)
- Base flange Ø9mm
- M3 through-bolt for securing to pedestal
- Countersink on top for bolt head
- **Print: 100% infill**

### Spring Contact Holder — `v2_spring_holder.scad`

Holds the brass spring strip that presses against the slip ring.

- Slot for 4mm wide spring strip
- Chamfered entry for easy spring insertion
- M2 clamping screw hole (presses spring down)
- Wire hole in bottom (solder connection to Arduino D12)
- Mounting tab with M3 hole (attaches to sensor pedestal)

### Platter Adapter Ring — `v2_platter_adapter.scad`

Optional ring to center the HDD platter on the hub flange.

- Recess for Ø95mm platter
- Center hole clears hub flange
- Weight reduction cutouts

### Legacy reference: `mt6701_mount.scad`

Earlier standalone MT6701 mount design. Superseded by the integrated pedestal in `v2_base.scad`, kept for reference.

---

## Slip Ring System (Capacitive Touch)

### Problem

The v1 design routes a copper wire through the optical encoder shaft to make electrical contact with the HDD platter. With the MT6701 magnetic encoder (contactless), there is no mechanical shaft connection to the platter.

### Solution: Rotating Slip Ring + Fixed Spring Contact

```
    Arduino D12 (sense) ──wire──┐
                                │
                        ╔═══════╧═══════╗
                        ║ SPRING CONTACT ║  (fixed, brass strip)
                        ║   presses on   ║
                        ╚═══════╤═══════╝
                                │ physical contact
                        ╔═══════╧═══════╗
                        ║  COPPER RING   ║  (rotates with hub)
                        ║  in hub groove ║
                        ╚═══════╤═══════╝
                                │ soldered wire
                        ╔═══════╧═══════╗
                        ║  HDD PLATTER   ║  (metal, user touches)
                        ╚═══════════════╝
                                │
                           User's finger
                         (body capacitance)
```

### Circuit

The existing Arduino capacitive sensing circuit is **unchanged**:

```
Arduino D10 ──── 1.2kΩ ──── Arduino D12 ──── wire ──── spring contact
                                                              │
                                                        copper ring
                                                              │
                                                        HDD platter
```

The `CapacitiveSensor` library measures charge time through the 1.2kΩ resistor. When a finger touches the platter, body capacitance increases the charge time, and the reading goes up.

### Materials for Slip Ring

| Item | Specification | Source |
|------|--------------|--------|
| Copper tape | 3mm wide, adhesive back | AliExpress / electronics store |
| Spring strip | Phosphor bronze, 0.1-0.2mm thick, 3mm wide | Cut from shim stock |
| Wire | 26-30 AWG silicone | Standard hookup wire |

### Assembly

1. Wrap copper tape into the rotor hub groove (full ring, overlapping ends)
2. Solder a wire from the copper ring through the hub wire channel to the platter contact point on the flange
3. Bend the brass spring strip — one end curved (contact tip), other end flat (solder pad)
4. Insert spring into holder slot, curved tip pointing toward hub
5. Secure with M2 screw through clamping hole
6. Solder wire from spring flat end to Arduino D12

---

## Wiring Changes (v1 → v2)

### Removed

- 1602A LCD display (I2C address 0x27)
- 600 PPR optical rotary encoder (Arduino D2/D3 interrupts)
- Encoder shaft wire for capacitive touch

### Added

- SSD1306 0.96" OLED (I2C address 0x3C, same SDA/SCL pins)
- MT6701 magnetic encoder (I2C address 0x06, same I2C bus, read directly by Pi)
- Slip ring system (replaces shaft wire, same D10/D12 circuit)

### Modified Arduino Protocol

The Arduino no longer reads the platter encoder. The serial packet shrinks:

| v1 Packet (8 bytes) | v2 Packet (6 bytes) |
|---------------------|---------------------|
| `0xAA` sync | `0xAA` sync |
| Fader high | Fader high |
| Fader low | Fader low |
| ~~Angle high~~ | Cap high |
| ~~Angle low~~ | Cap low |
| Cap high | Checksum (XOR 1-4) |
| Cap low | |
| Checksum | |

The Pi reads the MT6701 angle directly over I2C at address 0x06 (14-bit, register 0x03-0x04).

### I2C Bus Map

| Address | Device | Notes |
|---------|--------|-------|
| 0x06 | MT6701 | 14-bit angle, read by Pi at ~1kHz |
| 0x3C | SSD1306 OLED | 128x64 pixels, menu display |

---

## Dimensions Summary

| Parameter | V1 (OBJ measured) | V2 (Design) |
|-----------|-------------------|-------------|
| Outer width | 140mm | 145mm |
| Outer depth | 176mm | 175mm |
| Base height | 62mm | 60mm |
| Top plate | N/A (open) | 4mm |
| Total height | 70mm (incl. LCD bracket) | 64mm + hub + platter |
| Platter bay | 120x120mm square | Ø100mm circle |
| Fader slot | 60x9mm | 60x4mm |
| Wall thickness | ~2.5mm | 2.5mm |
| Corner radius | ~5mm | 5mm |

---

## Print Settings

| Part | Material | Layer | Walls | Infill | Supports |
|------|----------|-------|-------|--------|----------|
| Base | PETG | 0.2mm | 3 | 20% | Yes (back panel cutouts) |
| Top plate | PETG | 0.2mm | 3 | 20% | Yes (clip hooks) |
| Rotor hub | PETG | 0.16mm | 4 | 80% | No |
| Shaft | PETG | 0.16mm | 4 | 100% | No |
| Spring holder | PETG | 0.16mm | 3 | 40% | No |
| Platter adapter | PLA/PETG | 0.2mm | 2 | 15% | No |

---

## Legacy Files (v1 Reference)

v1 Tinkercad files (`djsc.obj`, `tinker.obj`) have been removed. They are available in the Git history under the `mk1` tag if needed.

---

## Next Steps

1. **Open `v2_assembly.scad` in OpenSCAD** — verify layout, adjust parameters
2. **Measure your actual components with calipers** — update `v2_common.scad`
3. **Print the rotor hub first** — test bearing fit and slip ring groove
4. **Modify `sc_input.c`** — add I2C MT6701 reading, update serial protocol
5. **Modify `lcd_menu.c`** — replace 1602 LCD driver with SSD1306 OLED driver
6. **Update Arduino firmware** — remove encoder reading, simplify packet to 6 bytes
