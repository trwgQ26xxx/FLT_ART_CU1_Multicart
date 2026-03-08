#! /bin/bash


# Either the new Microchip avr-gcc toolchain can be used (it includes built-in support for ATtiny806),
# or the stock avr-gcc bundled with Ubuntu, which requires the ATtiny806 DFP.

# Since the newer toolchain produces smaller code, the script uses it by default.
# The DFP patch is kept for optional backward compatibility.
BIN_PATCH=AVR_GCC/avr8-gnu-toolchain-4.0.0.52-linux.any.x86_64/avr8-gnu-toolchain-linux_x86_64/bin
DFP_PATCH=AVR_GCC/Microchip.ATtiny_DFP.3.3.272

clear

echo "Cleaning output files..."
rm multicart.elf
rm multicart.hex

echo "Compiling..."

"$BIN_PATCH/avr-gcc" -mmcu=attiny806 \
-B "$DFP_PATCH/gcc/dev/attiny806/" \
-I "$DFP_PATCH/include/" \
-O2 \
-DF_CPU=1000000L \
-Wall -Wextra -Wpedantic \
-flto -ffunction-sections -fdata-sections -Wl,--gc-sections \
-o multicart.elf multicart.c

"$BIN_PATCH/avr-size" -B -d multicart.elf

echo "Writing output files..."

"$BIN_PATCH/avr-objcopy" multicart.elf -O ihex multicart.hex

echo "Done!"
