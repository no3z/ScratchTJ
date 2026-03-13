// ============================================================================
// ScratchTJ — MT6701 Sensor Test Jig
// ============================================================================
//
// Minimal assembly to test the MT6701 magnetic encoder with a 625 bearing
// and HDD platter. Sits flat on a desk. No full enclosure needed.
//
// 4 printable parts:
//   1. test_base()          — Base plate + sensor mount + bearing seat
//   2. test_hub()           — Rotor hub (magnet + slip ring + platter flange)
//   3. test_shaft()         — Fixed shaft through bearing
//   4. test_spring_holder() — Slip ring spring contact
//
// Print: PETG, 0.16-0.2mm layers
// After printing: wire MT6701 I2C to Pi, run i2c test, spin platter.
//
// ============================================================================
//
// VERTICAL STACK (cross-section):
//
//          ┌──── HDD Platter (Ø95mm) ────┐
//          │    ┌── Hub flange Ø43 ──┐    │
//          │    │   [  ROTOR HUB  ]  │    │
//          │    │   ( slip ring   )  │    │
//          │    │   [ magnet 6x2.5]  │    │
//          │    └──┤  625 BEARING ├──┘    │
//          │       │   ┆shaft┆    │       │
//   -------│-------│---┆     ┆----│-------│------- hub_bottom_z
//          │       │   ┆     ┆    │       │
//          │       └───┆─────┆────┘       │
//          │      ·····┆·····┆·····       │  ← air gap (1mm)
//          │     [ MT6701 PCB 23x23]      │
//          │     └────────────────┘       │
//          │       BEARING SEAT           │
//          │       ┌──COLUMN──┐           │
//          └───────┤  BASE PLATE  ├───────┘
//                  └──────────────┘
//                  ~~~~~ desk ~~~~~
//
// The trick: the bearing sits AROUND/ABOVE the sensor column.
// The sensor PCB is recessed so its top surface is exactly
// (air_gap + mag_h) below the bearing top surface.
// The hub sits on the bearing, magnet in hub pocket faces DOWN.
//
// ============================================================================

$fn = 80;

// =====================
// PARAMETERS
// =====================

// -- 625 Bearing --
brg_id        = 5.0;     // Inner bore
brg_od        = 16.0;    // Outer diameter
brg_h         = 5.0;     // Height
brg_tol       = 0.15;    // Press-fit tolerance

// -- MT6701 PCB --
mt_pcb        = 23;      // Square PCB side
mt_thick      = 1.6;     // PCB thickness
mt_hole_sp    = 18;      // Mounting hole spacing
mt_hole_d     = 2.2;     // M2 screw holes

// -- Magnet --
mag_d         = 6.0;     // Diameter (diametrically magnetized)
mag_h         = 2.5;     // Height
mag_tol       = 0.1;     // Press-fit tolerance

// -- Air Gap --
air_gap       = 1.0;     // MT6701 IC to magnet face (datasheet: 0.5-2.0mm)

// -- Rotor Hub --
hub_od        = 26;      // Body outer diameter
hub_h         = 12;      // Total height
hub_flange_d  = 43;      // Platter flange diameter — matches HDD platter inner hole!
                          // (measured from v1 hdd-adapter.stl: flange Ø43mm at platter contact)
hub_flange_h  = 2;       // Flange thickness

// -- Fixed Shaft --
shaft_d       = 4.8;     // Diameter (clearance in 5mm bore)

// -- Slip Ring --
slip_ring_w   = 3;       // Copper ring width
slip_ring_groove = 0.8;  // Groove depth for copper tape
spring_w      = 4;       // Spring strip width
spring_slot_d = 8;       // Spring slot depth
spring_slot_h = 3;       // Spring slot height

// -- HDD Platter --
platter_d     = 95;
platter_t     = 1.5;

// -- Base Plate --
base_w        = 70;      // Base plate width
base_d        = 70;      // Base plate depth
base_floor    = 4;       // Base plate thickness
tol           = 0.3;     // General print tolerance

// =====================
// COMPUTED GEOMETRY
// =====================

// The bearing outer race press-fits into a seat on the column.
// The hub sits ON TOP of the bearing.
// The magnet is in the hub's bottom pocket, facing DOWN.
// The sensor PCB must be positioned so:
//   pcb_top + air_gap + mag_h = bearing_top = hub_bottom = magnet_bottom
//
// bearing_top = base_floor + col_h + brg_h
//   where col_h = height of column above base floor, bearing starts at col_h
//
// BUT the bearing seat needs a lip. Let's define:
//   bearing_seat_z = base_floor + col_h  (bottom of bearing)
//   bearing_top_z  = bearing_seat_z + brg_h
//   hub_bottom_z   = bearing_top_z  (hub sits on bearing)
//   magnet_bottom_z = hub_bottom_z  (magnet at hub bottom)
//
// We need: pcb_top_z + air_gap = magnet_bottom_z - mag_h
//   Wait, magnet pocket is in hub bottom, so magnet bottom face = hub_bottom_z
//   and magnet extends UP into the hub from there.
//   The BOTTOM face of the magnet = hub_bottom_z = the face toward the sensor.
//
// So: pcb_top_z + air_gap = hub_bottom_z
//     pcb_top_z = hub_bottom_z - air_gap
//     pcb_top_z = (base_floor + col_h + brg_h) - air_gap
//
// The PCB is recessed into the column top:
//     pcb_top_z = base_floor + col_h  (PCB top = column top)
//
// Therefore:
//     base_floor + col_h = base_floor + col_h + brg_h - air_gap
//     0 = brg_h - air_gap  ... that doesn't work!
//
// The issue: if the bearing sits right on top of the column (same Z as PCB top),
// the air gap equals the bearing height (5mm), not 1mm.
//
// SOLUTION: The PCB sits INSIDE the column, BELOW the bearing.
// The column extends above the PCB to create the bearing seat.
// The bearing sits in a ring around a central shaft hole.
// The sensor IC (center of PCB) looks up through a window in the column top.
//
// Better geometry:
//   pcb_top_z = base_floor + pcb_shelf_z
//   bearing_bottom_z = pcb_top_z + air_gap + mag_h = pcb_top_z + 3.5
//   col_h = (bearing_bottom_z - base_floor) + brg_h  (column goes up to bearing top)
//
// Wait, let me simplify. Column = solid cylinder. PCB recessed inside.
// Above PCB: a spacer ring with bearing seat.
//
//   base_floor = 4mm
//   pcb_pocket depth from column top: air_gap + mag_h + brg_h = 1 + 2.5 + 5 = 8.5mm
//   No... the bearing is AROUND the column, not on top.
//
// ACTUAL CORRECT GEOMETRY:
// The bearing sits in a RING SEAT around the column top.
// The column center has a hole for the shaft.
// The PCB sits below the bearing, inside the column.
// The magnet on the hub bottom hangs over the PCB center.
//
//   bearing_bottom_z = base_floor + seat_h  (seat_h = height of column to bearing)
//   bearing_top_z = bearing_bottom_z + brg_h
//   hub_bottom_z = bearing_top_z
//   magnet face = hub_bottom_z (facing down)
//
//   pcb_top_z + air_gap = magnet face
//   pcb_top_z = hub_bottom_z - air_gap = bearing_bottom_z + brg_h - air_gap
//
//   pcb_top_z must be inside the column:
//   pcb_top_z = base_floor + seat_h + brg_h - air_gap
//   pcb_bottom_z = pcb_top_z - mt_thick
//
//   For the PCB to fit inside, seat_h must be large enough:
//   pcb_bottom_z >= base_floor  (PCB must be above the base floor)
//   base_floor + seat_h + brg_h - air_gap - mt_thick >= base_floor
//   seat_h >= air_gap + mt_thick - brg_h = 1.0 + 1.6 - 5.0 = -2.4
//
//   So any seat_h >= 0 works! The bearing is tall enough.
//   With seat_h = 0 (bearing sits directly on base floor):
//     pcb_top_z = 4 + 0 + 5 - 1 = 8mm from desk
//     pcb_bottom_z = 8 - 1.6 = 6.4mm from desk
//     PCB center is at 6.4 to 8mm, well above the 4mm base floor.
//
//   Let's use seat_h = 2mm for a small lip under the bearing:
//     bearing_bottom_z = 4 + 2 = 6mm
//     bearing_top_z = 6 + 5 = 11mm
//     hub_bottom_z = 11mm
//     pcb_top_z = 11 - 1 = 10mm
//     pcb_bottom_z = 10 - 1.6 = 8.4mm (comfortably above floor)

seat_h        = 2;       // Column height above base floor to bearing bottom
bearing_bz    = base_floor + seat_h;                    // 6mm
bearing_tz    = bearing_bz + brg_h;                     // 11mm
hub_bz        = bearing_tz;                             // 11mm (hub sits on bearing)
pcb_top_z     = hub_bz - air_gap;                       // 10mm
pcb_bot_z     = pcb_top_z - mt_thick;                   // 8.4mm
col_top_z     = bearing_tz;                             // 11mm (column goes up to bearing top)
shaft_len     = brg_h + hub_h + 3;                      // 20mm

// Verify air gap
echo("=== TEST JIG GEOMETRY ===");
echo(str("Base floor:       0 to ", base_floor, "mm"));
echo(str("PCB bottom:       ", pcb_bot_z, "mm"));
echo(str("PCB top:          ", pcb_top_z, "mm"));
echo(str("Bearing bottom:   ", bearing_bz, "mm"));
echo(str("Bearing top:      ", bearing_tz, "mm"));
echo(str("Hub bottom:       ", hub_bz, "mm"));
echo(str("Air gap:          ", hub_bz - pcb_top_z, "mm (should be ", air_gap, ")"));
echo(str("Hub top (flange): ", hub_bz + hub_h, "mm"));

// =====================
// PART 1: BASE PLATE + SENSOR MOUNT
// =====================
// FDM-optimized: no supports needed.
//   - Column widened to d=30 to fully contain PCB pocket
//   - Retaining lip removed (hub + shaft hold bearing in place)
//   - PCB pocket opens upward (no ceiling = no overhang)
//   - Sensor window tapered to 8mm for easy bridging
//   - Max bridge span: ~4mm (bearing seat to PCB edge)
module test_base() {
    col_od = 30;          // Wide enough to contain 23.6mm PCB pocket
    seat_od = brg_od + brg_tol * 2;  // Bearing seat inner diameter

    difference() {
        union() {
            // Flat base plate
            translate([-base_w/2, -base_d/2, 0])
                cube([base_w, base_d, base_floor]);

            // Lower column — solid up to bearing bottom (no internal voids)
            cylinder(d=col_od, h=bearing_bz);

            // Upper column — bearing seat walls only (ring)
            // Open center merges with PCB pocket = no ceiling overhang
            translate([0, 0, bearing_bz])
                difference() {
                    cylinder(d=col_od, h=brg_h);
                    translate([0, 0, -0.1])
                        cylinder(d=seat_od, h=brg_h + 0.2);
                }

            // Spring holder mount tab (extending from column)
            translate([col_od/2, -5, 0])
                cube([10, 10, bearing_tz]);
        }

        // --- MT6701 PCB pocket (open top — no ceiling!) ---
        // Square pocket from pcb_bot_z up through column top
        translate([-mt_pcb/2 - tol, -mt_pcb/2 - tol, pcb_bot_z])
            cube([mt_pcb + tol*2, mt_pcb + tol*2, col_top_z - pcb_bot_z + 1]);

        // --- Sensor IC window (tapered cone for self-supporting bridge) ---
        // d=12mm at base, narrows to d=8mm at PCB shelf = easy 8mm bridge
        translate([0, 0, base_floor - 0.1])
            cylinder(d1=12, d2=8, h=pcb_bot_z - base_floor + 0.2);

        // --- PCB screw holes M2 (from bottom through column) ---
        for (sx = [-1, 1], sy = [-1, 1])
            translate([sx * mt_hole_sp/2, sy * mt_hole_sp/2, -0.1])
                cylinder(d=mt_hole_d, h=col_top_z + 1);

        // --- Shaft hole (center, all the way through) ---
        translate([0, 0, -0.1])
            cylinder(d=shaft_d + 0.5, h=col_top_z + 2);

        // --- I2C wire channel (slot in column side) ---
        translate([-3, -col_od/2 - 1, pcb_bot_z])
            cube([6, col_od/2 + 2, mt_thick + 1]);

        // --- Rubber feet holes (4 corners of base) ---
        for (sx = [-1, 1], sy = [-1, 1])
            translate([sx * (base_w/2 - 8), sy * (base_d/2 - 8), -0.1])
                cylinder(d=3.2, h=base_floor + 0.2);

        // --- Spring holder screw hole ---
        translate([col_od/2 + 6, 0, bearing_bz + brg_h/2])
            cylinder(d=3.2, h=10);
    }
}

// =====================
// PART 2: ROTOR HUB
// =====================
module test_hub() {
    slip_z = mag_h + 1.5;  // Slip ring Z from hub bottom

    difference() {
        union() {
            // Main body
            cylinder(d=hub_od, h=hub_h);

            // Platter flange
            translate([0, 0, hub_h - hub_flange_h])
                cylinder(d=hub_flange_d, h=hub_flange_h);
        }

        // Bearing inner race press-fit (tight)
        translate([0, 0, -0.1])
            cylinder(d=brg_id + 0.05, h=brg_h + 0.5);

        // Shaft clearance above bearing
        translate([0, 0, brg_h + 0.3])
            cylinder(d=shaft_d + 1.5, h=hub_h);

        // Magnet pocket (bottom, facing down toward sensor)
        translate([0, 0, -0.1])
            cylinder(d=mag_d + mag_tol*2, h=mag_h + 0.1);

        // Slip ring groove (exterior)
        translate([0, 0, slip_z])
            difference() {
                cylinder(d=hub_od + 0.2, h=slip_ring_w);
                translate([0, 0, -0.1])
                    cylinder(d=hub_od - slip_ring_groove*2, h=slip_ring_w + 0.2);
            }

        // Wire channel (ring to flange for platter contact)
        translate([hub_od/2 - slip_ring_groove - 0.3, -1, slip_z])
            cube([slip_ring_groove + 0.5, 2, hub_h - slip_z - hub_flange_h + 0.1]);

        // Wire exit on flange
        translate([hub_flange_d/2 - 5, 0, hub_h - hub_flange_h - 0.1])
            cylinder(d=2, h=hub_flange_h + 0.2);

        // Platter screw holes (3x M2 on flange)
        for (a = [0, 120, 240])
            rotate([0, 0, a])
                translate([hub_flange_d/2 - 4, 0, hub_h - hub_flange_h - 0.1])
                    cylinder(d=2.2, h=hub_flange_h + 0.2);

        // Set screw M2.5 (radial)
        translate([0, 0, brg_h/2 + 0.5])
            rotate([0, 90, 30])
                cylinder(d=2.5, h=hub_od);
    }
}

// =====================
// PART 3: FIXED SHAFT
// =====================
module test_shaft() {
    difference() {
        union() {
            cylinder(d=shaft_d, h=shaft_len);
            // Base flange
            cylinder(d=brg_id + 4, h=2.5);
        }
        // M3 through-bolt
        translate([0, 0, -0.1])
            cylinder(d=3.2, h=shaft_len + 0.2);
        // Countersink top
        translate([0, 0, shaft_len - 3])
            cylinder(d=6, h=3.1);
    }
}

// =====================
// PART 4: SPRING CONTACT HOLDER
// =====================
module test_spring_holder() {
    body_w = spring_w + 4;
    body_d = spring_slot_d + 4;
    body_h = spring_slot_h + 5;

    difference() {
        union() {
            // Main body
            translate([-body_w/2, -body_d/2, 0])
                cube([body_w, body_d, body_h]);
            // Mounting tab
            translate([-body_w/2 - 6, -3, 0])
                cube([6, 6, body_h]);
        }

        // Spring slot (open toward hub)
        translate([-spring_w/2, -body_d/2 - 0.1, 2.5])
            cube([spring_w, spring_slot_d, spring_slot_h]);

        // Wire hole bottom
        translate([0, 0, -0.1])
            cylinder(d=2.2, h=3);

        // Clamp screw M2
        translate([0, spring_slot_d/4, -0.1])
            cylinder(d=2.2, h=body_h + 0.2);

        // Mount screw M3
        translate([-body_w/2 - 3, 0, -0.1])
            cylinder(d=3.2, h=body_h + 0.2);
    }
}

// =====================
// MOCK COMPONENTS (for assembly view)
// =====================

module mock_mt6701() {
    color("ForestGreen", 0.7) {
        cube([mt_pcb, mt_pcb, mt_thick], center=true);
        // IC chip (center)
        translate([0, 0, mt_thick/2])
            color("DarkGray") cube([5, 5, 1], center=true);
    }
}

module mock_bearing() {
    color("Silver", 0.85)
        difference() {
            cylinder(d=brg_od, h=brg_h);
            translate([0, 0, -0.1])
                cylinder(d=brg_id, h=brg_h + 0.2);
        }
    color("DarkRed", 0.4)
        translate([0, 0, 0.5])
            difference() {
                cylinder(d=brg_od - 1, h=brg_h - 1);
                translate([0, 0, -0.1])
                    cylinder(d=brg_id + 1, h=brg_h);
            }
}

module mock_magnet() {
    color("Silver", 0.95) cylinder(d=mag_d, h=mag_h);
    color("Red", 0.4) translate([0, 0, mag_h])
        linear_extrude(0.1) text("N", size=2, halign="center", valign="center");
}

module mock_platter() {
    color("Silver", 0.35)
        difference() {
            cylinder(d=platter_d, h=platter_t);
            translate([0, 0, -0.1])
                cylinder(d=hub_flange_d + tol, h=platter_t + 0.2); // Inner hole = ~43mm
        }
}

module mock_copper_ring() {
    color("Peru", 0.8)
        difference() {
            cylinder(d=hub_od + 0.1, h=slip_ring_w);
            translate([0, 0, -0.1])
                cylinder(d=hub_od - slip_ring_groove*2 - 0.1, h=slip_ring_w + 0.2);
        }
}

module mock_spring() {
    color("Gold", 0.8)
        translate([-spring_w/2, 0, 0])
            cube([spring_w, spring_slot_d - 2, 0.15]);
}

// =====================
// ASSEMBLY VIEW
// =====================
module test_assembly() {
    col_od = 30;  // Must match test_base()

    // 1. Base plate (printed, fixed)
    color("DimGray", 0.5) test_base();

    // 2. MT6701 PCB
    translate([0, 0, pcb_bot_z + mt_thick/2])
        mock_mt6701();

    // 3. 625 Bearing
    translate([0, 0, bearing_bz])
        mock_bearing();

    // 4. Fixed shaft
    color("Gray", 0.6)
        translate([0, 0, base_floor])
            test_shaft();

    // 5. Rotor hub (on bearing)
    color("MediumPurple", 0.65)
        translate([0, 0, hub_bz])
            test_hub();

    // 6. Magnet (in hub pocket, facing down)
    translate([0, 0, hub_bz])
        mock_magnet();

    // 7. Copper slip ring
    translate([0, 0, hub_bz + mag_h + 1.5])
        mock_copper_ring();

    // 8. HDD Platter
    translate([0, 0, hub_bz + hub_h - hub_flange_h + 0.1])
        mock_platter();

    // 9. Spring contact holder
    color("DarkSlateGray", 0.7)
        translate([col_od/2 + 6, 0, bearing_bz + brg_h/2])
            rotate([0, 0, 0])
                test_spring_holder();

    // 10. Brass spring (in holder)
    translate([col_od/2 + 6, 0, bearing_bz + brg_h/2 + 2.5])
        mock_spring();

    // --- Dimension annotations (as thin cylinders) ---

    // Air gap indicator (red line between PCB top and magnet bottom)
    color("Red", 0.8)
        translate([mt_pcb/2 + 3, 0, pcb_top_z])
            cube([0.3, 0.3, air_gap]);

    // Label: air gap
    color("Red")
        translate([mt_pcb/2 + 5, 0, pcb_top_z + air_gap/2])
            rotate([0, 0, 0])
                linear_extrude(0.2)
                    text(str(air_gap, "mm gap"), size=2.5);
}

// =====================
// RENDER SELECTOR
// =====================
// Uncomment ONE, press F6, export STL.

// Assembly visualization (default):
test_assembly();

// Individual parts for printing:
// test_base();
// test_hub();
// test_shaft();
// test_spring_holder();

// All parts on print bed:
// translate([-45, 0, 0]) test_base();
// translate([45, 0, 0]) test_hub();
// translate([0, 40, 0]) test_shaft();
// translate([0, -40, 0]) test_spring_holder();
