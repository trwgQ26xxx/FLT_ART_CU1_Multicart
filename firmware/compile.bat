@echo off
echo Cleaning...
rm cart*

echo Compiling...
sdcc -mmcs51 --std-c23 multicart.c -o cart.ihx

echo Converting to hex...
packihx cart.ihx > cart.hex

echo Done!