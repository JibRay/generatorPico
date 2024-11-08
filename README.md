# generatorPico

## Description
This processor is the interface to the hardware. It senses temperature and
humidity via an SHT30 device connected via I2C. Generator output voltage
is sampled via the built-in ADC. These data are sent to a Zero W via the
UART.

## Setup
1. Create project source files in the this directory.
2. Create CMakeLists.txt file in this directory.
3. Create build directory (add 'build' to .gitignore).
4. cd to build directory.
5. Type `cmake ..`
6. Type `make -j8`
7. Hold BOOTSEL button on the pico board while plugging it in to the USB
 port.
8. Type `cp <project-name>.uf2 /media/jib/RPI-RP2`

## References
[Digikey](Digikey.com/en/maker/projects/...)
[Other](wellys.com/posts/rp2040_c_linus)
