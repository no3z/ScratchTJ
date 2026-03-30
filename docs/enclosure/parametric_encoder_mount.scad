// Parametric MT6701 Encoder Mount
// ==================================
//
// This is a fully parametric version of the AS5600 adapted mount.
// All key dimensions can be changed, and the design recomputes automatically.
//
// Parts:
//   1. BASE: Holds PCB, bearing seat, mounting holes
//   2. CAP: Accommodates bolt head and washer
//   3. PLATTER: Adapter ring for platter support
//   4. BOLT: M5 bolt (not printed) with magnet at bottom end
//
// Assembly order:
//   1. Bearing into base seat
//   2. Drop bolt (with magnet) through bearing from top
//   3. PCB slides into base from below (position regulated for perfect air gap)
//   4. Cap press-fits onto bolt shaft
//   5. Platter adapter on cap body
//
// Key features:
//   - Bolt head sits directly above cap
//   - PCB position automatically regulated for perfect air gap
//   - All dimensions parametric for different bearings/shafts

$fn = $preview ? 32 : 80;

// =====================
// WHAT TO RENDER — change these, press F5
// =====================
// SHOW modes: "assembly" "exploded" "cross_section" "base" "cap" "platter" "print"
SHOW          = "base";
show_mock     = true;
show_labels   = false;
explode_gap   = 10;

// =====================
// PARAMETRIC DIMENSIONS
// =====================

// -- BEARING --
brg_bore     = 5.0;     // inner diameter (bore) - fits shaft
brg_od       = 18.5;    // outer diameter
brg_h        = 5.5;     // thickness
brg_tol      = 0.15;    // press-fit tolerance for seat

// -- BOLT (replaces plain shaft) --
bolt_dia      = 5.0;     // bolt shaft diameter - must fit bearing bore
bolt_length   = 24.0;    // bolt length (excluding head)
bolt_head_dia = 12.5;     // bolt head diameter (e.g., M5 socket head)
bolt_head_h   = 1.5;     // bolt head height


// -- MAGNET (at shaft end) --
mag_dia      = 4.0;     // diameter
mag_h        = 2.0;     // thickness
mag_tol      = 0.1;     // press-fit tolerance

// -- RETENTION (nut + washer below bearing) --
nut_h        = 4.2;     // M6 nut height
nut_af       = 10.0;    // M6 across flats
washer_h     = 1.6;     // M6 washer thickness
washer_od    = 12.0;    // M6 washer OD

// -- MT6701 PCB --
pcb_w        = 23.0;    // width
pcb_d        = 23.0;    // depth
pcb_t        = 1.6;     // thickness
pcb_tol      = 0.3;     // pocket tolerance
pcb_hole_spacing = 19.0; // M2 hole spacing
pcb_hole_dia = 2.2;     // M2 hole diameter

// -- AIR GAP --
air_gap      = 1.0;     // between IC and magnet (datasheet: 0.5-2.0mm)

// -- WALL THICKNESS --
wall         = 6.0;     // base wall thickness

// -- MOUNTING HOLES --
mount_hole_dia = 3.2;   // M3 mounting holes

// -- CAP (press-fits onto bolt shaft) --
cap_flange_dia = 25.0;  // top retention flange
cap_flange_h   = 2.0;   // flange thickness
cap_body_dia   = 20.0;  // main body diameter
cap_body_h     = 6.0;   // body height
cap_bore_tol   = 0.1;   // press-fit tolerance for bolt shaft

// -- PLATTER ADAPTER --
platter_dia    = 52.0;  // support diameter for platter
platter_h      = 2.0;   // thickness
platter_bore_tol = 0.1; // clearance fit over cap body

// -- BASE EXTENSIONS --
mount_drop     = 25.3;   // extra depth below PCB for mounting
dupont_clearance = 23.0; // minimum clearance below PCB for DuPont connectors

// =====================
// COMPUTED GEOMETRY
// =====================

// Bearing seat (with tolerance)
brg_seat_dia = brg_od + brg_tol * 2;

// Magnet pocket (with tolerance)
mag_pocket_dia = mag_dia + mag_tol * 2;

// PCB pocket (with tolerance)
pcb_pocket_w = pcb_w + pcb_tol * 2;
pcb_pocket_d = pcb_d + pcb_tol * 2;

// Center hole - must clear bolt shaft + nut + washer stack
center_hole = max(washer_od + 0.5, bolt_dia + 2.0); // clears nut/washer or bolt

// Base outer dimensions
base_outer = max(max(pcb_pocket_w, pcb_pocket_d) + wall * 2, brg_seat_dia + wall * 2);

// Vertical stack:
// Cap/bearing positions are fixed relative to each other.
// Bolt head seats on the cap, magnet sits at the very bottom of the bolt,
// and the PCB is solved downward from the magnet using the requested air gap.
mag_to_nut_clearance = 0.3;
pcb_ledge_min_z = mount_drop + 0.5;                 // minimum structure below PCB

// Relative positions, using bearing top as Z=0 before final offset.
brg_top_rel       = 0;
brg_bot_rel       = brg_top_rel - brg_h;
cap_bot_rel       = brg_top_rel;
cap_top_rel       = cap_bot_rel + cap_body_h;
bolt_head_bot_rel = cap_top_rel;
bolt_head_top_rel = bolt_head_bot_rel + bolt_head_h;
washer_top_rel    = brg_bot_rel;
washer_bot_rel    = washer_top_rel - washer_h;
nut_top_rel       = washer_bot_rel;
nut_bot_rel       = nut_top_rel - nut_h;
bolt_shank_bot_rel = cap_top_rel - bolt_length;     // head seated on cap top
mag_bot_rel       = bolt_shank_bot_rel;             // magnet sits at bolt bottom
mag_top_rel       = mag_bot_rel + mag_h;
pcb_top_rel       = mag_bot_rel - air_gap;
pcb_ledge_rel     = pcb_top_rel - pcb_t;

// Shift the whole mount upward so the PCB still has the requested lower support.
z_offset      = pcb_ledge_min_z - pcb_ledge_rel;
pcb_ledge_z   = pcb_ledge_rel + z_offset;
pcb_top_z     = pcb_top_rel + z_offset;
mag_bot_z     = mag_bot_rel + z_offset;
mag_top_z     = mag_top_rel + z_offset;
nut_bot_z     = nut_bot_rel + z_offset;
nut_top_z     = nut_top_rel + z_offset;
washer_bot_z  = washer_bot_rel + z_offset;
washer_top_z  = washer_top_rel + z_offset;
brg_bot_z     = brg_bot_rel + z_offset;
brg_top_z     = brg_top_rel + z_offset;
cap_bot_z     = cap_bot_rel + z_offset;
cap_top_z     = cap_top_rel + z_offset;
bolt_head_bot_z = bolt_head_bot_rel + z_offset;
bolt_head_top_z = bolt_head_top_rel + z_offset;
bolt_shank_bot_z = bolt_shank_bot_rel + z_offset;
base_h        = brg_top_z;

// Diagnostics
required_bolt_length = cap_body_h + brg_h + washer_h + nut_h + mag_h + mag_to_nut_clearance;
mag_to_nut_gap = nut_bot_z - mag_top_z;             // free shank between magnet and nut
bolt_below_brg = brg_bot_z - bolt_shank_bot_z;      // total bolt shank below bearing
bolt_above_brg = bolt_head_bot_z - brg_top_z;       // shank above bearing up to the cap

// Cap bore (press-fit onto bolt shaft)
cap_bore = bolt_dia + cap_bore_tol;

// Platter bore (clearance fit over cap body)
platter_bore = cap_body_dia + platter_bore_tol;

// Verification echoes
echo("=== PARAMETRIC MT6701 MOUNT DIMENSIONS ===");
echo(str("Bearing: ", brg_bore, "mm bore, ", brg_od, "mm OD, ", brg_h, "mm thick"));
echo(str("Bolt: M", brg_bore, " x ", bolt_length, "mm (", bolt_below_brg, "mm below brg, ", bolt_above_brg, "mm above)"));
echo(str("Bolt head: ", bolt_head_dia, "mm dia x ", bolt_head_h, "mm (sits on cap)"));
echo(str("Magnet: ", mag_dia, "mm dia x ", mag_h, "mm thick at bolt end"));
echo(str("Base: ", base_outer, "mm x ", base_outer, "mm x ", base_h, "mm"));
echo(str("Center hole: ", center_hole, "mm (clears bolt + retention)"));
echo(str("Air gap: ", air_gap, "mm between PCB and magnet"));
echo(str("PCB top: ", pcb_top_z, "mm, magnet bottom: ", mag_bot_z, "mm"));
echo(str("Magnet-to-nut gap: ", mag_to_nut_gap, "mm"));
if (mag_to_nut_gap < mag_to_nut_clearance)
    echo(str("WARNING: bolt_length is ", mag_to_nut_clearance - mag_to_nut_gap,
             "mm too short for the magnet + nut stack."));

// =====================
// PRINTABLE PARTS
// =====================

// BASE: Main mounting structure
module mount_base() {
    difference() {
        // Main block with rounded corners for better strength
        hull() {
            translate([-base_outer/2 + 2, -base_outer/2 + 2, 0])
                cylinder(r=2, h=base_h);
            translate([base_outer/2 - 2, -base_outer/2 + 2, 0])
                cylinder(r=2, h=base_h);
            translate([-base_outer/2 + 2, base_outer/2 - 2, 0])
                cylinder(r=2, h=base_h);
            translate([base_outer/2 - 2, base_outer/2 - 2, 0])
                cylinder(r=2, h=base_h);
        }

        // PCB pocket (open from below, PCB rests on ledge)
        translate([-pcb_pocket_w/2, -pcb_pocket_d/2, -0.1])
            cube([pcb_pocket_w, pcb_pocket_d, pcb_top_z + 0.1]);

        // DuPont connector clearance (side exit at PCB level)
        translate([0, -dupont_clearance/2, -0.1])
            cube([base_outer/2 + 1, dupont_clearance, pcb_top_z + 0.1]);

        // Center hole (clears shaft + nut + washer)
        translate([0, 0, pcb_top_z - 0.1])
            cylinder(d=center_hole, h=brg_bot_z - pcb_top_z + 0.2);

        // Bearing seat (press-fit with chamfer for easy insertion)
        translate([0, 0, brg_bot_z])
            cylinder(d=brg_seat_dia, h=brg_h + 1.0);
        // Chamfer for bearing insertion
        translate([0, 0, brg_bot_z - 0.5])
            cylinder(d1=brg_seat_dia + 2, d2=brg_seat_dia, h=0.5);

        // PCB mounting holes (M2)
        if (pcb_hole_spacing > 0) {
            for (sx = [-1, 1], sy = [-1, 1]) {
                translate([sx * pcb_hole_spacing/2,
                          sy * pcb_hole_spacing/2, -0.1])
                    cylinder(d=pcb_hole_dia, h=pcb_top_z + 1);
            }
        }

        // Base mounting holes (M3, in corners with countersinks)
        for (sx = [-1, 1], sy = [-1, 1]) {
            translate([sx * (base_outer/2 - wall/2 - 0.5),
                      sy * (base_outer/2 - wall/2 - 0.5), -0.1])
                cylinder(d=mount_hole_dia, h=base_h + 0.2);
            // Countersink for M3 screw head
            translate([sx * (base_outer/2 - wall/2 - 0.5),
                      sy * (base_outer/2 - wall/2 - 0.5), base_h - 2.5])
                cylinder(d=6.0, h=2.6); // M3 screw head recess
        }

        // Additional side mounting holes for flexibility
        for (sx = [-1, 1]) {
            translate([sx * (base_outer/2 - wall/2 - 0.5), 0, base_h/2])
                rotate([90, 0, 0])
                    cylinder(d=mount_hole_dia, h=base_outer/2 + 0.2, center=true);
        }
    }
}

// CAP: Press-fits onto bolt shaft
module mount_cap() {
    difference() {
        union() {
            // Main body
            cylinder(d=cap_body_dia, h=cap_body_h);
            // Top retention flange
            translate([0, 0, cap_body_h - cap_flange_h])
                cylinder(d=cap_flange_dia, h=cap_flange_h);
        }

        // Bore for bolt shaft press-fit
        translate([0, 0, -0.1])
            cylinder(d=cap_bore, h=cap_body_h + 0.2);
    }
}

// PLATTER ADAPTER: Support ring for platter
module mount_platter() {
    difference() {
        // Support ring
        cylinder(d=platter_dia, h=platter_h);

        // Bore - clearance fit over cap body
        translate([0, 0, -0.1])
            cylinder(d=platter_bore, h=platter_h + 0.2);
    }
}

// =====================
// MOCK COMPONENTS (for visualization)
// =====================

module mock_bearing() {
    color("Yellow", 0.85)
    difference() {
        cylinder(d=brg_od, h=brg_h);
        translate([0, 0, -0.1])
            cylinder(d=brg_bore, h=brg_h + 0.2);
    }
}

module mock_bolt() {
    color("Silver", 0.9) {
        // Bolt shaft
        cylinder(d=bolt_dia, h=bolt_length);
        // Bolt head
        translate([0, 0, bolt_length])
            cylinder(d=bolt_head_dia, h=bolt_head_h);
    }
}

module mock_magnet() {
    color("Red", 0.9)
        cylinder(d=mag_dia, h=mag_h);
}

module mock_nut() {
    color("DimGray", 0.9)
        cylinder(d=nut_af / cos(30), h=nut_h, $fn=6);
}

module mock_washer() {
    color("Gray", 0.8)
    difference() {
        cylinder(d=washer_od, h=washer_h);
        translate([0, 0, -0.1])
            cylinder(d=bolt_dia + 0.2, h=washer_h + 0.2);
    }
}

module mock_pcb() {
    color("Green", 0.7) {
        translate([-pcb_w/2, -pcb_d/2, 0])
            cube([pcb_w, pcb_d, pcb_t]);
        // IC representation
        translate([-2.5, -2.5, pcb_t])
            cube([5, 5, 0.8]);
    }
    // Pins
    color("Gold")
        for (i = [0:4])
            translate([-5.08 + i*2.54, -0.3, -6])
                cube([0.6, 0.6, 6]);
}

module mock_dupont() {
    color("Black", 0.7)
        translate([-6.35, -1.27, 0])
            cube([12.7, 2.54, 14]);
}

// =====================
// ASSEMBLY VIEWS
// =====================

module mount_assembly() {
    // Base
    color("DarkSlateGray", 0.8) mount_base();

    // Bolt with magnet at bottom end
    if (show_mock)
        translate([0, 0, bolt_shank_bot_z])
            mock_bolt();

    // Cap press-fits onto bolt shaft
    color("SteelBlue", 0.6)
        translate([0, 0, cap_bot_z])
            mount_cap();

    // Platter adapter below cap flange
    color("CornflowerBlue", 0.5)
        translate([0, 0, cap_bot_z + cap_body_h - cap_flange_h - platter_h])
            mount_platter();

    // Mock components
    if (show_mock) {
        translate([0, 0, brg_bot_z]) mock_bearing();
        translate([0, 0, washer_bot_z]) mock_washer();
        translate([0, 0, nut_bot_z]) mock_nut();
        translate([0, 0, mag_bot_z]) mock_magnet();
        translate([0, 0, pcb_ledge_z]) mock_pcb();
        translate([0, 0, pcb_ledge_z - 14]) mock_dupont();
    }
}

module mount_exploded() {
    eg = explode_gap;

    // 1. Base
    color("DarkSlateGray", 0.75) mount_base();

    // 2. Bearing drops into seat
    if (show_mock)
        translate([0, 0, brg_bot_z + eg * 1.5])
            mock_bearing();

    // 3. Bolt + magnet through bearing
    if (show_mock) {
        translate([0, 0, bolt_shank_bot_z + eg * 2.5])
            mock_bolt();
        translate([0, 0, mag_bot_z + eg * 2.5])
            mock_magnet();
    }

    // 4. PCB from below
    if (show_mock) {
        translate([0, 0, pcb_ledge_z - eg * 0.8])
            mock_pcb();
        translate([0, 0, pcb_ledge_z - 14 - eg * 0.8])
            mock_dupont();
    }

    // 5. Cap press-fits onto bolt shaft
    color("SteelBlue", 0.6)
        translate([0, 0, cap_bot_z + eg * 3.5])
            mount_cap();

    // 6. Platter adapter
    color("CornflowerBlue", 0.5)
        translate([0, 0, cap_bot_z + eg * 3.5 + cap_body_h - cap_flange_h - platter_h - eg])
            mount_platter();

    // Labels
    if (show_labels) {
        color("White") {
            lx = base_outer/2 + 4;
            translate([lx, 0, base_h/2])
                rotate([90, 0, 90])
                    text("1. Base", size=2.5, halign="left");
            translate([lx, 0, brg_bot_z + eg*1.5 + brg_h/2])
                rotate([90, 0, 90])
                    text("2. Bearing", size=2.5, halign="left");
            translate([lx, 0, brg_top_z + eg*2.5 + bolt_length/2])
                rotate([90, 0, 90])
                    text("3. Bolt+Magnet", size=2.5, halign="left");
            translate([lx, 0, pcb_ledge_z - eg*0.8])
                rotate([90, 0, 90])
                    text("4. PCB", size=2.5, halign="left");
            translate([lx, 0, cap_bot_z + eg*3.5 + cap_body_h/2])
                rotate([90, 0, 90])
                    text("5. Cap on bolt", size=2.5, halign="left");
            translate([lx, 0, cap_bot_z + eg*3.5 + cap_body_h - cap_flange_h - platter_h/2 - eg])
                rotate([90, 0, 90])
                    text("6. Platter", size=2.5, halign="left");
        }
    }
}

module mount_cross_section() {
    difference() {
        mount_assembly();
        translate([-50, 0, -20])
            cube([100, 50, 80]);
    }
}

// =====================
// RENDER SELECTOR
// =====================

if (SHOW == "assembly")      mount_assembly();
if (SHOW == "exploded")      mount_exploded();
if (SHOW == "cross_section") mount_cross_section();
if (SHOW == "base")          mount_base();
if (SHOW == "cap")           mount_cap();
if (SHOW == "platter")       mount_platter();
if (SHOW == "print") {
    translate([-30, 0, 0]) mount_base();
    translate([30, 10, cap_body_h])
        rotate([180, 0, 0]) mount_cap();
    translate([10, -35, 0]) mount_platter();
}
