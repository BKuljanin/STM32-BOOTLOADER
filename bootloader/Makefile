# Toolchain
CC=arm-none-eabi-gcc
MACH=cortex-m4
CFLAGS= -c -mcpu=$(MACH) -mthumb -mfloat-abi=soft -std=gnu11 -Wall -O0 -g
LDFLAGS= -mcpu=$(MACH) -mthumb -mfloat-abi=soft --specs=nano.specs -T source/stm32_ls.ld -Wl,-Map=out/bootloader.map

# Collect all .c files from source/ and generate .o paths
SRCS = $(wildcard source/*.c)
OBJS = $(patsubst source/%.c, out/%.o, $(SRCS))

all: $(OBJS) out/bootloader.elf

# Pattern rule for compiling source/*.c to out/*.o
out/%.o: source/%.c
	mkdir -p out
	$(CC) $(CFLAGS) -o $@ $^

out/bootloader.elf: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

clean:
	rm -rf out/

# OpenOCD flashing
OPENOCD=C:/ST/STM32CubeIDE_1.19.0/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.openocd.win32_2.4.200.202505051030/tools/bin/openocd.exe
OPENOCD_SCRIPTS=C:/ST/STM32CubeIDE_1.19.0/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.debug.openocd_2.3.100.202501240831/resources/openocd/st_scripts

flash:
	"$(OPENOCD)" -s openocd -s "$(OPENOCD_SCRIPTS)" -f interface/stlink-dap.cfg -f target/stm32f4x.cfg -c "program out/bootloader.elf verify reset exit"

debug:
	"$(OPENOCD)" -s openocd -s "$(OPENOCD_SCRIPTS)" -f interface/stlink-dap.cfg -f target/stm32f4x.cfg
