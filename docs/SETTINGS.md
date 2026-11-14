# ScratchTJ Settings Guide

This guide explains every setting on the ScratchTJ: what it does, when to change it, and what the SC1000 / SC500 uses.

Out of the box, ScratchTJ plays like an SC1000 / SC500. The extras ScratchTJ adds (Speed assist, Touch by motion, the Curve fader, Fader smoothing and Soft clip) all start **off**, so you can turn them on one at a time and hear what each one does.

---

## Using the Config menu

Open **Main Menu > Config**. You will see:

| Item | What it does |
|------|--------------|
| Sound Settings | The sound card's own controls (output volume, input gain and so on). |
| Platter | How the platter and touch sensor behave. |
| Fader & Volume | How the crossfader cuts the sound, and output clipping. |
| Screen | Screen saver. |
| Save Config | Saves every setting now. You rarely need it: see below. |
| Reset Defaults | Puts every setting back to its default, after asking you to confirm. |
| Restart (rescan files) | Restarts the program so it finds new files in the beats and samples folders. |
| Reboot Pi | Reboots the whole unit. |

Inside **Platter**, **Fader & Volume** or **Screen**, each row shows a setting and its current value.

- **Turn the knob** to move between settings.
- **Press KB0** to open a setting. The edit screen shows:
  - the value, in large type;
  - a live reading for some settings, such as the touch sensor or the fader position;
  - the default and the allowed range;
  - a short explanation.
- **Turn the knob** to change the value. The change takes effect straight away, so you can play while you adjust it. Turning the knob quickly takes bigger steps.
- **Press KB0** to keep the new value, or **push the knob** to undo it and go back to the old value.
- **Push the knob** in the list to leave the section. If you changed anything, it is saved automatically.

### Where settings are stored

ScratchTJ works out each setting's value in three layers. Each layer overrides the one before it:

1. **The built-in default**, which is the SC1000 / SC500 value.
2. **`scsettings.txt`**, the SC1000-style settings file. It is read at startup from `/media/sda/scsettings.txt` if there is a USB stick, otherwise from `~/ScratchTJ/software/scsettings.txt`. The values in this file become the defaults.
3. **`~/.scratchtj/config.cfg`**, which holds what you set in the Config menu. It is written by the menu, by the SETTINGS-mode buttons, and when the unit restarts or reboots from the menu.

**Reset Defaults** goes back to layer 2. That means the built-in values, except where `scsettings.txt` sets something.

---

## Platter

| Setting | Default | What it does |
|---------|---------|--------------|
| Platter speed | 9100 | How far you turn the platter for one second of sound. |
| Reverse platter | On | Flips the platter's direction. |
| Touch threshold | 5000 | The touch-sensor reading above which a hand counts as on the platter. |
| Touch by motion | Off | Treats a turning platter as touched. Only for a broken touch sensor. |
| Slipmat | 200 | How fast the record gets back to speed when you let go. |
| Brake length | 3000 | How long the stop button takes to stop the track. |
| Tracking gain | 40 | How tightly the sound follows the platter under your hand. |
| Pitch response | 0.10 | How quickly the speed of the sound follows changes. |
| Speed assist | Off | Makes the sound follow your hand with less delay. |
| Glitch filter | 400 | Ignores impossible jumps from the platter sensor. |

### Platter speed

This sets how far you turn the platter for one second of sound, which is how "big" the record feels under your hand.

- A higher number means more turning for the same sound, like a record spinning faster.
- The edit screen shows the record speed this feels like.
- **9100** feels like a 33 rpm record, which is the SC1000 default. **12288** feels like 45 rpm.
- You can also change it while playing: in SETTINGS mode, hold the green button (button 2) and turn the platter.

> The number is sensor steps per second of sound. The platter sensor has 16384 steps per turn, so rpm = Platter speed × 60 / 16384.

### Reverse platter

Flips which way is forwards. Change it if turning the record forwards plays the sound backwards.

Deck > Settings > Toggle Jog Reverse, and any button mapped to `JOGREVERSE` in `scsettings.txt`, flip the same setting.

### Touch threshold

The touch sensor in the platter gives a number that goes up when your hand is on it. Above this threshold, the platter counts as touched.

To set it:

1. Open the setting. The **Sensor** line shows the live reading and whether it counts as touched.
2. Note the reading with your hand off the platter, then with your hand resting on it.
3. Set the threshold about halfway between the two readings.

If the threshold is too low, the platter "touches" by itself and the record stops. If it is too high, the platter misses your hand and the record keeps playing under it.

### Touch by motion

When this is on, the platter also counts as touched whenever it is being turned. It is a workaround for a touch sensor that is missing or broken. Leave it **Off** when the sensor works.

It has two downsides:

- A record you hold still starts playing again after a fraction of a second, because a still platter looks the same as no hand.
- After a fast spin, a free-spinning platter keeps control for a moment after you let go, so the track is slow to pick up again.

### Slipmat

This controls how fast the record gets back to normal speed when you let go, or after a backspin. Think of it as how slippery the slipmat is.

- **Lower** is faster, like a tight slipmat.
- **Higher** is slower, with more drag.
- The SC1000 default is **200**.

### Brake length

This controls how long the track takes to stop when you press stop, like a turntable's brake.

- **Higher** is a slower stop.
- **3000** is about half a second, which is the SC1000 default.

### Tracking gain

While your hand is on the platter, the sound is pulled towards where the platter is. This setting is how hard it is pulled.

- **Higher** is tighter, but can buzz or wobble.
- **Lower** is smoother, but the sound lags behind your hand.
- The SC1000 default is **40**.

### Pitch response

This smooths the speed of the sound. It applies under your hand, and also when the record speeds back up after you let go.

- **Lower** is smoother, but slower to react. Letting go of the record also takes longer to get back to speed.
- **Higher** is snappier, but grainier.
- The SC1000 uses **0.10**.

### Speed assist

This is a ScratchTJ extra. It feeds the platter's measured speed straight into the sound, instead of waiting for the position to drift. The sound follows your hand with less delay, and fast scratches sound sharper.

- **Off** is the original SC1000 feel, where only Tracking gain and Pitch response matter.
- **1.0** is full assist.
- Try values from 0.5 to 1.0 and compare.

### Glitch filter

Very occasionally the platter sensor returns a bad reading. A reading that jumps further than this number in one go is skipped, twice in a row at most.

- Raise it if very fast spins stutter.
- Lower it if you hear random jumps.

---

## Fader & Volume

| Setting | Default | What it does |
|---------|---------|--------------|
| Fader mode | SC1000 cut | SC1000-style hard cut, or a ScratchTJ crossfade curve. |
| Cut beats | Off | Which end of the fader (if any) cuts the beats. |
| Open point | 5 | How far from the end the fader must move before the sound comes in. |
| Close point | 3 | How close to the end the fader must come to cut the sound again. |
| Fader direction | Normal | Swaps the two ends of the fader. |
| Curve width | 0.10 | Curve mode: fader travel to full volume. |
| Curve shape | 0.20 | Curve mode: how the volume rises. |
| Fader smoothing | Off | Fades the volume over a few milliseconds instead of jumping. |
| Soft clip | Off | Rounds off peaks when the volume is boosted past 100%. |

The deck names used below:

- **Beats** is deck 1, loaded from the `beats` folder.
- **Sample** is deck 2, loaded from the `samples` folder. It is the deck you scratch.

### Fader mode

- **SC1000 cut** works like the SC1000 and SC500. The beats always play. The sample plays at full volume anywhere along the fader and is cut only at the very ends, like a scratch mixer with a sharp cut-in. **Cut beats**, **Open point** and **Close point** shape this mode.
- **Curve** is a ScratchTJ extra. One end of the fader cuts the beats, the other end cuts the sample, and in between the volume follows a curve set by **Curve width** and **Curve shape**. This is how ScratchTJ behaved before the SC1000 mode was added.

### Cut beats

This applies to SC1000 cut mode only, and matches the SC1000's `cutbeats` setting.

- **Off**: both ends of the fader cut the sample.
- **Left end** or **Right end**: that end cuts the beats instead, so each end cuts one deck.

"Left" is the left of the fader bar on the home screen.

### Open point and Close point

These apply to SC1000 cut mode. The fader position is a number from 0 to 1023, measured from each end.

- **Open point**: a closed end opens (sound comes in) once the fader is more than this far from it. Smaller means a shorter cut-in.
- **Close point**: an open end closes (sound cuts) once the fader comes closer to the end than this.

Close point is always smaller than Open point. The gap between them stops the sound crackling on and off when the fader sits right on the edge.

The SC500, which uses the same Innofader Mini Pro as ScratchTJ, uses **Open 5 / Close 3**. If you fit a different fader and it crackles at the edges, widen the gap or raise both values.

The edit screen shows the live fader position from both ends (`L:` and `R:`), so you can see where your fader cuts.

### Fader direction

Swaps the two ends of the fader. Change it if moving the fader left behaves like moving it right.

### Curve width and Curve shape

These apply to Curve mode only. The edit screen draws the curve: the beats line is cyan and the sample line is orange.

- **Curve width**: how far the fader travels from the end before the sound reaches full volume. Smaller is a sharper cut.
- **Curve shape**:
  - below 1, the sound jumps in fast and then levels off;
  - 1 is a straight line;
  - above 1, it fades in slowly.

### Fader smoothing

This fades the volume over a number of milliseconds, instead of jumping, which softens clicks when you cut.

- **Off** is instant, as on the SC1000. The audio engine still glides across a single 2.7 ms block, so cuts never click badly.
- For scratching, keep it **Off** or at **1 to 3 ms**. More than that makes cuts sound slow.

### Soft clip

In SETTINGS mode, the blue button can push a deck's volume up to 800%. Above 100% the sound clips.

- **Off** gives a hard digital clip, which is the SC1000 sound.
- **On** rounds off loud peaks smoothly, a warmer kind of distortion. It also makes normal levels slightly quieter and uses a little more processing power.

---

## Screen

| Setting | Default | What it does |
|---------|---------|--------------|
| Screen off after | 10 min | Turns the screen off after this many minutes without use. |

The screen turns off after the set time if you have not touched the knob, KB0, the platter, the fader, the touch sensor or the cue buttons.

- Any of those controls wakes it again.
- If you wake it with the knob or KB0, that first turn or press only wakes the screen. It does not also move the menu.
- **Never** keeps the screen on.
- The screen stays on while recording.

> **Hardware note:** "off" puts the display panel to sleep. On units where the display's backlight pin (BLK) is wired straight to 3.3 V, the backlight still glows faintly. To turn it fully dark, the backlight needs to be wired to a GPIO pin instead.

---

## SETTINGS-mode buttons

In SETTINGS mode (Main Menu > **Btn: CUE/SETTINGS**), the four cue buttons become live controls. Hold a button and turn the platter:

| Button | Controls | Range |
|--------|----------|-------|
| 1 (red) | Pitch: musical speed of the active deck | 0.25× to 4× |
| 2 (green) | Platter speed (the same setting as in the Platter section) | 1000 to 32768 |
| 3 (yellow) | Bend: turntable-style tempo nudge, for beat matching | ±8% |
| 4 (blue) | Volume of the active deck | 0 to 800% |

While you hold a button, the platter does not move the sound, and a box on the home screen shows the value. Settings are saved when you release the buttons.

---

## Sound Settings

This lists every control the sound card offers, under the names the card gives them. Changes take effect straight away and are saved when you leave the list. They are restored at the next boot.

---

## scsettings.txt reference

`scsettings.txt` is the SC1000's settings file, and ScratchTJ reads the same format. Lines starting with `#` are comments.

Settings that also appear in the Config menu only give the **default** here. A value saved from the menu wins.

| Key | Default | Meaning |
|-----|---------|---------|
| `buffersize` | 256 | Audio buffer size in samples. Smaller means less delay but more risk of crackles. Takes effect on restart. |
| `platterspeed` | 9100 | Default for **Platter speed**. |
| `slippiness` | 200 | Default for **Slipmat**. |
| `brakespeed` | 3000 | Default for **Brake length**. |
| `jogreverse` | 1 | Default for **Reverse platter** (1 = on). |
| `faderopenpoint` | 5 | Default for **Open point**. |
| `faderclosepoint` | 3 | Default for **Close point**. |
| `cutbeats` | 0 | Default for **Cut beats**: 0 off, 1 left end, 2 right end. |
| `platterenabled` | 1 | 0 means there is no platter: the sample deck is always "held". Leave at 1. |
| `pitchrange` | 50 | Range, in %, of pitch-bend MIDI messages. |
| `mididelay` | 5 | Seconds to wait at startup before looking for MIDI devices. |
| `debouncetime` | 5 | GPIO button debounce, in input-loop cycles. |
| `holdtime` | 100 | GPIO button hold time before a "held" action fires, in input-loop cycles. |
| `gpio=` / `io=` | — | Maps a GPIO button to an action. See the comments in the file. |
| `midii=` | — | Maps a MIDI message to an action. See the comments in the file. |
| `samplerate`, `updaterate`, `disablevolumeadc` | — | Read for SC1000 compatibility, but have no effect on ScratchTJ. |

---

## config.cfg keys

`~/.scratchtj/config.cfg` uses these keys. You can edit the file by hand while the program is not running. Unknown keys are ignored, and values outside the allowed range are brought back inside it.

| Key | Menu name | Range |
|-----|-----------|-------|
| `platterspeed` | Platter > Platter speed | 1000 – 32768 |
| `jogreverse` | Platter > Reverse platter | 0 / 1 |
| `touch_threshold` | Platter > Touch threshold | 100 – 65000 |
| `motion_touch` | Platter > Touch by motion | 0 / 1 |
| `slippiness` | Platter > Slipmat | 1 – 3000 |
| `brakespeed` | Platter > Brake length | 1 – 10000 |
| `target_pitch` | Platter > Tracking gain | 1 – 240 |
| `pitch_filter` | Platter > Pitch response | 0.01 – 1 |
| `vel_ff` | Platter > Speed assist | 0 – 1 |
| `blipthreshold` | Platter > Glitch filter | 50 – 2048 |
| `fader_mode` | Fader & Volume > Fader mode | 0 SC1000 cut, 1 Curve |
| `cutbeats` | Fader & Volume > Cut beats | 0 / 1 / 2 |
| `faderopenpoint` | Fader & Volume > Open point | 1 – 200 |
| `faderclosepoint` | Fader & Volume > Close point | 0 – 199 |
| `Fad Switch` | Fader & Volume > Fader direction | 0 reversed, 1 normal |
| `Fad Factor` | Fader & Volume > Curve width | 0.01 – 10 |
| `Fad Power` | Fader & Volume > Curve shape | 0.01 – 10 |
| `fader_smooth` | Fader & Volume > Fader smoothing | 0 – 50 (ms) |
| `soft_clip` | Fader & Volume > Soft clip | 0 / 1 |
| `screen_off` | Screen > Screen off after | 0 – 60 (minutes, 0 = never) |

Older configs may also contain `fader_sharp` and `fhist_secs`. These keys are no longer used and are ignored.

---

## Recipes

**Play exactly like an SC1000.** Run Config > Reset Defaults. Everything ScratchTJ adds is then off.

**Tighter, more direct scratching.** Turn Speed assist up to 0.5–1.0. If it still feels loose, raise Tracking gain a little.

**The record stops by itself, or keeps playing under your hand.** Set Touch threshold using the live Sensor reading.

**The touch sensor is broken.** Turn on Touch by motion until it is fixed.

**The sample crackles when the fader rests at the edge.** Widen the gap between Open point and Close point, for example to 8 and 3.

**Get back the old ScratchTJ fader.** Set Fader mode to Curve. Alternatively, stay in SC1000 cut mode and set Cut beats to Left end: one end then cuts the beats and the other cuts the sample, with a hard cut instead of a curve.

**The track takes a long time to start after you let go.** Check that Touch by motion is Off, and that Pitch response and Slipmat are at their defaults (0.10 and 200).
