
  

# ScratchTJ: Tinkering a DIY Digital Turntable with Raspberry Pi 

The journey of creating a DIY digital turntable began with a spark of inspiration from two YouTube videos: [SC500 DIY CDJ build](https://www.youtube.com/watch?v=j9CJ7EI0yY4) and [SC1000 Open Source Turntable](https://youtu.be/Llxfi6l2I-U). If you're interested in buying one, check out the [SC1000 MK2 here](https://portablismgear.com/sc1000/devices/sc1000mk2.html). It’s honestly a fantastic piece of gear if you can get your hands on it. The innovative work by **the_rasteri** on these models captivated me, and the idea of building a custom digital turntable took root. 
The goal was clear: adapt [the open-source code of the SC1000](https://github.com/rasteri/SC1000) to work on a Raspberry Pi 2 equipped with an AudioInjector audio hat, and create a functional, customizable digital turntable using readily available components. Let's dive into how this all came together. 

But first: one demo video of the thing running: (Click to watch)

[![Watch the video](https://img.youtube.com/vi/rufXcn8hjYE/maxresdefault.jpg)](https://youtu.be/rufXcn8hjYE)

## The Spark of Inspiration
![Soldering Station](https://github.com/no3z/ScratchTJ/raw/main/docs/sc500_teal_transp_3.jpg)


## The Journey Begins
### Exploring the SC1000 Code 
Discovering that the SC500 units were scarce and difficult to purchase, I turned my attention to the [SC1000's open-source code](https://github.com/rasteri/SC1000). Diving into the codebase, I sought ways to initialize a 1602 LCD display and integrate it with [xwax](http://www.xwax.co.uk/), an open-source digital vinyl emulation software.
### Initial Hardware Tests
 The first step was to get the LCD screen operational and establish communication with the Raspberry Pi. I experimented with the 1602A LCD display, ensuring it could display text and respond to inputs. Concurrently, I began working with a rotary encoder to navigate menus and control parameters.
 
![Soldering Station](https://github.com/no3z/ScratchTJ/raw/main/docs/soldering_station_setup.jpg)
### Integrating the Rotary Encoder
 With some assistance from GPT-powered coding suggestions, I developed C code to handle rotary encoder events. This allowed for start/stop control of the decks and adjustment of various parameters displayed on the LCD screen.

## Expanding the Hardware
 ### Acquiring New Components
To enhance the functionality, I sourced a magnetoelectric rotary encoder and a DJ fader from AliExpress. I also upgraded the LCD display to a 1602A with an integrated circuit (IC) to reduce the GPIO pin usage on the Raspberry Pi.

![Final Enclosure](https://github.com/no3z/ScratchTJ/raw/main/docs/enclosure_and_controls.jpg)
![Testing the Assembly](https://github.com/no3z/ScratchTJ/raw/main/docs/testing_assembly.jpg)
### Introducing the Arduino Nano
 Realizing the need for additional processing capabilities to handle the fader and the rotary encoder, I incorporated an Arduino Nano into the setup. The Arduino reads the input from these components and communicates with the Raspberry Pi via serial connection (TX/RX).

### Adding Capacitive Touch
 To emulate the feel of a real turntable, I wanted to incorporate capacitive touch sensitivity. Initial attempts using electric paint on a CD proved inadequate. The solution was to use an open hard drive (HDD) platter connected to the magnetoelectric rotary encoder. The platter, connected to the Arduino Nano acting as a capacitive sensor, provided the desired touch sensitivity.


![Workbench Setup](https://github.com/no3z/ScratchTJ/raw/main/docs/initial_build_setup.jpg)
![Optical Encoder and HDD Platter](https://github.com/no3z/ScratchTJ/raw/main/docs/optical_encoder_and_platter.jpg)

## Bill of Materials (BOM)
 - **Raspberry Pi 2** 
 - **AudioInjector Sound Card** 
 - **1602A LCD Display with IC** 
 -  **Rotary Encoder with Push Button** 
 -  **Arduino Nano** (connected via TX/RX serial)
 -  **1200Ω Resistor** 
 -  **DJ Fader** 
 -  **Magnetoelectric Rotary Encoder** 
 -  **Hard Drive Platter**

## 3D Printing and Enclosure Design

![Resistor Array](https://github.com/no3z/ScratchTJ/raw/main/docs/IMG_3892.jpg)
![Resistor Array](https://github.com/no3z/ScratchTJ/raw/main/docs/IMG_3893.jpg)
### HDD Adapter and Enclosure
 To house the components and provide a user-friendly interface, I designed custom parts in Tinkercad: 
 - **HDD Adapter**: [Tinkercad Model](https://www.tinkercad.com/things/61eF1Ijn7o5-hdd-adapter)  The HDD adapter features channels for a 1.5mm wire to connect the platter to the shaft, enabling touch sensitivity.
 -  **Enclosure**: [Tinkercad Model](https://www.tinkercad.com/things/2LCXX7xvP9b-tinkerscratchv0) The enclosure was designed to accommodate all components, though initial prints required adjustments to fit the Raspberry Pi properly.


## Software Development
 ### Customizing the SC1000 Code 
 Analyzing the SC1000 source code, I identified areas to integrate the new hardware components. Modifications included: 
 - Initializing the 1602A LCD display. 
 - Handling input from the Arduino Nano for the fader and rotary encoder. 
 -  Implementing capacitive touch functionality for the platter. 
 -  Extending menu options to adjust fader settings, platter speed, pitch factor, and more in real-time. 
![Watch the video](https://img.youtube.com/vi/jpz3jol8UZQ/maxresdefault.jpg)](https://youtu.be/jpz3jol8UZQ)
(Click to watch)
### Additional Features 
The system was expanded to: 
- Control ALSA mixer parameters for the sound card. 
-  Enable recording from the sound card inputs. 
-  Provide adjustable fader settings (cut curve, cut position switch).
[![Watch the video](https://img.youtube.com/vi/uF4GSIXVzZU/maxresdefault.jpg)](https://youtu.be/uF4GSIXVzZU)
(Click to watch)

## Wrapping Up

If you're interested in making your own ScratchTJ or have ideas to improve it, I'd love to hear from you. The project isn’t perfect, and there are still so many ways to make it better, but that’s the beauty of open-source projects: it’s all about iteration and collaboration.

Kiitos paljon ja happy scratching!

---
