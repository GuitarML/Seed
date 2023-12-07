# Seed

Seed is a digital multi-effects pedal with neural modeling at
its’ heart. It features Reverb, Delay, and Tremolo to further
modify your sound. A five minute reversible looper rounds out
this pedalboard-in-a-box for practice, performance, and
musical experimentation. This pedal is built using the [Terrarium](https://www.pedalpcb.com/product/pcb351/) platform
from PedalPCB, so if you have one you can try out Seed without building
new hardware! If you want a pre-drilled and UV printed enclosure for the complete Seed pedal, use the UV print pdf 
along with the Tayda drill template included here to order one from Tayda.

Check out the [YouTube Demo!](https://www.youtube.com/watch?v=G6pMcl99Tug)

![app](https://github.com/GuitarML/Seed/blob/main/docs/seed_pic.jpg)

Neural Modeling is a technique that uses neural networks to
replicate the sound of real analog devices. Seed uses this
technology to recreate three classic amplifiers and one overdrive pedal (and maybe a few easter eggs!).
All models were trained using the "ns-capture" branch of the [Automated-GuitarAmpModelling]([https://github.com/GuitarML/NeuralSeed](https://github.com/GuitarML/Automated-GuitarAmpModelling/tree/ns-capture)) code.

- 9v DC Power (Center Negative)
- Input / Output - 1/4" Mono

Read the [Documentation](https://github.com/GuitarML/Seed/blob/main/docs/seed_doc_scaled%2090.pdf) for a rundown of all the controls.

If you already have a Terrarium pedal built, head to the [Releases](), download the .bin file, flash to the Daisy Seed and start rockin!


## Build Your Own Seed Pedal

The Seed pedal consists of the following main components:

 - Daisy Seed (order from [Electro-Smith](https://www.electro-smith.com/)) - $30
 - Terrarium PCB (order from [PedalPCB](https://www.pedalpcb.com/product/pcb351/) - $12
 - 125B Enclosure ([Powder coat](https://www.taydaelectronics.com/cream-125b-style-aluminum-diecast-enclosure.html), [Custom Drill](https://www.taydaelectronics.com/125b-custom-drill-enclosure-service.html), and [UV print](https://www.taydaelectronics.com/125b-uv-printing-service.html) ordered from [Tayda](https://www.taydaelectronics.com/)) - $15
 - Electronic Components for Terrarium (see [build docs](https://docs.pedalpcb.com/project/Terrarium.pdf)) (Tayda, Mouser, Amazon, etc.)
 - Hardware Components for Terrarium (1/4" Mono Audio Jacks, Power Jack, Knobs, Switches) (I like [LoveMySwitches](https://lovemyswitches.com/))
 - "Seed" Software - Build your own binary from this repo or download .bin from the [Releases page]()

You can build your own complete Seed pedal by using the Tayda Drill Template
and UV print pdf file. Follow the instructions on the Tayda website to do this. 
Custom drill service and UV printing can be purchased on the Tayda website.
The original Seed pedal uses the "Cream" color powder coat, but feel free to experiment,
the all-black UV print should show up well on light colors.

IMPORTANT: If you use the Drill Template, double check that the hole diameters match your components. Especially for the LED's, which use the larger diameter hole for 5mm [LED Bezel](https://lovemyswitches.com/5mm-chrome-metal-led-bezel-bag-of-5/).

## Build the Software
Head to the [Electro-Smith Wiki](https://github.com/electro-smith/DaisyWiki) to learn how to set up the Daisy environment on your computer.

```
# Clone the repository
$ git clone https://github.com/GuitarML/Seed.git
$ cd Seed

# initialize and set up submodules
$ git submodule update --init --recursive

# Build the daisy libraries with (after installing the Daisy Toolchain):
$ make -C DaisySP
$ make -C libDaisy

# Build the Seed binary (after installing the Daisy Toolchain)
$ cd src
$ make
```
As shown above, navigate to the Seed/src folder, and run ```make``` (after installing the Daisy Toolchain and building libDaisy and DaisySP libraries) to build the Seed.bin.

Then flash your Seed pedal with the following commands (or use the [Electrosmith Web Programmer](https://electro-smith.github.io/Programmer/))
```
cd your_pedal
# using USB (after entering bootloader mode)
make program-dfu
# using JTAG/SWD adaptor (like STLink)
make program
```

## Note from the Author

The Seed pedal is my first attempt at a fully featured digital effects pedal comparable to other commercial effects. 
I build and sell them for around $200 from the [GuitarML Pedals facebook page](https://www.facebook.com/profile.php?id=100089301889206) (reach out to me here and I may have a couple avaiable).
Although selling these pedals is rewarding, it is much more fulfilling to me to spend my time creating new designs, 
and open sourcing them so the DIY community to build and enjoy these for themselves. I love encouraging people to get their hands
dirty with science and engineering, and guitar effects are a terrific way to get involved in so many diciplines - electronics,
software, mechanical, product design, art, and of course music. No matter what your interests are, I hope this project sparks your 
curiosity and encourages you to create new and amazing things.

Keep on rockin!

-Keith 
