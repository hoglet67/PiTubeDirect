# PiTubeDirect
PiTubeDirect is a low cost Second Processor project for Acorn's 8-bit machines (Beeb, Master, Electron, Atom)
which uses two cheap chips to interface a Raspberry Pi to the Tube connector.

The Pi emulates one of a number of CPUs, and also the Tube interface chip.

A Pi Zero can emulate a 6502 Second Processor running at 274MHz.

For more information, see the project [Wiki](../../wiki).

Here's the minimal configuration, using a Pi Zero and a DIY two-chip level shifter: 
![minimal configuration](https://raw.githubusercontent.com/wiki/hoglet67/PiTubeDirect/images/intro1.jpg)

Benchmark results for the 65C02 Second Processor:
![benchmark results](https://raw.githubusercontent.com/wiki/hoglet67/PiTubeDirect/images/intro4.jpg)
