/*
 * STM32 UART Bootloader
 *
 * Build and Flash (CMD):
 *   cd C:\Users\Bogdan Kuljanin\Documents\Git\STM32-BOOTLOADER\bootloader
 *   setup_env.bat
 *   make clean
 *   make
 *   make flash
 *
 *   cd ..\app
 *   make clean
 *   make
 *   make flash
 */

#include <stdint.h>
#include "main.h"
#include "flash.h"

// Register definitions
#define RCC_BASE        0x40023800U
#define RCC_AHB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x30U))
#define RCC_APB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x40U))

#define GPIOA_BASE      0x40020000U
#define GPIOA_MODER     (*(volatile uint32_t *)(GPIOA_BASE + 0x00U))
#define GPIOA_ODR       (*(volatile uint32_t *)(GPIOA_BASE + 0x14U))
#define GPIOA_AFR_LOW   (*(volatile uint32_t *)(GPIOA_BASE + 0x20U))

#define GPIOC_BASE      0x40020800U
#define GPIOC_MODER     (*(volatile uint32_t *)(GPIOC_BASE + 0x00U))
#define GPIOC_IDR       (*(volatile uint32_t *)(GPIOC_BASE + 0x10U))

#define USART2_BASE     0x40004400U
#define USART2_SR       (*(volatile uint32_t *)(USART2_BASE + 0x00U))
#define USART2_DR       (*(volatile uint32_t *)(USART2_BASE + 0x04U))
#define USART2_BRR      (*(volatile uint32_t *)(USART2_BASE + 0x08U))
#define USART2_CR1      (*(volatile uint32_t *)(USART2_BASE + 0x0CU))

#define SCB_VTOR        (*(volatile uint32_t *)0xE000ED08U)

#define GPIOAEN         (1U << 0)
#define GPIOCEN         (1U << 2)
#define USART2EN        (1U << 17)
#define CR1_TE          (1U << 3)
#define CR1_RE          (1U << 2)
#define CR1_UE          (1U << 13)
#define SR_TXE          (1U << 7)
#define SR_RXNE         (1U << 5)

#define SYS_FREQ        16000000U
#define APB1_FREQ       SYS_FREQ
#define BAUD            115200U

#define BTN_PIN         13

// Protocol commands
#define CMD_SYNC        0xA5U
#define CMD_DATA        0xB6U
#define CMD_DONE        0xC7U
#define RSP_ACK         0x06U
#define RSP_NACK        0x15U

#define PACKET_SIZE     128U

static void uart2_init(void);
static void uart2_write(const char *str);
static void uart2_putchar(char ch);
static uint8_t uart2_getchar(void);
static void uart2_read(uint8_t *buf, uint32_t len);
static void led_init(void);
static void led_on(void);
static void led_off(void);
static void delay(volatile uint32_t count);
static void button_init(void);
static uint8_t button_pressed(void);
static void jump_to_app(void);
static void receive_firmware(void);
static uint32_t crc32(const uint8_t *data, uint32_t len);

int main(void)
{
    uart2_init();
    led_init();
    button_init();

    uart2_write("Bootloader started\r\n");

    delay(100000);

    if (!button_pressed())
    {
        uart2_write("Jumping to app...\r\n");
        delay(100000);
        jump_to_app();
    }

    // Update mode
    uart2_write("Update mode: waiting for firmware...\r\n");
    receive_firmware();

    // After successful update, jump to new app
    uart2_write("Update complete, launching app...\r\n");
    delay(100000);
    jump_to_app();

    // Should never reach here
    while (1);
}

/*
 * Firmware receive protocol:
 *   1. Wait for CMD_SYNC from host
 *   2. Reply ACK
 *   3. Receive 4 bytes: firmware size (little endian)
 *   4. Reply ACK
 *   5. Erase app flash sectors
 *   6. Reply ACK (erase done)
 *   7. Loop: receive CMD_DATA + 128 bytes + 4 byte CRC
 *      - Verify CRC, write to flash, reply ACK (or NACK on bad CRC)
 *   8. Receive CMD_DONE + 4 byte total CRC
 *      - Verify against written flash, reply ACK/NACK
 */
static void receive_firmware(void)
{
    uint8_t cmd;
    uint8_t buf[PACKET_SIZE];
    uint32_t fw_size;
    uint32_t bytes_written = 0;
    uint32_t write_addr = APP_START_ADDR;

    // Wait for sync
    while (1)
    {
        cmd = uart2_getchar();
        if (cmd == CMD_SYNC)
            break;

        // Blink LED while waiting
        led_on();
        delay(200000);
        led_off();
        delay(200000);
    }
    uart2_putchar(RSP_ACK);

    // Receive firmware size (4 bytes, little endian)
    uint8_t size_buf[4];
    uart2_read(size_buf, 4);
    fw_size = (uint32_t)size_buf[0]
            | ((uint32_t)size_buf[1] << 8)
            | ((uint32_t)size_buf[2] << 16)
            | ((uint32_t)size_buf[3] << 24);

    uart2_putchar(RSP_ACK);

    // Erase app sectors
    uart2_write("Erasing flash...\r\n");
    flash_unlock();
    flash_erase_app_sectors();
    uart2_putchar(RSP_ACK);
    uart2_write("Erase done\r\n");

    // Receive data packets
    while (bytes_written < fw_size)
    {
        cmd = uart2_getchar();
        if (cmd != CMD_DATA)
        {
            uart2_putchar(RSP_NACK);
            continue;
        }

        // How many bytes in this packet
        uint32_t chunk = fw_size - bytes_written;
        if (chunk > PACKET_SIZE)
            chunk = PACKET_SIZE;

        // Receive data
        uart2_read(buf, chunk);

        // Receive packet CRC (4 bytes)
        uint8_t crc_buf[4];
        uart2_read(crc_buf, 4);
        uint32_t expected_crc = (uint32_t)crc_buf[0]
                              | ((uint32_t)crc_buf[1] << 8)
                              | ((uint32_t)crc_buf[2] << 16)
                              | ((uint32_t)crc_buf[3] << 24);

        // Verify packet CRC
        uint32_t actual_crc = crc32(buf, chunk);
        if (actual_crc != expected_crc)
        {
            uart2_putchar(RSP_NACK);
            continue;
        }

        // Write to flash (word aligned)
        // Pad last chunk to word boundary if needed
        uint32_t words = (chunk + 3U) / 4U;
        uint32_t *src = (uint32_t *)buf;
        for (uint32_t i = 0; i < words; i++)
        {
            flash_write_word(write_addr, src[i]);
            write_addr += 4U;
        }

        bytes_written += chunk;
        led_on();
        uart2_putchar(RSP_ACK);
    }

    // Receive CMD_DONE + total CRC
    cmd = uart2_getchar();
    if (cmd == CMD_DONE)
    {
        uint8_t crc_buf[4];
        uart2_read(crc_buf, 4);
        uint32_t expected_crc = (uint32_t)crc_buf[0]
                              | ((uint32_t)crc_buf[1] << 8)
                              | ((uint32_t)crc_buf[2] << 16)
                              | ((uint32_t)crc_buf[3] << 24);

        // Verify entire flash content
        uint32_t actual_crc = crc32((const uint8_t *)APP_START_ADDR, fw_size);
        if (actual_crc == expected_crc)
        {
            uart2_putchar(RSP_ACK);
            uart2_write("CRC OK\r\n");
        }
        else
        {
            uart2_putchar(RSP_NACK);
            uart2_write("CRC FAIL\r\n");
            while (1);  // Halt on failure
        }
    }

    flash_lock();
    led_off();
}

// CRC32 (same polynomial as standard CRC32: 0xEDB88320 reflected)
static uint32_t crc32(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFU;
    for (uint32_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++)
        {
            if (crc & 1U)
                crc = (crc >> 1) ^ 0xEDB88320U;
            else
                crc >>= 1;
        }
    }
    return crc ^ 0xFFFFFFFFU;
}

static void jump_to_app(void)
{
    uint32_t app_stack = *(volatile uint32_t *)APP_START_ADDR;
    uint32_t app_reset = *(volatile uint32_t *)(APP_START_ADDR + 4U);

    if ((app_stack & 0x2FF00000) != 0x20000000)
    {
        uart2_write("No valid app found\r\n");
        return;
    }

    SCB_VTOR = APP_START_ADDR;

    __asm volatile (
        "MSR MSP, %0\n"
        "BX  %1\n"
        :
        : "r" (app_stack), "r" (app_reset)
    );
}

static void button_init(void)
{
    RCC_AHB1ENR |= GPIOCEN;
    GPIOC_MODER &= ~(3U << (BTN_PIN * 2));
}

static uint8_t button_pressed(void)
{
    return !(GPIOC_IDR & (1U << BTN_PIN));
}

// PA2 = TX (AF7), PA3 = RX (AF7)
static void uart2_init(void)
{
    RCC_AHB1ENR |= GPIOAEN;

    // PA2 alternate function mode (TX)
    GPIOA_MODER &= ~(3U << 4);
    GPIOA_MODER |= (2U << 4);

    // PA3 alternate function mode (RX)
    GPIOA_MODER &= ~(3U << 6);
    GPIOA_MODER |= (2U << 6);

    // PA2 AF7, PA3 AF7
    GPIOA_AFR_LOW &= ~(0xFFU << 8);
    GPIOA_AFR_LOW |= (0x77U << 8);

    RCC_APB1ENR |= USART2EN;

    USART2_BRR = (APB1_FREQ + (BAUD / 2U)) / BAUD;

    // Enable TX, RX, and USART
    USART2_CR1 = CR1_TE | CR1_RE | CR1_UE;
}

static void uart2_putchar(char ch)
{
    while (!(USART2_SR & SR_TXE));
    USART2_DR = ch;
}

static uint8_t uart2_getchar(void)
{
    while (!(USART2_SR & SR_RXNE));
    return (uint8_t)USART2_DR;
}

static void uart2_read(uint8_t *buf, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++)
    {
        buf[i] = uart2_getchar();
    }
}

static void uart2_write(const char *str)
{
    while (*str)
    {
        uart2_putchar(*str++);
    }
}

static void led_init(void)
{
    RCC_AHB1ENR |= GPIOAEN;
    GPIOA_MODER |= (1U << 10);
    GPIOA_MODER &= ~(1U << 11);
}

static void led_on(void)
{
    GPIOA_ODR |= (1U << 5);
}

static void led_off(void)
{
    GPIOA_ODR &= ~(1U << 5);
}

static void delay(volatile uint32_t count)
{
    while (count--);
}
