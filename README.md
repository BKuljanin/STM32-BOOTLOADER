# STM32 UART Bootloader

Custom UART bootloader for STM32F446RE (Nucleo board). Written in bare-metal C with register-level peripheral access, no HAL or CMSIS.

The bootloader occupies the first 16KB of flash (sector 0) and can receive new application firmware over UART from a Python script on the PC. This allows firmware updates without a debugger.

## How it works

The flash is split into two regions:

| Region | Address | Size | Contents |
|---|---|---|---|
| Bootloader | 0x08000000 | 16KB | Always stays, handles updates |
| Application | 0x08004000 | 496KB | User firmware, replaceable over UART |

On reset, the bootloader checks the blue button (PC13):
- **Button not pressed**: bootloader jumps to the application
- **Button held**: bootloader enters update mode and waits for firmware over UART

The jump to application works by reading the app's vector table (stack pointer + reset handler), relocating the vector table via SCB->VTOR, and branching to the app's Reset_Handler.

## UART update protocol

The Python flash tool (`tools/flash.py`) sends the `.bin` file to the bootloader using a simple packet protocol over UART at 115200 baud:

1. Sync handshake
2. Send firmware size (4 bytes, little endian)
3. Bootloader erases app flash sectors 1-7
4. Data sent in 128-byte packets, each with CRC32 verification
5. Final whole-image CRC32 check against flash contents

## Project structure

```
STM32-BOOTLOADER/
├── bootloader/
│   ├── source/
│   │   ├── main.c          Bootloader logic, UART, jump to app
│   │   ├── main.h          App start address define
│   │   ├── flash.c         Register-level flash erase/write driver
│   │   ├── flash.h
│   │   ├── stm32_startup.c Vector table and Reset_Handler
│   │   ├── stm32_ls.ld     Linker script (16KB at 0x08000000)
│   │   └── syscalls.c      Newlib stubs
│   ├── Makefile
│   └── setup_env.bat       Toolchain PATH setup
├── app/
│   ├── source/
│   │   ├── main.c          Demo app (UART print + LED blink)
│   │   ├── stm32_startup.c Vector table and Reset_Handler
│   │   ├── stm32_ls.ld     Linker script (496KB at 0x08004000)
│   │   └── syscalls.c      Newlib stubs
│   └── Makefile
└── tools/
    └── flash.py            Python script to send firmware over UART
```

## Building and flashing

Prerequisites: `arm-none-eabi-gcc`, `make`, `openocd` (all included in STM32CubeIDE)

Flash the bootloader (one time, needs ST-LINK debugger):
```
cd bootloader
setup_env.bat
make clean
make
make flash
```

Build the app:
```
cd app
make clean
make
```

Flash the app over UART (no debugger needed):
```
pip install pyserial
cd tools
python flash.py COM4 ../app/out/app.bin
```

Hold the blue button and press reset before running the flash script.

## Hardware

- Board: NUCLEO-F446RE (STM32F446RE, Cortex-M4F)
- UART2: PA2 (TX), PA3 (RX) routed to ST-LINK virtual COM port
- LED: PA5 (LD2)
- Button: PC13 (active low, triggers update mode)
- Clock: 16MHz HSI (no PLL)

## Key concepts

- Flash memory organization: sectors, erase before write, unlock sequence
- Custom linker scripts placing two independent binaries in separate flash regions
- Vector table relocation via SCB->VTOR
- Reliable UART transfer protocol with per-packet and whole-image CRC32
- Full system: embedded firmware + host-side Python tooling
