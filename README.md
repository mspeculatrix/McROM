# McROM

STM32-based ROM emulator

Pin assignments

Address bus
A0   ->  PB10
A1   ->  PB2
A2   ->  PB1
A3   ->  PB0
A4   ->  PA7
A5   ->  PA6
A6   ->  PA3
A7   ->  PA2
A8   ->  PC14
A9   ->  PC13
A10  ->  PB4
A11  ->  PB7
A12  ->  PA1
A13  ->  PC15

Data bus
D0  <-  PB13
D1  <-  PB14
D2  <-  PB15
D3  <-  PA8
D4  <-  PA9
D5  <-  PA10
D6  <-  PA11
D7  <-  PA12

Control pins
READ_EN (/OE)    -  PB6
ROM_ENABLE (/CE) - PA15

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
