
# ScratchTJ: Tinkering a DIY Digital Turntable with Raspberry Pi 

Hey there! Welcome to **ScratchTJ**, a project born out of curiosity and a passion for music and tinkering. This is all about turning a Raspberry Pi into a DIY digital turntable that lets you scratch like a pro DJ, using affordable and repurposed hardware. Let's dive into how this all came together.

## Table of Contents

- [The Spark of Inspiration](#the-spark-of-inspiration)
- [Discovering the SC1000 and The_Rasteri's Work](#discovering-the-sc1000-and-the_rasteris-work)
- [Adapting Code for Raspberry Pi 2](#adapting-code-for-raspberry-pi-2)
- [Getting Xwax Up and Running](#getting-xwax-up-and-running)
- [Adding an LCD Menu System](#adding-an-lcd-menu-system)
- [Upgrading with Hardware Controls](#upgrading-with-hardware-controls)
- [Implementing Capacitive Touch](#implementing-capacitive-touch)
- [3D Printing the Custom Parts](#3d-printing-the-custom-parts)
- [Features and Capabilities](#features-and-capabilities)
- [Project Resources](#project-resources)
- [Wrapping Up](#wrapping-up)

## The Spark of Inspiration
![Soldering Station](https://github.com/no3z/ScratchTJ/raw/main/docs/sc500_teal_transp_3.jpg)


It all started when I stumbled upon [this awesome video](https://youtu.be/Llxfi6l2I-U). Watching someone turn a simple setup into a functioning DJ turntable was mind-blowing. That got me thinking: could I build something similar using a Raspberry Pi? I grew up with a love for music and gadgets, so this seemed like the perfect project to combine both.

## Discovering the SC1000 and The_Rasteri's Work

Digging deeper, I found out about the **SC1000**, a DIY turntable controller designed by **the_rasteri**. The designs were open-source, but getting my hands on the hardware was tricky. If you're interested in buying one, check out the [SC1000 MK2 here](https://portablismgear.com/sc1000/devices/sc1000mk2.html). It’s honestly a fantastic piece of gear if you can get your hands on it. However, I decided to take matters into my own hands and adapt the available resources to what I had lying around—enter the Raspberry Pi!

![Workbench Setup](https://github.com/no3z/ScratchTJ/raw/main/docs/initial_build_setup.jpg)

## Adapting Code for Raspberry Pi 2

With hardware in hand, the next step was figuring out the software. The SC1000 was running some open-source code, but to make it work with a **Raspberry Pi 2** and an **AudioInjector audio hat**, I had to do some deep diving. The open-source code wasn't exactly plug-and-play for my setup. With lots of googling, trial and error, and a fair bit of coffee, I managed to adapt the original code to be compatible with my hardware. To be honest, some parts of the code are still kind of a mess, but it's functional, and that's what counts, right? Plus, I had a lot of help from AI like large language models to navigate the trickier bits.


![Soldering Station](https://github.com/no3z/ScratchTJ/raw/main/docs/soldering_station_setup.jpg)

## Getting SC1000 - Xwax Up and Running

Once the Pi was ready, it was time to tackle the software. The heart of this setup is Xwax, an open-source digital vinyl emulation software that provides realistic scratching capabilities. However, instead of starting from scratch, much of the work was based on the SC1000 GitHub repository, which is itself built upon Xwax. This repository offered a solid foundation tailored for DIY digital turntables. Adapting the SC1000 code to work with the Raspberry Pi and the AudioInjector wasn't straightforward. It required compiling from source, tweaking dependencies, and ensuring compatibility with the hardware. After some head-scratching moments, everything finally came together, marking a satisfying milestone in the project.

## Adding an LCD Menu System

I didn’t want this project to require a keyboard or monitor every time I needed to tweak something. So, I added a **1602A LCD display**. The goal was to create an easy-to-use menu system that would let me adjust settings on the fly. I learned from the SC1000 Xwax adaptation and created a custom menu system for my setup. It was controlled by a small rotary encoder connected to the Pi. Now, I could change settings, switch between tracks, and do it all with just a knob—simple and effective.

## Upgrading with Hardware Controls

After the initial testing, I decided it needed more tactile, physical controls—something to make it feel more like DJ equipment. I ordered a big **optical rotary encoder** from AliExpress and a budget-friendly **DJ fader**. Of course, simply plugging these into the Pi wouldn’t do. I needed an **Arduino** to interface everything properly. The Arduino took on the task of reading these inputs and sending them to the Raspberry Pi. This made the fader and the encoder super responsive and fun to use.

![Final Enclosure](https://github.com/no3z/ScratchTJ/raw/main/docs/enclosure_and_controls.jpg)

![Testing the Assembly](https://github.com/no3z/ScratchTJ/raw/main/docs/testing_assembly.jpg)
## Implementing Capacitive Touch

The turning point came when I realized I needed capacitive touch on the platter, just like a real DJ turntable. That’s when things got really DIY. I repurposed an old **hard drive platter** as my spinning disk—it already looked kind of like a vinyl record, so why not? I connected it to the encoder and added a simple capacitive touch sensor using a 1MΩ resistor. Now, whenever I touched the platter, it acted just like a vinyl turntable—pausing, slowing, and spinning the audio as I moved it.
![Optical Encoder and HDD Platter](https://github.com/no3z/ScratchTJ/raw/main/docs/optical_encoder_and_platter.jpg)
## 3D Printing the Custom Parts

To bring everything together, I also had to dive into the world of **3D printing**. Here’s what I printed:

- **Platter Adapter**: This was used to attach the HDD platter to the rotary encoder shaft. Initially, I just used a CD for testing before upgrading to the HDD platter.
- **Enclosure**: I designed a simple case to house all the components and keep everything neat and tidy. Without it, the wires were going everywhere, and the setup was just a mess.

You can find the models here:

- HDD Adapter: [TinkerCAD Model](https://www.tinkercad.com/things/61eF1Ijn7o5-hdd-adapter?sharecode=nWsXvmSv_DllBxx8CcdytpptvyzZCUKnqFkQ3bDZFho)
- Enclosure: [TinkerCAD Model](https://www.tinkercad.com/things/2LCXX7xvP9b-tinkerscratchv0?sharecode=RHVKMN4xlvb5UvUtA5s9apYJHQghMAneHwZlXMxaT3Y)

These parts made a huge difference in getting everything to look and feel more polished, rather than a spaghetti mess of wires.

![Resistor Array](https://github.com/no3z/ScratchTJ/raw/main/docs/resistor_array.jpg)
## Features and Capabilities

The ScratchTJ setup offers quite a few cool features:

- **Live Scratching**: With the HDD platter and optical encoder, I can do real scratching. It's not perfect, but it's pretty damn fun.
- **Recording Samples**: The system lets me record my scratch sessions on the fly, which is useful for saving cool sounds and coming back to them later.
- **Adjustable Fader Settings**: I can tweak crossfader curves and responsiveness right from the menu.
- **Platter Speed Control**: I can adjust playback speed, which adds a ton of versatility.
- **LCD Menu Navigation**: With the rotary encoder, navigating through different settings feels quite intuitive.
- **Capacitive Touch Detection**: Just like a real DJ setup, I can control the platter by simply touching it.

These features really brought everything together, making it more than just a tech project, but an actual instrument you can jam with.

## Project Resources

- **GitHub Repository**: Check out the code and instructions on [GitHub](https://github.com/no3z/ScratchTJ/tree/main). A quick warning: the code isn’t the cleanest, and some parts are kind of messy because, well, this whole thing is a mishmash of a lot of trial and error (and a lot of help from large language models like ChatGPT).
- **3D Models**:
  - HDD Adapter: [Link](https://www.tinkercad.com/things/61eF1Ijn7o5-hdd-adapter?sharecode=nWsXvmSv_DllBxx8CcdytpptvyzZCUKnqFkQ3bDZFho)
  - Enclosure: [Link](https://www.tinkercad.com/things/2LCXX7xvP9b-tinkerscratchv0?sharecode=RHVKMN4xlvb5UvUtA5s9apYJHQghMAneHwZlXMxaT3Y)

Feel free to explore the repository and models to build your own version, or maybe even improve upon it.

## Wrapping Up

This project has been a fantastic journey into combining hardware and software to create something fun and functional. From watching a YouTube video to building a DIY digital turntable, it's been all about learning and experimenting. 

If you're interested in making your own ScratchTJ or have ideas to improve it, I'd love to hear from you. The project isn’t perfect, and there are still so many ways to make it better, but that’s the beauty of open-source projects: it’s all about iteration and collaboration.

Kiitos paljon ja happy scratching!

 



---
