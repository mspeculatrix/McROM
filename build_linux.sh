#!/bin/zsh

# Script to build the STM32 firmware for the McROM and flash it to the MCU.
# To be used on the MX Linux machine.

# --- Configuration ---
ROM_BINARY="rom.bin"           # The source ROM file from your Mac
LINKER_SCRIPT="STM32H523xx_FLASH.ld"
BUILD_DIR="build_linux"
TARGET_NAME="McROM"
ABS_PATH=$(pwd)
LOGFILE="build_linux/build.log"

# Save original stdout to fd 3
exec 3>&1

# Enable logging: redirect stdout to tee, stderr to stdout
# Tee to screen and filtered log (no ANSI codes)
exec > >(tee >(sed -r "s/\x1B\[[0-9;]*[a-zA-Z]//g" > "$LOGFILE"))
exec 2>&1

# Redirect stderr to stdout (now teed)
exec 2>&1
echo "=================================="
echo "Building & Flashing McROM Firmware"
echo "=================================="
echo "Timestamp: $(date +%Y%m%d_%H%M%S)"
echo

echo "*** Checking build configuration ***"
# Create the build dir if it doesn't exist
if [[ ! -d "$BUILD_DIR" ]]; then
    echo "-- Creating $BUILD_DIR directory"
    mkdir "$BUILD_DIR"
    if [[ $? -eq 0 ]]; then
        echo "   -- created"
    else
        echo "!! Could not create $BUILD_DIR directory"
        exit 1
    fi
else 
    echo "-- $BUILD_DIR found"
fi

# Reconfigure to ensure paths and flags are correct
# Using the absolute path for the linker script
echo "*** Reconfiguring CMake environment ***"
cd "$BUILD_DIR"
cmake -G Ninja .. \
  -DCMAKE_C_COMPILER=arm-none-eabi-gcc \
  -DCMAKE_ASM_COMPILER=arm-none-eabi-gcc \
  -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
  -DCMAKE_C_FLAGS="-mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard" \
  -DCMAKE_EXE_LINKER_FLAGS="-T $ABS_PATH/$LINKER_SCRIPT -Wl,--gc-sections" \
  -DCMAKE_BUILD_TYPE=Debug

echo "*** Building with Ninja ***"
ninja

if [[ $? -eq 0 ]]; then
    echo "-- Build succeeded"
    echo "-- Renaming ELF file for compatibility with Programmer CLI"
    # Rename the output to .elf so the ST tool is happy
    mv "$TARGET_NAME" "$TARGET_NAME.elf"
    echo "-- ELF file info:"
    nm McROM.elf | grep _binary_rom_bin
    nm McROM.elf | grep ROM_Emulator_
    arm-none-eabi-size McROM.elf

    echo
    echo "*** Flashing to MCU with STM32_Programmer_CLI ***"
    echo
    STM32_Programmer_CLI -c port=swd mode=UR -w "$TARGET_NAME.elf" 0x08000000 -v -rst
else
    echo "!! Build failed. Check the errors above."
    exit 1
fi

# Restore original stdout (screen-only), close saved fd
exec >&3 3>&-

exit 0
