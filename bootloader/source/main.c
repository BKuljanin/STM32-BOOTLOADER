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

#define USART2_BASE     0x40004400U
#define USART2_SR       (*(volatile uint32_t *)(USART2_BASE + 0x00U))
#define USART2_DR       (*(volatile uint32_t *)(USART2_BASE + 0x04U))
#define USART2_BRR      (*(volatile uint32_t *)(USART2_BASE + 0x08U))
#define USART2_CR1      (*(volatile uint32_t *)(USART2_BASE + 0x0CU))

#define GPIOAEN         (1U << 0)
#define USART2EN        (1U << 17)
#define CR1_TE          (1U << 3)
#define CR1_UE          (1U << 13)
#define SR_TXE          (1U << 7)

#define SYS_FREQ        16000000U   // HSI 16 MHz, no PLL for bootloader
#define APB1_FREQ       SYS_FREQ
#define BAUD            115200U

static void uart2_tx_init(void);
static void uart2_write(const char *str);
static void uart2_putchar(char ch);
static void led_init(void);
static void led_on(void);
static void led_off(void);
static void delay(volatile uint32_t count);

int main(void)
{
    uart2_tx_init();
    led_init();

    uart2_write("Bootloader started\r\n");

    while (1)
    {
        led_on();
        delay(800000);
        led_off();
        delay(800000);
    }
}

// PA2 = USART2_TX (AF7), connected to ST-LINK virtual COM port
static void uart2_tx_init(void)
{
    // Enable GPIOA clock
    RCC_AHB1ENR |= GPIOAEN;

    // PA2 alternate function mode
    GPIOA_MODER &= ~(1U << 4);
    GPIOA_MODER |= (1U << 5);

    // PA2 AF7 (USART2)
    GPIOA_AFR_LOW &= ~(0xFU << 8);
    GPIOA_AFR_LOW |= (7U << 8);

    // Enable USART2 clock
    RCC_APB1ENR |= USART2EN;

    // Baud rate: 16MHz / 115200 = 138.89 -> BRR = 0x008B (mantissa=8, fraction=11)
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
    // PA5 output mode
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
