// ScratchTJ — Stepped enclosure (flat decks, 90-degree step)
// =========================================================
//
//   Front view:
//                         platter ════════════════
//              ┌───────────────────╥──────────────┐  high deck (flat)
//   low deck   │ riser (90 deg)    ║ bearing      │
//   ┌──────────┘                   ║ magnet       │
//   │ display        USB plugs     ║ MT6701       │
//   │ buttons        pass under    ║ + DuPont     │
//   └──────────────────────────────╨──────────────┘
//    left                                     right
//
//   TRAY       floor + walls + screw posts, one rigid piece. Printed
//              upright, no supports. Pi + AudioInjector, Nano and the
//              MT6701 tube live here and stay put when you open the box.
//   HIGH DECK  flat plate (platter bearing, fader) with the riser as a
//              lip along its left edge. Printed face-down.
//   LOW DECK   flat plate (display, buttons). Printed face-down.
//   CLAMP      small bar, print 2x. Holds the display board.
//
//   A stepped top cannot be one support-free print, so it is two flat
//   plates. Both are screwed from above into the tray posts with
//   countersunk M3. The low deck also sits on a ledge of the riser, so it
//   clamps the high deck along the whole step.
//
// >>> Values tagged MEASURE come from photos / older files, not calipers.

$fn = $preview ? 32 : 80;

// =====================
// WHAT TO RENDER — change these, press F5
// =====================
// SHOW modes: "assembly" "exploded" "open" "section" "tray" "high" "low" "clamp" "print"
SHOW          = "assembly";
show_tray     = true;
show_decks    = true;
show_mock     = true;     // Pi, Nano, display, fader, platter, DuPonts
show_labels   = false;    // text labels (slow! keep false for speed)
deck_alpha    = 1.0;
explode_gap   = 50;

// =====================
// PARAMETERS (all mm)
// =====================

// -- BOX --
box_x        = 168;     // <= 180 for the A1 mini
box_y        = 168;
x_step       = 64;      // the step: low deck left of it, high deck right
h_high       = 0;       // 0 = auto (lowest that fits Pi stack + encoder)
h_low        = 0;       // 0 = auto (lowest that fits display + buttons)
wall         = 2.5;
deck_th      = 3.0;
floor_th     = 3.0;
corner_r     = 7.0;
fit          = 0.3;

// -- DUPONT (straight female on straight male header) --
dupont_len   = 14.0;
hdr_plastic  = 2.5;
dupont_bend  = 6.0;     // generous: room for fingers and wire slack

// -- SCREWS: only M3 and M2 machine screws, threading straight into the
// plastic. Pilots are drawn 0.2 over the tap size because printed holes
// come out tight; the screw cuts its own thread and holds firmly.
//   deck -> posts     M3 x 10 or 12   (9 used)
//   fader             M3 x 6          (2, into the fader's own threads)
//   display clamps    M3 x 8          (4)
//   Pi                M2 x 6 or 8     (4)
//   MT6701            M2 x 6          (up to 4)
m3_pilot     = 2.8;     // M3 thread-forming hole
m3_clear     = 3.4;     // M3 passes freely
m2_pilot     = 1.8;     // M2 thread-forming hole
// "pan" = flat-bottomed recess: seats pan, button and cap heads.
// "csk" = conical recess for countersunk heads, flush with the deck.
m3_head      = "pan";
m3_head_d    = 6.6;
m3_head_h    = 1.2;     // recess depth for "pan" (csk uses 1.8)
post_d       = 9.0;
screw_depth  = 13;      // pilot depth in the posts

// Screw posts [x, y, wall dx, wall dy]; dx/dy = wall the post is tied to
// (0, 0 = free-standing pillar).
posts_low = [
    [  8,   8, -1, -1],
    [ 56,   8,  0, -1],
    [  8, 103, -1,  0],
    [ 56, 103,  0,  0],
    [  8, 160, -1,  1],
    [ 56, 160,  0,  1],
];
posts_high = [
    [ 72,   8,  0, -1],
    [160,   8,  1, -1],
    [160,  45,  1,  0],
    [ 72, 160,  0,  1],
];

// -- PLATTER SHAFT --
plat_cx      = 113;
plat_cy      = 84;
plat_d       = 95;
plat_th      = 1.3;
brg_od       = 18.5;
brg_h        = 5.5;
brg_tol      = 0.15;
brg_proud    = 0.5;
brg_ledge    = 1.0;
nut_clear_d  = 13.0;
nut_h        = 4.2;
washer_h     = 1.6;
mag_h        = 2.0;
air_gap      = 1.0;
cap_body_d   = 20.0;    // existing printed cap (reused)
cap_body_h   = 6.0;
cap_flange_d = 25.0;
cap_flange_h = 2.0;
adapter_d    = 52.0;    // existing printed platter adapter (reused)
adapter_h    = 2.0;
enc_boss_d   = 32.0;
enc_spigot_h = 2.5;
// MEASURE: under the bolt head down to the magnet face. The current
// build is 24. 19.5 is the shortest that still takes washer + nut, and it
// is what lets the high deck drop. Put your real value here: the tube,
// the MT6701 height and the box height all follow it (24 -> deck +4.5 mm).
shaft_len    = 19.5;

// -- CAPACITIVE TOUCH WIRE --
cap_wire_w   = 2.5;
cap_wire_d   = 0.8;
cap_wire_ang = 270;     // towards the Nano

// -- MT6701 BOARD + TUBE --
enc_pcb      = 23.0;
enc_pcb_t    = 1.6;
enc_pcb_tol  = 0.3;
enc_hole_sp  = 19.0;
enc_tube_wall= 3.7;
enc_shelf    = 1.8;
enc_slot_w   = 15.2;
enc_groove   = 2.2;
enc_rot      = 90;      // cable slot: 0 = front, 90 = right, 180 = back

// -- RASPBERRY PI 2 + AUDIOINJECTOR (rear-right corner) --
// Board X: SD end (0) -> USB end (85). Board Y: HDMI edge (0) -> GPIO (56).
// SD end faces the RIGHT wall, HDMI edge faces the BACK wall, USB ports
// face left (plugs and cable pass under the riser into the low zone).
pi_bx        = 85;
pi_by        = 56;
pi_hole_in   = 3.5;
pi_hole_dx   = 58;
pi_hole_dy   = 49;
pi_standoff  = 3.0;
pi_wall_gap  = 2.2;     // > HDMI overhang, so the Pi drops straight in
pi_stack_top = 27.0;    // MEASURE Pi PCB underside -> top of RCA jacks
pi_headroom  = 0.5;

// Connectors. Heights are above the Pi PCB underside.
// HAT level, back wall: [board X, height, hole diameter]
io_back_hat = [
    [15.1, 18.5,  8.6],   // 3.5 mm jack
    [26.0, 21.4, 11.0],   // RCA
    [40.0, 21.4, 11.0],   // RCA
    [54.5, 20.9,  8.6],   // pot shaft (pull the knob off to assemble)
];
// HAT level, right wall: [board Y, height, hole diameter]
io_right_hat = [
    [11.0, 20.9,  8.6],   // pot shaft
    [23.7, 21.4, 11.0],   // RCA
    [38.8, 21.4, 11.0],   // RCA
];
// Pi level, back wall: [board X, height, width, height]
io_usb_power = [10.6, 3.1, 11, 8];
io_hdmi      = [32.0, 4.8, 22, 12];
show_hdmi    = true;
io_av_jack_x = 53.5;      // unused A/V jack: inner relief only
// microSD, right wall: [board Y, height, width, height]
io_sd        = [28.0, -0.8, 14, 2.8];

// -- ARDUINO NANO (USB plug points to -X) --
nano_cx      = 113;
nano_cy      = 48;
nano_x       = 45.0;    // MEASURE
nano_y       = 18.0;
nano_rail_h  = 5.0;

// -- DISPLAY MODULE (portrait, knob away from you) --
disp_cx      = 32.5;
disp_cy      = 55.5;
disp_bx      = 55.0;
disp_by      = 67.0;
disp_t       = 1.6;
disp_margin  = 3.6;
disp_hole_in = 3.5;
disp_peg_d   = 2.3;
disp_boss_d  = 7.0;
disp_boss_h  = 8.0;
disp_boss_dx = 16.0;
disp_boss_off= 4.5;

// -- PANEL BUTTONS --
btn_x        = 24;
btn_y0       = 114;
btn_pitch    = 14;
btn_n        = 4;
btn_hole_d   = 7.3;     // MEASURE bushing thread + 0.3
btn_depth    = 19;      // MEASURE body + terminals below the panel

// -- FADER (Innofader Mini Pro on 2x M3 from the top) --
fad_cx       = 113;
fad_cy       = 18;
fad_m3_sp    = 65.0;    // MEASURE
fad_slot_x   = 50.0;    // MEASURE
fad_slot_y   = 4.0;     // MEASURE
fad_relief_y = 8.0;
fad_relief_h = 1.2;
fad_body     = [73, 17, 7];

// -- RISER --
riser_th     = 2.5;
ledge_w      = 3.0;     // low deck rests on this, clamping the high deck
ledge_y      = [[16, 96], [114, 152]];

// -- VENTS --
vent_n       = 6;
vent_w       = 2.5;
vent_h       = 12;

// =====================
// COMPUTED GEOMETRY
// =====================

e = 0.05;

dupont_stack = hdr_plastic + dupont_len + dupont_bend;
enc_pcb_drop = shaft_len - cap_body_h + air_gap;   // bearing top -> PCB top

h_need_enc  = floor_th - enc_groove + dupont_stack + enc_pcb_t
              + enc_pcb_drop - brg_proud;
h_need_pi   = floor_th + pi_standoff + pi_stack_top + pi_headroom + deck_th;
h_need_disp = floor_th + dupont_stack + disp_t + deck_th;
h_need_btn  = floor_th + btn_depth + 3 + deck_th;
Hh       = h_high > 0 ? h_high : ceil(max(h_need_enc, h_need_pi));
Hl       = h_low  > 0 ? h_low  : ceil(max(h_need_disp, h_need_btn) - 0.25);
zh       = Hh - deck_th;                // high deck underside
zl       = Hl - deck_th;                // low deck underside

// Bearing stack, deck coordinates (z = 0 is the outside face)
brg_seat_d  = brg_od + 2 * brg_tol;
brg_top     = brg_proud;
brg_bot     = brg_top - brg_h;
cone_h      = (brg_seat_d - nut_clear_d) / 2 - brg_ledge;
boss_bot    = brg_bot - cone_h - 1.0;
thread_free = shaft_len - mag_h - cap_body_h - brg_h;

// Encoder, world coordinates
enc_pcb_top = Hh + brg_top - enc_pcb_drop;
enc_pcb_bot = enc_pcb_top - enc_pcb_t;
enc_pocket  = enc_pcb + 2 * enc_pcb_tol;
enc_tube    = enc_pocket + 2 * enc_tube_wall;
enc_tube_top= Hh + boss_bot - 0.2;
enc_under   = enc_pcb_bot - (floor_th - enc_groove);
plat_bot    = Hh + brg_top + cap_body_h - cap_flange_h;

// Pi placement
pi_ox   = box_x - wall - pi_wall_gap;   // world X of the SD end
pi_oy   = box_y - wall - pi_wall_gap;   // world Y of the HDMI edge
pi_z    = floor_th + pi_standoff;
function pix(bx) = pi_ox - bx;
function piy(by) = pi_oy - by;

// Display
disp_y0  = disp_cy - disp_by / 2;
disp_y1  = disp_cy + disp_by / 2;
clamp_len  = disp_bx - 1;
clamp_w    = 11;
clamp_th   = 3;
clamp_foot = disp_boss_h - disp_t;

echo("=== SCRATCHTJ STEPPED ===");
echo(str("Box: ", box_x, " x ", box_y, " mm.  High deck ", Hh, " mm, low deck ", Hl,
         " mm, step ", Hh - Hl, " mm"));
echo(str("High deck needs  encoder: ", h_need_enc, "  Pi stack: ", h_need_pi));
echo(str("Low deck needs   display: ", h_need_disp, "  buttons: ", h_need_btn));
echo(str("Platter top: ", plat_bot + plat_th, " mm above the table"));
echo(str("Under MT6701: ", enc_under, " mm, straight DuPont + bend needs ", dupont_stack));
echo(str("Free above the AudioInjector PCB: ", zh - (pi_z + 14.2), " mm"));
echo(str("Free above the Nano board: ", zh - (floor_th + 3.6), " mm, DuPont + bend needs ", dupont_stack));
echo(str("Shaft: ", shaft_len, " mm under the head incl. magnet; thread below bearing ",
         thread_free, " mm (nut ", nut_h, " + washer ", washer_h, ")"));
if (thread_free < nut_h + washer_h)
    echo("NOTE: no room for nut + washer below the bearing: fit the nut without washer");
if (thread_free < nut_h)
    echo("WARNING: shaft too short for the nut");
if (enc_under < dupont_stack)
    echo("WARNING: high deck too low for a straight DuPont under the MT6701");
if (Hh < h_need_pi)
    echo("WARNING: high deck too low for the Pi + AudioInjector stack");
if (Hl >= Hh)
    echo("WARNING: low deck is not lower than the high deck");

// =====================
// HELPERS
// =====================

module rrect(x, y, r) {
    translate([r, r]) offset(r) square([x - 2 * r, y - 2 * r]);
}

module rrect_c(x, y, r) {
    translate([-x / 2, -y / 2]) rrect(x, y, r);
}

// M3 through a deck with a head recess on the outside face (deck coords).
module csk(x, y) {
    translate([x, y, -deck_th - 1]) cylinder(d = m3_clear, h = deck_th + 2);
    if (m3_head == "csk")
        translate([x, y, -1.8]) cylinder(d1 = m3_clear, d2 = m3_head_d, h = 1.8 + e);
    else
        translate([x, y, -m3_head_h]) cylinder(d = m3_head_d, h = m3_head_h + e);
}

// Thread-forming pilot going down from z = 0, with a lead-in so the
// screw starts straight.
module pilot(d, depth) {
    translate([0, 0, -depth]) cylinder(d = d, h = depth + e);
    translate([0, 0, -0.5]) cylinder(d1 = d, d2 = d + 1, h = 0.5 + e);
}

// U-notch through a wall: round hole, open up to the deck.
module key_back(x, zc, d) {
    hull() {
        translate([x, box_y - wall / 2, zc]) rotate([90, 0, 0]) cylinder(d = d, h = wall + 2, center = true);
        translate([x - d / 2, box_y - wall - 1, Hh]) cube([d, wall + 2, e]);
    }
}

module key_right(y, zc, d) {
    hull() {
        translate([box_x - wall / 2, y, zc]) rotate([0, 90, 0]) cylinder(d = d, h = wall + 2, center = true);
        translate([box_x - wall - 1, y - d / 2, Hh]) cube([wall + 2, d, e]);
    }
}

// Window with 45-degree top corners (bridges cleanly when printed upright).
module window_back(x, zc, w, h) {
    ch = min(2, w / 4);
    translate([x, box_y - wall - 1, zc - h / 2]) hull() {
        translate([-w / 2, 0, 0]) cube([w, wall + 2, h - ch]);
        translate([-w / 2 + ch, 0, 0]) cube([w - 2 * ch, wall + 2, h]);
    }
}

module window_right(y, zc, w, h) {
    translate([box_x - wall - 1, y - w / 2, zc - h / 2]) cube([wall + 2, w, h]);
}

// =====================
// PRINTABLE PARTS
// =====================
// Decks are modelled in deck coordinates: world X/Y, z = 0 at the outside
// face, body and features below.

// ---- HIGH DECK: platter bearing, fader, riser lip

module high_solids() {
    intersection() {
        translate([0, 0, -deck_th]) linear_extrude(deck_th) rrect(box_x, box_y, corner_r);
        translate([x_step, -1, -deck_th - 1]) cube([box_x, box_y + 2, deck_th + 2]);
    }

    // riser: lip down to just below the low deck, between the end walls
    translate([x_step, wall + fit, zl - 0.5 - Hh])
        cube([riser_th, box_y - 2 * (wall + fit), Hh - zl + 0.5 - deck_th + e]);
    // ledge under the low deck edge (45-degree underside, prints unsupported)
    for (seg = ledge_y)
        translate([x_step, seg[0], zl - Hh]) rotate([-90, 0, 0])
            linear_extrude(seg[1] - seg[0])
                polygon([[riser_th, 0], [-ledge_w, 0], [0, ledge_w], [riser_th, ledge_w]]);

    // bearing boss + square spigot that plugs into the MT6701 tube
    translate([plat_cx, plat_cy, 0]) {
        translate([0, 0, boss_bot]) cylinder(d = enc_boss_d, h = -deck_th - boss_bot + e);
        rotate([0, 0, enc_rot]) translate([0, 0, boss_bot - enc_spigot_h]) hull() {
            linear_extrude(e) rrect_c(enc_pocket - 1.6, enc_pocket - 1.6, 1.5);
            translate([0, 0, 1]) linear_extrude(enc_spigot_h - 1 + e)
                rrect_c(enc_pocket - 0.5, enc_pocket - 0.5, 1.5);
        }
    }

    // tabs that close the U-notches above each jack and key the deck
    for (c = io_back_hat) {
        top_z = pi_z + c[1] + c[2] / 2 + 0.3 - Hh;
        if (-deck_th - top_z > 0.4)
            translate([pix(c[0]) - c[2] / 2 + 0.2, box_y - wall, top_z])
                cube([c[2] - 0.4, wall, -deck_th - top_z + e]);
    }
    for (c = io_right_hat) {
        top_z = pi_z + c[1] + c[2] / 2 + 0.3 - Hh;
        if (-deck_th - top_z > 0.4)
            translate([box_x - wall, piy(c[0]) - c[2] / 2 + 0.2, top_z])
                cube([wall, c[2] - 0.4, -deck_th - top_z + e]);
    }
}

module high_cuts() {
    for (p = posts_high) csk(p[0], p[1]);

    // fader: stem slot, rail relief, 2x M3 countersunk
    translate([fad_cx, fad_cy, -deck_th - 1]) linear_extrude(deck_th + 2)
        rrect_c(fad_slot_x, fad_slot_y, fad_slot_y / 2 - 0.01);
    translate([fad_cx, fad_cy, -deck_th - e]) linear_extrude(fad_relief_h + e)
        rrect_c(fad_slot_x + 4, fad_relief_y, 2);
    for (sx = [-1, 1]) csk(fad_cx + sx * fad_m3_sp / 2, fad_cy);

    // bearing seat (pressed in from outside), ledge cone, nut clearance
    translate([plat_cx, plat_cy, 0]) {
        translate([0, 0, brg_bot]) cylinder(d = brg_seat_d, h = brg_h + 1);
        translate([0, 0, -0.4]) cylinder(d1 = brg_seat_d, d2 = brg_seat_d + 0.8, h = 0.4 + e);
        translate([0, 0, brg_bot - cone_h])
            cylinder(d1 = nut_clear_d, d2 = brg_seat_d - 2 * brg_ledge, h = cone_h + e);
        translate([0, 0, boss_bot - enc_spigot_h - 1])
            cylinder(d = nut_clear_d, h = brg_bot - boss_bot + enc_spigot_h + 1);
        // capacitive-touch wire: in through a slit in the boss wall (outside
        // the tube, clear of the spinning nut), then up a shallow groove
        // where the pressed-in bearing pinches it
        rotate([0, 0, cap_wire_ang]) {
            translate([brg_seat_d / 2 - 1, -cap_wire_w / 2, boss_bot + 1])
                cube([cap_wire_d + 1, cap_wire_w, -boss_bot]);
            translate([brg_seat_d / 2 - 1, -cap_wire_w / 2, boss_bot + 1])
                cube([enc_boss_d / 2, cap_wire_w, -deck_th - boss_bot - 1]);
        }
    }
}

module high_deck() {
    difference() {
        high_solids();
        high_cuts();
    }
}

// ---- LOW DECK: display, buttons

module low_solids() {
    intersection() {
        translate([0, 0, -deck_th]) linear_extrude(deck_th) rrect(box_x, box_y, corner_r);
        translate([-1, -1, -deck_th - 1]) cube([x_step - 0.2 + 1, box_y + 2, deck_th + 2]);
    }
    for (sx = [-1, 1], sy = [-1, 1]) {
        translate([disp_cx + sx * (disp_bx / 2 - disp_hole_in),
                   disp_cy + sy * (disp_by / 2 - disp_hole_in), -deck_th - disp_t + 0.2])
            cylinder(d = disp_peg_d, h = disp_t - 0.2 + e);
        translate([disp_cx + sx * disp_boss_dx,
                   disp_cy + sy * (disp_by / 2 + disp_boss_off), -deck_th - disp_boss_h])
            cylinder(d = disp_boss_d, h = disp_boss_h + e);
    }
}

module low_cuts() {
    for (p = posts_low) csk(p[0], p[1]);

    translate([disp_cx, disp_cy, -deck_th - 1]) linear_extrude(deck_th + 2)
        rrect_c(disp_bx - 2 * disp_margin, disp_by - 2 * disp_margin, 2.2);
    for (sx = [-1, 1], sy = [-1, 1])
        translate([disp_cx + sx * disp_boss_dx,
                   disp_cy + sy * (disp_by / 2 + disp_boss_off), -deck_th - disp_boss_h - e])
            cylinder(d = m3_pilot, h = disp_boss_h + 1.8);

    for (i = [0 : btn_n - 1])
        translate([btn_x, btn_y0 + i * btn_pitch, -deck_th - 1])
            cylinder(d = btn_hole_d, h = deck_th + 2);
}

module low_deck() {
    difference() {
        low_solids();
        low_cuts();
    }
}

// ---- TRAY

module post(p, h) {
    x = p[0]; y = p[1]; dx = p[2]; dy = p[3];
    hull() {
        translate([x, y, 0]) cylinder(d = post_d, h = h);
        if (dx != 0)
            translate([dx < 0 ? wall - e : box_x - wall, y - post_d / 2, 0]) cube([e, post_d, h]);
        if (dy != 0)
            translate([x - post_d / 2, dy < 0 ? wall - e : box_y - wall, 0]) cube([post_d, e, h]);
        // free-standing pillar: stretch it along Y so it is stiff
        if (dx == 0 && dy == 0)
            translate([x, y + 6, 0]) cylinder(d = post_d, h = h);
    }
}

// MT6701 tube, local frame: centred, cable slot towards -Y.
module enc_tube() {
    lower = enc_pocket - 2 * enc_shelf;
    pillar = 4.4;
    difference() {
        translate([0, 0, floor_th - e]) linear_extrude(enc_tube_top - floor_th + e)
            rrect_c(enc_tube, enc_tube, 2);
        translate([0, 0, enc_pcb_bot]) linear_extrude(Hh) rrect_c(enc_pocket, enc_pocket, 0.6);
        difference() {
            translate([0, 0, floor_th]) linear_extrude(Hh) square(lower, center = true);
            for (sx = [-1, 1], sy = [-1, 1])
                translate([sx * (lower - pillar) / 2, sy * (lower - pillar) / 2, 0])
                    linear_extrude(enc_pcb_bot) square(pillar, center = true);
        }
        translate([-enc_slot_w / 2, -enc_tube, floor_th]) cube([enc_slot_w, enc_tube - 6.5, Hh]);
        for (sx = [-1, 1], sy = [-1, 1])
            translate([sx * enc_hole_sp / 2, sy * enc_hole_sp / 2, enc_pcb_bot]) pilot(m2_pilot, 8);
    }
}

module enc_floor_groove() {
    translate([-enc_slot_w / 2, -enc_tube / 2 - 12, floor_th - enc_groove])
        cube([enc_slot_w, enc_tube / 2 + 12 - 6.5, enc_groove + e]);
}

module nano_cradle() {
    gap = nano_y + 0.6;
    for (sy = [-1, 1]) {
        translate([-nano_x / 2, sy * (gap / 2 + 1) - 1, floor_th - e]) cube([nano_x, 2, nano_rail_h + e]);
        translate([-nano_x / 2, sy * (gap / 2 - 0.6) - 0.6, floor_th - e]) cube([nano_x, 1.2, 2 + e]);
    }
    translate([nano_x / 2, -gap / 2 - 2, floor_th - e]) cube([2, gap + 4, nano_rail_h + e]);
}

module tray_solids() {
    difference() {
        linear_extrude(zh) rrect(box_x, box_y, corner_r);
        translate([wall, wall, floor_th]) linear_extrude(Hh)
            rrect(box_x - 2 * wall, box_y - 2 * wall, corner_r - wall);
    }
    for (p = posts_low)  post(p, zl);
    for (p = posts_high) post(p, zh);

    for (bx = [pi_hole_in, pi_hole_in + pi_hole_dx], by = [pi_hole_in, pi_hole_in + pi_hole_dy])
        translate([pix(bx), piy(by), floor_th - e]) cylinder(d = 6.5, h = pi_standoff + e);

    translate([nano_cx, nano_cy, 0]) nano_cradle();
    translate([plat_cx, plat_cy, 0]) rotate([0, 0, enc_rot]) enc_tube();
}

module tray_cuts() {
    // low zone: walls stop under the low deck
    translate([-1, -1, zl]) cube([x_step + 1, box_y + 2, Hh]);

    for (p = posts_low)  translate([p[0], p[1], zl]) pilot(m3_pilot, screw_depth);
    for (p = posts_high) translate([p[0], p[1], zh]) pilot(m3_pilot, screw_depth);

    for (bx = [pi_hole_in, pi_hole_in + pi_hole_dx], by = [pi_hole_in, pi_hole_in + pi_hole_dy])
        translate([pix(bx), piy(by), pi_z]) pilot(m2_pilot, pi_z - 1);
    for (i = [0 : 4])
        translate([pix(18 + i * 7) - vent_w / 2, piy(45), -e]) cube([vent_w, 34, floor_th + 2 * e]);

    translate([plat_cx, plat_cy, 0]) rotate([0, 0, enc_rot]) enc_floor_groove();

    // back wall: power, HDMI, jack / RCA / pot
    window_back(pix(io_usb_power[0]), pi_z + io_usb_power[1], io_usb_power[2], io_usb_power[3]);
    if (show_hdmi)
        window_back(pix(io_hdmi[0]), pi_z + io_hdmi[1], io_hdmi[2], io_hdmi[3]);
    for (c = io_back_hat) key_back(pix(c[0]), pi_z + c[1], c[2]);
    translate([pix(io_av_jack_x) - 4.5, box_y - wall - e, pi_z]) cube([9, 0.9, Hh]);

    // right wall: microSD, pot / RCA
    window_right(piy(io_sd[0]), pi_z + io_sd[1], io_sd[2], io_sd[3]);
    for (c = io_right_hat) key_right(piy(c[0]), pi_z + c[1], c[2]);

    // vents, right wall in front of the Pi
    for (i = [0 : vent_n - 1])
        translate([box_x - wall - 1, 58 + i * 6, zh - 4 - vent_h]) cube([wall + 2, vent_w, vent_h]);
}

module tray() {
    difference() {
        intersection() {
            tray_solids();
            linear_extrude(Hh + 1) rrect(box_x, box_y, corner_r);
        }
        tray_cuts();
    }
}

// ---- DISPLAY CLAMP. Printed as drawn: plate on the bed, feet up. Print 2.
module clamp_bar() {
    difference() {
        union() {
            translate([-clamp_len / 2, 0, 0]) cube([clamp_len, clamp_w, clamp_th]);
            for (sx = [-1, 1])
                translate([sx * (clamp_len / 2 - 4) - 4, clamp_w - 4, clamp_th - e])
                    cube([8, 4, clamp_foot + e]);
        }
        for (sx = [-1, 1])
            translate([sx * disp_boss_dx, 3.5, -e]) cylinder(d = m3_clear, h = clamp_th + 2 * e);
    }
}

// Clamp in deck coordinates. side = -1 front end, +1 rear end of the board.
module clamp_placed(side) {
    off = disp_boss_off + 3.5;
    translate([disp_cx, side < 0 ? disp_y0 - off : disp_y1 + off, -deck_th - disp_boss_h - clamp_th])
        mirror([0, side < 0 ? 0 : 1, 0]) clamp_bar();
}

// =====================
// MOCK COMPONENTS (visualisation only, never exported)
// =====================

// Straight DuPont hanging from z = 0 downwards, row along X.
module mock_dupont(n) {
    color("Gold") translate([-n * 1.27, -0.3, -hdr_plastic]) cube([n * 2.54, 0.6, hdr_plastic]);
    color("#222") translate([-n * 1.27, -1.27, -hdr_plastic - dupont_len]) cube([n * 2.54, 2.54, dupont_len]);
}

module mock_pi() {
    translate([pi_ox - pi_bx, pi_oy - pi_by, pi_z]) {
        color("ForestGreen") cube([pi_bx, pi_by, 1.6]);
        color("Silver") translate([-2, 4, 1.6]) cube([19, 48, 16]);
        color("DodgerBlue", 0.7) translate([-34, 21, 2.5]) cube([32, 12, 7]);   // USB plug, lower port
        color("#222") translate([pi_bx - 58, 1, 1.6]) cube([51, 5, 11]);
        color("RoyalBlue") translate([pi_bx - 65, 0, 12.6]) cube([65, pi_by, 1.6]);
    }
    for (c = io_back_hat)
        color(c[2] > 10 ? "Crimson" : "DimGray")
            translate([pix(c[0]), pi_oy - 6, pi_z + c[1]]) rotate([-90, 0, 0])
                cylinder(d = c[2] - 1.5, h = 6 + (c[2] > 10 ? 10 : 12));
    for (c = io_right_hat)
        color(c[2] > 10 ? "Crimson" : "DimGray")
            translate([pi_ox - 6, piy(c[0]), pi_z + c[1]]) rotate([0, 90, 0])
                cylinder(d = c[2] - 1.5, h = 6 + (c[2] > 10 ? 10 : 12));
    color("Silver") {
        translate([pix(io_usb_power[0]) - 4, pi_oy - 5, pi_z + 1.6]) cube([8, 6, 3]);
        translate([pix(io_hdmi[0]) - 7.5, pi_oy - 10, pi_z + 1.6]) cube([15, 11.8, 6.5]);
    }
}

module mock_nano() {
    translate([nano_cx, nano_cy, floor_th + 2]) {
        color("Teal") translate([-nano_x / 2, -nano_y / 2, 0]) cube([nano_x, nano_y, 1.6]);
        color("Silver") translate([-nano_x / 2 - 2, -4, 1.6]) cube([9, 8, 4]);
        color("DodgerBlue", 0.7) translate([-nano_x / 2 - 26, -5.5, 0.6]) cube([24, 11, 7]);
        for (sy = [-1, 1]) translate([4, sy * 7.6, 1.6]) mirror([0, 0, 1]) mock_dupont(6);
    }
}

// Low-deck hardware, deck coordinates.
module mock_low() {
    translate([disp_cx, disp_cy, -deck_th - disp_t]) {
        color("MidnightBlue") translate([-disp_bx / 2, -disp_by / 2, 0]) cube([disp_bx, disp_by, disp_t]);
        color("Black") translate([-16, -disp_by / 2 + 5, disp_t]) cube([32, 34, 2.5]);
        color("#333") translate([-12, disp_by / 2 - 12, disp_t]) cylinder(d = 14, h = 20);
        color("#333") translate([10, disp_by / 2 - 12, disp_t]) cylinder(d = 7, h = 5.5);
        translate([-disp_bx / 2 + 2, -12, 0]) rotate([0, 0, 90]) mock_dupont(11);
    }
    for (i = [0 : btn_n - 1]) translate([btn_x, btn_y0 + i * btn_pitch, 0]) {
        color("Silver") translate([0, 0, -deck_th - btn_depth]) cylinder(d = 10, h = btn_depth);
        color(["Red", "Green", "Gold", "Blue"][i % 4]) cylinder(d = 6, h = 9);
    }
}

// High-deck hardware (lifts off with the deck), deck coordinates.
module mock_high() {
    mag_face = brg_top - enc_pcb_drop + air_gap;
    translate([fad_cx, fad_cy, 0]) {
        color("DarkSlateGray") translate([-fad_body[0] / 2, -fad_body[1] / 2, -deck_th - fad_body[2]]) cube(fad_body);
        color("Silver") translate([-12, -1, -deck_th]) cube([2, 2, deck_th + 8]);
        color("White") translate([-17, -5, 3]) cube([12, 10, 9]);
    }
    translate([plat_cx, plat_cy, 0]) {
        color("Yellow") translate([0, 0, brg_bot]) difference() {
            cylinder(d = brg_od, h = brg_h);
            translate([0, 0, -1]) cylinder(d = 5, h = brg_h + 2);
        }
        color("Silver") translate([0, 0, mag_face + mag_h]) cylinder(d = 5, h = brg_top + cap_body_h - mag_face - mag_h);
        color("Red") translate([0, 0, mag_face]) cylinder(d = 4, h = mag_h);
        color("DimGray") translate([0, 0, brg_bot - washer_h - nut_h]) cylinder(d = 9.2, h = nut_h, $fn = 6);
        color("SteelBlue") translate([0, 0, brg_top]) {
            cylinder(d = cap_body_d, h = cap_body_h);
            translate([0, 0, cap_body_h - cap_flange_h]) cylinder(d = cap_flange_d, h = cap_flange_h);
        }
        color("CornflowerBlue") translate([0, 0, brg_top + cap_body_h - cap_flange_h - adapter_h])
            cylinder(d = adapter_d, h = adapter_h);
        color("Gainsboro", 0.8) translate([0, 0, brg_top + cap_body_h - cap_flange_h]) difference() {
            cylinder(d = plat_d, h = plat_th);
            translate([0, 0, -1]) cylinder(d = 25, h = plat_th + 2);
        }
        color("Silver") translate([0, 0, brg_top + cap_body_h]) {
            cylinder(d = 16, h = 1.2);
            cylinder(d = 9.5, h = 4);
        }
        color("Chocolate") rotate([0, 0, cap_wire_ang]) {
            translate([brg_od / 2, -1, boss_bot + 1.5]) cube([0.8, 2, brg_top - boss_bot - 2]);
            translate([brg_od / 2, -1, boss_bot + 1.5]) cube([enc_boss_d / 2 + 6, 2, 0.8]);
        }
    }
}

module mock_encoder_fixed() {
    translate([plat_cx, plat_cy, 0]) rotate([0, 0, enc_rot]) {
        color("Green") translate([-enc_pcb / 2, -enc_pcb / 2, enc_pcb_bot]) cube([enc_pcb, enc_pcb, enc_pcb_t]);
        color("#111") translate([-2.5, -2.5, enc_pcb_top]) cube([5, 5, 0.8]);
        translate([0, -enc_pcb / 2 + 1.3, enc_pcb_bot]) mock_dupont(5);
    }
}

module labels() {
    color("Red") {
        translate([box_x / 2, -10, 0]) text(str(box_x, " x ", box_y, "   high ", Hh, " / low ", Hl), size = 6, halign = "center");
        translate([box_x + 4, piy(28), Hh]) rotate([90, 0, 90]) text("SD / RCA", size = 4, halign = "center");
        translate([pix(30), box_y + 4, Hh]) rotate([90, 0, 180]) text("power / HDMI / RCA", size = 4, halign = "center");
    }
}

// =====================
// ASSEMBLY VIEWS
// =====================

module low_group() {
    translate([0, 0, Hl]) {
        if (show_decks) {
            color("#2b2b2b", deck_alpha) low_deck();
            color("Ivory") { clamp_placed(-1); clamp_placed(1); }
        }
        if (show_mock) mock_low();
    }
}

module high_group() {
    translate([0, 0, Hh]) {
        if (show_decks) color("#333333", deck_alpha) high_deck();
        if (show_mock) mock_high();
    }
}

module tray_group() {
    if (show_tray) color("#454545") tray();
    if (show_mock) {
        mock_pi();
        mock_nano();
        mock_encoder_fixed();
    }
}

module assembly(lift_low = 0, lift_high = 0) {
    tray_group();
    translate([0, 0, lift_low]) low_group();
    translate([0, 0, lift_high]) high_group();
    if (show_labels) labels();
}

// =====================
// RENDER SELECTOR — driven by SHOW at the top
// =====================

if (SHOW == "assembly") assembly();
if (SHOW == "exploded") assembly(explode_gap * 1.6, explode_gap);
if (SHOW == "open")     tray_group();                    // what you see with both decks off
if (SHOW == "section")  // cut across the step, through the platter axis, seen from the front
    difference() {
        assembly();
        translate([-10, -10, -10]) cube([box_x + 20, plat_cy + 10, Hh + 60]);
    }
if (SHOW == "tray")  tray();
if (SHOW == "high")  translate([-x_step, box_y, 0]) rotate([180, 0, 0]) high_deck();   // outside face on the bed
if (SHOW == "low")   translate([0, box_y, 0]) rotate([180, 0, 0]) low_deck();          // outside face on the bed
if (SHOW == "clamp") clamp_bar();
if (SHOW == "print") {
    // overview only — export each part separately with -D 'SHOW="..."'
    tray();
    translate([box_x + 12, box_y, 0]) rotate([180, 0, 0]) low_deck();
    translate([box_x + 24, box_y, 0]) rotate([180, 0, 0]) high_deck();
    translate([box_x / 2, -20, 0]) clamp_bar();
    translate([box_x / 2, -36, 0]) clamp_bar();
}
