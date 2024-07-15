# RP2040 Retro Sound

## What is?
RP2040 Retro Sound is a project that enables the Raspberry Pi Pico (RP2040) to serve as a COM port passthrough for various FM retro sound chips like the SAA1099, YM2413, SN76489, YM3812, and YMF262. It achieves this with the help of two 74HC595 shift registers.

![PCB prototype](https://github.com/user-attachments/assets/85f5cebe-c401-4fa2-a182-d5b8ab359145)


With this setup, you can bring the authentic sound of classic gaming and computing systems to life using original sound chips. Enjoy the nostalgia of retro audio in a modern, convenient package!

## Purpose
The goal of this project is to allow enthusiasts to listen to retro sound with real sound chips without needing any other retro hardware.

## How to use
To use this project, you will need a modified version of DosBox-X(to be provided later) or a Sega Master System/Sega Genesis emulator that can passthrough sound data directly to the chips via a COM port.

The operation protocol is straightforward and consists of two-byte transfers. The first byte is the command byte:

```
/*
 * 76543210 - command byte
 * ||||||||
 * |||||||+-- chip 0 / chip 1
 * ||||||+--- address or data
 * ||||++---- reserved
 * ++++------ 0x0...0xE chip id:
 *
 *            0x0 - SN76489 (Tandy 3 voice)![photo_2024-07-05_19-19-13](https://github.com/user-attachments/assets/35203a20-3679-40de-9a69-3cdcec56f17c)

 *            0x1 - YM2413 (OPLL)
 *            0x2 - YM3812 (OPL2)
 *            0x3 - SAA1099 (Creative Music System / GameBlaster)
 *            0x4 - YMF262 (OPL3)
 *            0x5 - YM2612, YM2203, YM2608, YM2610, YMF288 (OPN2 and OPN-compatible series)
 *            ...
 *            0xf - all chips reset/initialization
 */
```

The second byte is the data byte, which is routed directly to the chip.

## Requirements
1. PCB (to be provided later)
2. RP2040: The microcontroller that powers the project.
3. 2x 74HC595 Shift Registers: These are used to interface with the sound chips.
4. Retro Sound Chips: Choose from SN76489, SAA1099, YM2413, YM3812, YMF262, or any OPN2 compatible chip. The YMF288 is recommended for its versatility. 
5. Capacitors and Resistors: A few passive components for stable operation.
6. Basic Soldering Skills: Minimal soldering is required to assemble the hardware.
