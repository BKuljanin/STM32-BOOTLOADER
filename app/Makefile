# Toolchain
CC=arm-none-eabi-gcc
OBJCOPY=arm-none-eabi-objcopy
MACH=cortex-m4
CFLAGS= -c -mcpu=$(MACH) -mthumb -mfloat-abi=soft -std=gnu11 -Wall -O0 -g
LDFLAGS= -mcpu=$(MACH) -mthumb -mfloat-abi=soft --specs=nano.specs -T source/stm32_ls.ld -Wl,-Map=out/app.map

# Collect all .c files from source/ and generate .o paths
SRCS = $(wildcard source/*.c)
OBJS = $(patsubst source/%.c, out/%.o, $(SRCS))

all: $(OBJS) out/app.elf out/app.bin

# Pattern rule for compiling source/*.c to out/*.o
out/%.o: source/%.c
	mkdir -p out
	$(CC) $(CFLAGS) -o $@ $^

out/app.elf: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

out/app.bin: out/app.elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -rf out/

# OpenOCD flashing (flashes app to 0x08004000 without erasing bootloader)
OPENOCD=C:/ST/STM32CubeIDE_1.19.0/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.openocd.win32_2.4.200.202505051030/tools/bin/openocd.exe
OPENOCD_SCRIPTS=C:/ST/STM32CubeIDE_1.19.0/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.debug.openocd_2.3.100.202501240831/resources/openocd/st_scripts

flash:
	"$(OPENOCD)" -s openocd -s "$(OPENOCD_SCRIPTS)" -f interface/stlink-dap.cfg -f target/stm32f4x.cfg -c "program out/app.elf verify reset exit"
