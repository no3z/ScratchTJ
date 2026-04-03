# ScratchTJ MK2 — DIY Digital Scratch Turntable

ScratchTJ MK2 is the **new, improved** build of my standalone scratch deck on Raspberry Pi + xwax.
It is designed for real scratching performance with a touch platter, crossfader, cue buttons, and an on-device menu.

> **Project status:** this repository tracks **MK2**.
> The previous generation is preserved in Git tag **`mk1`**.

---

## Video Demos (Embedded)

### 1) Randomize Function Scratch
[![Randomize function scratch](https://img.youtube.com/vi/6zyM5B6j0_E/hqdefault.jpg)](https://youtu.be/6zyM5B6j0_E?si=wSV1pAbMLMZF-k79)

### 2) Scratch Demo (First Video)
[![Scratch demo first video](https://img.youtube.com/vi/LH9r2fsUk-c/hqdefault.jpg)](https://youtu.be/LH9r2fsUk-c?si=Gcfz0AI6pA22WaxF)

### 3) Menu Showing (Short)
[![Menu showing short](https://img.youtube.com/vi/j2CSrozANF8/hqdefault.jpg)](https://youtube.com/shorts/j2CSrozANF8?si=jhMayBKkclHSwKNa)

### 4) Little Scratch Demo (Short)
[![Little scratch demo](https://img.youtube.com/vi/L9gylG18938/hqdefault.jpg)](https://youtube.com/shorts/L9gylG18938?si=5t2caiyyweBquCjl)

---

## Latest MK2 Build Photos

Only recent MK2 build photos are shown below.

![MK2 assembled top view](docs/assembled_top_view.jpg)
![MK2 assembled front view](docs/assembled_front_view.jpg)
![MK2 assembled back view](docs/assembled_back_view.jpg)
![MK2 workbench overview](docs/workbench_overview.jpg)

---

## Why MK2 is Better than MK1

- Higher precision platter sensing (MT6701 magnetic angle sensor)
- Better menu/display workflow for live adjustments
- Improved enclosure and internal layout
- Cleaner modular hardware + easier maintenance
- Better integration of controller features and performance behavior

---

## Core Features

- Touch-sensitive platter for real scratch gestures
- Adjustable crossfader behavior (cut-in / curve style controls in software)
- On-device menu and runtime settings
- Cue and deck controls with physical buttons
- Built on proven xwax playback/scratch engine

---

## Quick Build + Run

```bash
cd ~/ScratchTJ/software
make -j4
sudo LC_ALL=en_GB.utf8 nice -n -19 ./xwax
```

If locale is not set as above, xwax may fail to start correctly.

---

## Documentation

- Build process and wiring notes: [`docs/BUILD_GUIDE.md`](docs/BUILD_GUIDE.md)
- Installation details: [`docs/INSTALLATION.md`](docs/INSTALLATION.md)
- Software architecture: [`docs/SOFTWARE.md`](docs/SOFTWARE.md)
- Enclosure design files: [`docs/enclosure/ENCLOSURE_DESIGN.md`](docs/enclosure/ENCLOSURE_DESIGN.md)

---

## Sharing / Community

Once your build is ready, share photos/videos and your settings:

- Reddit post (build log + scratch clip)
- Discord showcase channel (demo + hardware notes)

If you publish, include that it is **ScratchTJ MK2** and link this repo + mention that MK1 is archived in tag `mk1`.
