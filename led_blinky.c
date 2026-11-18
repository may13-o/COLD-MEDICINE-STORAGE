/*
 * FRDM-MCXC444 bring-up self-test
 * Replaces the blinky main(). Needs no external hardware.
 *
 * Tests:
 *   1. Debug console over OpenSDA virtual COM (115200 8N1)
 *   2. Core clock report
 *   3. Chip unique ID read
 *   4. SysTick 1 ms tick
 *   5. SRAM pattern test (1 KB buffer)
 *   6. On-board RGB LED sequence, then green heartbeat
 *
 * Result: PASS -> green heartbeat, FAIL -> red blinking fast.
 */

#include <stdint.h>
#include "board.h"
#include "clock_config.h"
#include "pin_mux.h"
#include "fsl_common.h"
#include "fsl_debug_console.h"

#define RAM_TEST_WORDS 256U

static volatile uint32_t g_ms = 0;
static volatile uint32_t g_ram[RAM_TEST_WORDS];

void SysTick_Handler(void)
{
    g_ms++;
}

static void delay_ms(uint32_t ms)
{
    uint32_t start = g_ms;
    while ((g_ms - start) < ms)
    {
        __WFI();
    }
}

static void all_leds_off(void)
{
    LED_RED_OFF();
    LED_GREEN_OFF();
    LED_BLUE_OFF();
}

/* Returns number of failing words */
static uint32_t ram_test(void)
{
    uint32_t errors = 0;
    const uint32_t patterns[] = {0xAAAAAAAAU, 0x55555555U, 0x00000000U, 0xFFFFFFFFU};

    /* fixed patterns */
    for (uint32_t p = 0; p < (sizeof(patterns) / sizeof(patterns[0])); p++)
    {
        for (uint32_t i = 0; i < RAM_TEST_WORDS; i++) g_ram[i] = patterns[p];
        for (uint32_t i = 0; i < RAM_TEST_WORDS; i++)
        {
            if (g_ram[i] != patterns[p]) errors++;
        }
    }

    /* walking ones */
    for (uint32_t bit = 0; bit < 32U; bit++)
    {
        uint32_t v = 1UL << bit;
        for (uint32_t i = 0; i < RAM_TEST_WORDS; i++) g_ram[i] = v;
        for (uint32_t i = 0; i < RAM_TEST_WORDS; i++)
        {
            if (g_ram[i] != v) errors++;
        }
    }

    /* address-in-data (catches stuck/shorted address lines) */
    for (uint32_t i = 0; i < RAM_TEST_WORDS; i++) g_ram[i] = (i * 0x01010101U) ^ 0xA5A5A5A5U;
    for (uint32_t i = 0; i < RAM_TEST_WORDS; i++)
    {
        if (g_ram[i] != ((i * 0x01010101U) ^ 0xA5A5A5A5U)) errors++;
    }

    return errors;
}

int main(void)
{
    /* ---- Board init: keep whichever pair your blinky project used ---- */
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitDebugConsole();
    /* If your project uses a single call instead, replace the 3 lines above with:
     *     BOARD_InitHardware();
     */

    LED_RED_INIT(LOGIC_LED_OFF);
    LED_GREEN_INIT(LOGIC_LED_OFF);
    LED_BLUE_INIT(LOGIC_LED_OFF);

    SystemCoreClockUpdate();
    SysTick_Config(SystemCoreClock / 1000U); /* 1 ms tick */

    PRINTF("\r\n==============================\r\n");
    PRINTF(" FRDM-MCXC444 self-test\r\n");
    PRINTF("==============================\r\n");

    /* 1. Console works if you can read this */
    PRINTF("[1] Console        : OK\r\n");

    /* 2. Clock */
    PRINTF("[2] Core clock     : %u Hz\r\n", (unsigned int)SystemCoreClock);

    /* 3. Unique ID */
    PRINTF("[3] Unique ID      : %08X-%08X-%08X\r\n",
           (unsigned int)SIM->UIDMH, (unsigned int)SIM->UIDML, (unsigned int)SIM->UIDL);

    /* 4. SysTick */
    uint32_t t0 = g_ms;
    delay_ms(100);
    uint32_t dt = g_ms - t0;
    PRINTF("[4] SysTick 100ms  : measured %u ms -> %s\r\n", (unsigned int)dt,
           (dt >= 99U && dt <= 101U) ? "OK" : "CHECK");

    /* 5. RAM */
    uint32_t ram_err = ram_test();
    PRINTF("[5] SRAM test      : %u errors -> %s\r\n", (unsigned int)ram_err,
           (ram_err == 0U) ? "PASS" : "FAIL");

    /* 6. RGB LED sequence */
    PRINTF("[6] RGB LED        : watch R -> G -> B\r\n");
    all_leds_off();
    LED_RED_ON();   delay_ms(400); LED_RED_OFF();
    LED_GREEN_ON(); delay_ms(400); LED_GREEN_OFF();
    LED_BLUE_ON();  delay_ms(400); LED_BLUE_OFF();

    if (ram_err != 0U)
    {
        PRINTF("\r\nRESULT: FAIL\r\n");
        while (1)
        {
            LED_RED_TOGGLE();
            delay_ms(100);
        }
    }

    PRINTF("\r\nRESULT: PASS  (green heartbeat, counter prints every second)\r\n");

    uint32_t beat = 0;
    while (1)
    {
        LED_GREEN_ON();  delay_ms(100);
        LED_GREEN_OFF(); delay_ms(900);
        PRINTF("heartbeat %u  uptime %u ms\r\n", (unsigned int)beat++, (unsigned int)g_ms);
    }
}