# generatorPico

## Description
This processor is the interface to the hardware. Generator output voltage
and battery voltage are sampled via the built-in ADC channels. From a ten
cycle sample of the generator output waveform, the RMS voltage and
frequency are calculated every 10 seconds. These data are sent to a Zero W
via the UART. See generatorZero.git for Zero W code.

## Setup
1. Create project source files in the this directory.
2. Create CMakeLists.txt file in this directory.
3. Create build directory (add 'build' to .gitignore).

## Build and Programming
1. cd to build directory.
2. Type `cmake ..`
3. Type `make -j8`
4. Hold BOOTSEL button on the pico board while plugging it in to the USB
 port.
5. Type `cp generatorPico.uf2 /media/jib/RPI-RP2`

## References
[Digikey](Digikey.com/en/maker/projects/...)
[Other](wellys.com/posts/rp2040_c_linux)
