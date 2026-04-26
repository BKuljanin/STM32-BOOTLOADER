#ifndef FLASH_H_
#define FLASH_H_

#include <stdint.h>

void flash_unlock(void);
void flash_lock(void);
void flash_erase_sector(uint8_t sector);
void flash_write_word(uint32_t address, uint32_t data);
void flash_erase_app_sectors(void);

#endif
