#include "flash.h"

// Flash register base
#define FLASH_BASE      0x40023C00U
#define FLASH_KEYR      (*(volatile uint32_t *)(FLASH_BASE + 0x04U))
#define FLASH_SR        (*(volatile uint32_t *)(FLASH_BASE + 0x0CU))
#define FLASH_CR        (*(volatile uint32_t *)(FLASH_BASE + 0x10U))

// Flash keys
#define FLASH_KEY1      0x45670123U
#define FLASH_KEY2      0xCDEF89ABU

// Flash CR bits
#define FLASH_CR_PG     (1U << 0)
#define FLASH_CR_SER    (1U << 1)
#define FLASH_CR_STRT   (1U << 16)
#define FLASH_CR_LOCK   (1U << 31)
#define FLASH_CR_PSIZE_WORD (2U << 8)  // 32-bit parallelism

// Flash SR bits
#define FLASH_SR_BSY    (1U << 16)

// Sector number position in CR
#define FLASH_CR_SNB_POS 3

// flash is locked by default, two magic keys unlock it
void flash_unlock(void)
{
    if (FLASH_CR & FLASH_CR_LOCK)
    {
        FLASH_KEYR = FLASH_KEY1;
        FLASH_KEYR = FLASH_KEY2;
    }
}

void flash_lock(void)
{
    FLASH_CR |= FLASH_CR_LOCK;
}

static void flash_wait_busy(void)
{
    while (FLASH_SR & FLASH_SR_BSY);
}

// flash can only go 1 to 0, so we erase first (sets everything to 0xFF)
void flash_erase_sector(uint8_t sector)
{
    flash_wait_busy();

    FLASH_CR &= ~(0xFU << FLASH_CR_SNB_POS);
    FLASH_CR |= (sector << FLASH_CR_SNB_POS);
    FLASH_CR |= FLASH_CR_SER;
    FLASH_CR |= FLASH_CR_PSIZE_WORD;
    FLASH_CR |= FLASH_CR_STRT;

    flash_wait_busy();

    FLASH_CR &= ~FLASH_CR_SER;
}

// set PG bit, write the word, wait for flash controller to finish
void flash_write_word(uint32_t address, uint32_t data)
{
    flash_wait_busy();

    FLASH_CR |= FLASH_CR_PG;
    FLASH_CR |= FLASH_CR_PSIZE_WORD;

    // this is where the actual write happens
    *(volatile uint32_t *)address = data;

    flash_wait_busy();

    FLASH_CR &= ~FLASH_CR_PG;
}

// Erase sectors 1 through 7 (entire app region)
void flash_erase_app_sectors(void)
{
    for (uint8_t s = 1; s <= 7; s++)
    {
        flash_erase_sector(s);
    }
}
