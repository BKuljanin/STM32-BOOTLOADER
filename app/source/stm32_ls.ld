ENTRY(Reset_Handler)

/* Application starts at sector 1, after the 16KB bootloader */
MEMORY
{
  FLASH(rx):ORIGIN =0x08004000,LENGTH =496K
  SRAM(rwx):ORIGIN =0x20000000,LENGTH =128K
}

SECTIONS
{
  .text :
  {
    *(.isr_vector)
    *(.text)
    *(.rodata)
	. = ALIGN(4);
	_etext = .;
  }> FLASH

  _la_data = LOADADDR(.data);

  .data :
  {
	_sdata = .;
	*(.data)
	. = ALIGN(4);
	_edata = .;
  }> SRAM AT> FLASH

  .bss :
  {
	_sbss = .;
	__bss_start__ = .;
	*(.bss)
	*(COMMON)
	. = ALIGN(4);
	_ebss = .;
	__bss_end__ = .;
  }> SRAM

  PROVIDE(end = _ebss);
}
