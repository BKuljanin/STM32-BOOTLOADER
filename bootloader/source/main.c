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
#define CR1_UE          (1U << 13)
#define SR_TXE          (1U << 7)

#define SYS_FREQ        16000000U
#define APB1_FREQ       SYS_FREQ
#define BAUD            115200U

#define BTN_PIN         13  // PC13 user button, active LOW

static void uart2_tx_init(void);
static void uart2_write(const char *str);
static void uart2_putchar(char ch);
static void led_init(void);
static void led_on(void);
static void led_off(void);
static void delay(volatile uint32_t count);
static void button_init(void);
static uint8_t button_pressed(void);
static void jump_to_app(void);

int main(void)
{
    uart2_tx_init();
    led_init();
    button_init();

    uart2_write("Bootloader started\r\n");

    // Short delay to allow button state to settle
    delay(100000);

    if (!button_pressed())
    {
        // No button held, jump to application
        uart2_write("Jumping to app...\r\n");
        delay(100000);  // Let UART finish transmitting
        jump_to_app();
    }

    // Button held, stay in bootloader (update mode)
    uart2_write("Update mode: waiting for firmware...\r\n");

    while (1)
    {
        led_on();
        delay(200000);
        led_off();
        delay(200000);
    }
}

// Jump to application at APP_START_ADDR
static void jump_to_app(void)
{
    // Read application vector table
    uint32_t app_stack = *(volatile uint32_t *)APP_START_ADDR;         // First word: initial SP
    uint32_t app_reset = *(volatile uint32_t *)(APP_START_ADDR + 4U);  // Second word: Reset_Handler

    // Basic sanity check: stack pointer should point to SRAM
    if ((app_stack & 0x2FF00000) != 0x20000000)
    {
        uart2_write("No valid app found\r\n");
        return;
    }

    // Relocate vector table to application
    SCB_VTOR = APP_START_ADDR;

    // Set stack pointer and jump
    __asm volatile (
        "MSR MSP, %0\n"   // Load app stack pointer
        "BX  %1\n"        // Branch to app Reset_Handler
        :
        : "r" (app_stack), "r" (app_reset)
    );
}

// PC13 = user button on Nucleo, input with external pull-up, active LOW
static void button_init(void)
{
    RCC_AHB1ENR |= GPIOCEN;
    // PC13 input mode (reset default, but explicit)
    GPIOC_MODER &= ~(3U << (BTN_PIN * 2));
}

static uint8_t button_pressed(void)
{
    return !(GPIOC_IDR & (1U << BTN_PIN));  // Active LOW: pressed = 0 on pin
}

// PA2 = USART2_TX (AF7), connected to ST-LINK virtual COM port
static void uart2_tx_init(void)
{
    RCC_AHB1ENR |= GPIOAEN;

    // PA2 alternate function mode
    GPIOA_MODER &= ~(1U << 4);
    GPIOA_MODER |= (1U << 5);

    // PA2 AF7 (USART2)
    GPIOA_AFR_LOW &= ~(0xFU << 8);
    GPIOA_AFR_LOW |= (7U << 8);

    // Enable USART2 clock
    RCC_APB1ENR |= USART2EN;

    // Baud rate
    USART2_BRR = (APB1_FREQ + (BAUD / 2U)) / BAUD;

    // Enable transmitter and USART
    USART2_CR1 = CR1_TE | CR1_UE;
}

static void uart2_putchar(char ch)
{
    while (!(USART2_SR & SR_TXE));
    USART2_DR = ch;
}

static void uart2_write(const char *str)
{
    while (*str)
    {
        uart2_putchar(*str++);
    }
}

// PA5 = LD2 on Nucleo F446RE
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
