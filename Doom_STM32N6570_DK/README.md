# Doom STM32N6570-DK (LRUN Implementation)

This project brings Doom to the STM32N6570-DK Discovery Kit.
For good performance on the Cortex-M55, this project uses the LRUN (Load and Run) mode.
The application code is stored in external Flash but is copied to and executed
from the internal AXISRAM at 600 MHz, bypassing the latency of XIP (Execute In
Place) - previously XIP was supported, but this is no longer used, but I might
have left some traces of XIP here and there.

System Specifications

 * MCU: STM32N657X0H3QU (Cortex-M55) @ 600 MHz
 * Execution Mode: LRUN (Code runs in internal SRAM for maximum speed)
 * Display: 800x480 (16-bit color, Double Buffering)
 * Memory Config: * 32 MB External PSRAM (XSPI1) used for Heap
 * Internal AXISRAM used for Code and Framebuffers

Once the application runs, the green LED (GPIO PO.01) toggles.

## Installation

Make sure you have the latest **STM32CubeCLT** installed.

 - [https://www.st.com/en/development-tools/stm32cubeclt.html](https://www.st.com/en/development-tools/stm32cubeclt.html)
 - Reboot your machine to update all the path environment variables

### Initial Hardware Setup (OTP Fuses)

To enable high-speed access to the external Flash, a specific OTP (One-Time Programmable) fuse must be set.

 * WARNING: OTP fuses are permanent and cannot be reset once written.
 * Set the board to Development Mode (`BOOT1` jumper on positions 1-3).
 * In your IDE project settings (Preprocessor), ensure the `NO_OTP_FUSE` symbol is disabled.
 * Flash and run the project once. This sets `VDDIO3_HSLV=1`.
 * For all subsequent builds, enable the `NO_OTP_FUSE` definition in the IDE to prevent further write attempts.


### Flashing Doom WAD Files

The (free shareware) game data (`wad/DOOM1.WAD`) must be flashed to a specific address in the external memory.

 * `DOOM1.WAD` file must be in the `/wad` directory.
 * Connect the board via SWD.
 * Run the `flash_wad.bat` script.
 * The script flashes the WAD file to 0x70300000.
 * It uses the external loader: `MX66UW1G45G_STM32N6570-DK.stldr`.

### Build and Deployment

The project consists of two parts: the First Stage Bootloader (FSBL) and the Main Application (Appli).

 * Build FSBL:
   * Select the FSBL project in your workspace and click Build.
   * The FSBL initializes the clocks and handles the LRUN copying process.

 * Build Application:
   * Select the Appli project and click Build.
   * This generates the Project-trusted.bin.

 * Debugging:
   * Start the Debugger via the FSBL project.
   * **IMPORTANT** Make sure no breakpoints in the Appli Project are active (the memory is only mapped during the FSBL boot process).
   * The debugger will load the application binary to the Flash address 0x70100000.

 * Flashing
    * Build the FSBL and Application projects in STM32CubeIDE
    * Run `flash_all.bat` to flash both the FSBL and the Application automatically.

## Memory Layout

    0x90000000 (XSPI1 PSRAM 32MB 200 MHz "Memory 2") 0x91FFFFFE 16 MB used for HEAP
    0x70300000 (XSPI2 NOR SFDP Flash "Memory 1") Doom WAD Game Data
    0x70000000 (XSPI2 NOR SFDP Flash "Memory 1") FSBL
    0x70100000 (XSPI2 NOR SFDP Flash "Memory 1") Application

    0x34350000 AXISRAM6 (enabled by FSBL, ideally for NPU) end address: 0x343C0000
    0x342E0000 AXISRAM5 (enabled by FSBL, ideally for NPU)
    0x34270000 AXISRAM4 (enabled by FSBL, ideally for NPU)
    0x34200000 AXISRAM3 (enabled by FSBL, ideally for NPU)
    0x34100000 AXISRAM2
    0x34064000 AXISRAM1
    0x34000000 FLEXRAM

FSBL RAM: `0x34180400`  LENGTH = 511K (if AppS code size reaches into this area - boom)

FLEXRAM to AXISRAM2 End:
`0x34000400` - `0x341FEFFF` (2092032 Bytes of RAM for LRUN code) (1KB (0x400) for signature at the beginning is unusable)
Non-cacheable area: `0x341FF000` - `0x341FFFFF`

Framebuffer 1: `0x34200000` to `0x342BB7FF` (AXISRAM3-4) (defined in `Appli/Inc/stm32n6570_discovery_conf.h`)
Framebuffer 2: `0x342BB800` to `0x34376FFF` (AXISRAM4-5) (defined in `Appli/Inc/stm32n6570_discovery_conf.h`)

    AXISRAM1 size:  624 KBytes (0x09C000 Bytes,   638.976 Bytes)
    AXISRAM2 size: 1024 KBytes (0x100000 Bytes, 1.048.576 Bytes)
    AXISRAM3 size:  448 KBytes (0x070000 Bytes,   458.752 Bytes)
    AXISRAM4 size:  448 KBytes (0x070000 Bytes,   458.752 Bytes)
    AXISRAM5 size:  448 KBytes (0x070000 Bytes,   458.752 Bytes)
    AXISRAM6 size:  448 KBytes (0x070000 Bytes,   458.752 Bytes)
    FLEXRAM  size:  400 KBytes (0x064000 Bytes,   409.600 Bytes)

16-Bit Framebuffer size: `800*480 * sizeof(uint16_t) = 768.000` (0xBB800)

## Debugger

Background information on the debugger setup:

https://community.st.com/t5/stm32-mcus/how-to-execute-code-from-the-external-psram-using-the-stm32n6/ta-p/772691
