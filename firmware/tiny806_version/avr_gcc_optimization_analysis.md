This analysis explores the optimization settings for stock Ubuntu's AVR-GCC toolchain version 7.3.0 (including DFP) and compares them to the current version of the Microchip AVR-GCC toolchain version 15.1.0 (AVR_8_bit_GNU_Toolchain_4.0.0_52). The focus is on understanding the differences in output size and efficiency across various optimization levels.

| Optimization Level | New Toolchain (gcc version 15.1.0) | Old Toolchain (gcc version 7.3.0) |
|---|---|---|
| -O0 | 1048 bytes | 1084 bytes |
| -O1 | 592 bytes | 632 bytes |
| -O2 | 650 bytes | 676 bytes |
| -O3 | 756 bytes | 784 bytes |
| -Os | 604 bytes | 660 bytes |
