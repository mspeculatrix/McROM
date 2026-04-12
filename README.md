# McROM

STM32-based ROM emulator. Based on the STM32H523CET6 running at 250MHz from its internal oscillator.

The board matches the pinout of the 28-pin DIP AT28C256 EEPROM.

Differences between the McROM and the EEPROM:

- A14 (chip pin 1) is not connected on the McROM. It is connected on the EEPROM in order to select the upper 16KB of the EEPROM's 32KB memory space.
- /WE (chip pin 27) is not connected on the McROM. On the Zolatron's CPU board, this pin is pulled high by a resistor.

## PIN ASSIGNMENTS

**Address bus**

```
 A0  ->  PB10
 A1  ->  PB2
 A2  ->  PB1
 A3  ->  PB0
 A4  ->  PA7
 A5  ->  PA6
 A6  ->  PA3
 A7  ->  PA2
 A8  ->  PC14
 A9  ->  PC13
A10  ->  PB4
A11  ->  PB7
A12  ->  PA1
A13  ->  PC15
```

**Data bus**

```
D0  <-  PB13
D1  <-  PB14
D2  <-  PB15
D3  <-  PA8
D4  <-  PA9
D5  <-  PA10
D6  <-  PA11
D7  <-  PA12
```

**Control pins**

```
READ_EN (/OE)    -  PB6
ROM_ENABLE (/CE) - PA15
```

## Build script for Linux

The `build_linux.sh` script is designed to be run on my MX Linux machine which is connected to the McROM board via an ST-LINK V2 programmer/debugger. It builds into the `build_linux` directory.

Building the code on the Mac (to check if it compiles) builds into the `build` directory, but this won't make its way to the McROM board.

## COMMAND LINE

### Clean

```
cmake --build build/Debug --target clean
```

### Build

```
cmake --build build/Debug
```

### Flash

The STM32_Programmer_CLI is at the following locations:

- MX: ~/stm32clt/STM32CubeProgrammer/bin/STM32_Programmer_CLI
- M1: /opt/ST/STM32CubeCLT_1.19.0/STM32CubeProgrammer/bin/STM32_Programmer_CLI

```
STM32_Programmer_CLI -c port=swd mode=UR -w build/Debug/McROM.elf 0x08000000 -v -rst
```

## Branches

- `**main**` - Code that is working to some significant extent.
- `**dev**` - Work in progress. Probably not working.
- `**vA1**` - This is a version that mostly works on the A1 version of the McROM board. It often takes several attempts to get a reset to work, but otherwise it looks good. It's a snapshot of the code on 12/04/2026.
