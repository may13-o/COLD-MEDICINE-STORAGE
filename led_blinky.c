/*
 * FRDM-MCXC444 - OLED (SSD1306 0.96") bring-up test
 *
 * Wiring (mikroBUS socket, see board pinout):
 *   OLED SCL -> PTC1      OLED SDA -> PTC2
 *   OLED VCC -> 3V3 (NOT 5V0)      OLED GND -> GND
 *
 * Stage 1: scans I2C 0x08..0x77 and prints every address that ACKs.
 * Stage 2: if 0x3C or 0x3D answers, initialises the SSD1306 and cycles
 *          full white / black / stripes once per second.
 *
 * ASSUMPTION TO VERIFY: PTC1/PTC2 are I2C1_SCL/I2C1_SDA on mux ALT2 (as on other
 * Kinetis L parts). If the scan finds nothing, open the Pins tool, click PTC1/PTC2
 * and confirm which I2C instance/ALT they offer, then change the three defines below.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "board.h"
#include "clock_config.h"
#include "pin_mux.h"
#include "fsl_common.h"
#include "fsl_clock.h"
#include "fsl_port.h"
#include "fsl_i2c.h"
#include "fsl_debug_console.h"

/* Defined in this project's hardware_init.c (pins, clocks, debug console). */
void BOARD_InitHardware(void);

/* ---- change these three if the Pins tool shows a different instance/ALT ---- */
#define OLED_I2C_BASE      I2C1
#define OLED_PIN_MUX       kPORT_MuxAlt2
#define OLED_I2C_BAUD      100000U

/* ---- board init: use the same call(s) your working blinky used ---- */
#define USE_BOARD_INIT_HARDWARE 1

static void delay_ms(uint32_t ms)
{
    SDK_DelayAtLeastUs(ms * 1000U, SystemCoreClock);
}

static void i2c_pins_init(void)
{
    port_pin_config_t cfg = {0};
    cfg.pullSelect = kPORT_PullUp;      /* helps if the module has weak/no pull-ups */
    cfg.mux        = OLED_PIN_MUX;

    CLOCK_EnableClock(kCLOCK_PortC);
    PORT_SetPinConfig(PORTC, 1U, &cfg); /* PTC1 = SCL */
    PORT_SetPinConfig(PORTC, 2U, &cfg); /* PTC2 = SDA */
}

static void i2c_bus_init(void)
{
    i2c_master_config_t m;
    I2C_MasterGetDefaultConfig(&m);
    m.baudRate_Bps = OLED_I2C_BAUD;
    I2C_MasterInit(OLED_I2C_BASE, &m, CLOCK_GetBusClkFreq());
}

/* Same as I2C_MasterStop() but gives up instead of waiting forever if the bus never idles. */
static void i2c_stop_timeout(void)
{
    uint32_t guard = 20000U;
    OLED_I2C_BASE->C1 &= (uint8_t)~(I2C_C1_MST_MASK | I2C_C1_TX_MASK | I2C_C1_TXAK_MASK);
    while (((OLED_I2C_BASE->S & kI2C_BusBusyFlag) != 0U) && (guard > 0U))
    {
        guard--;
    }
}

/* Address-only probe: true if a device ACKs its 7-bit address. */
static bool i2c_probe(uint8_t addr7)
{
    uint32_t guard = 100000U;

    if (I2C_MasterStart(OLED_I2C_BASE, addr7, kI2C_Write) != kStatus_Success)
    {
        i2c_stop_timeout();
        return false;
    }
    while (((OLED_I2C_BASE->S & kI2C_TransferCompleteFlag) == 0U) && (guard-- > 0U))
    {
    }
    bool nak = ((OLED_I2C_BASE->S & kI2C_ReceiveNakFlag) != 0U) || (guard == 0U);
    OLED_I2C_BASE->S = kI2C_IntPendingFlag; /* clear flag (write 1 to clear) */
    i2c_stop_timeout();
    delay_ms(1);
    return !nak;
}

/* ctrl: 0x00 = command bytes follow, 0x40 = display data follows */
static bool oled_write(uint8_t addr7, uint8_t ctrl, const uint8_t *data, size_t len)
{
    i2c_master_transfer_t x = {0};
    x.flags          = kI2C_TransferDefaultFlag;
    x.slaveAddress   = addr7;
    x.direction      = kI2C_Write;
    x.subaddress     = ctrl;
    x.subaddressSize = 1U;
    x.data           = (uint8_t *)data;
    x.dataSize       = len;
    return I2C_MasterTransferBlocking(OLED_I2C_BASE, &x) == kStatus_Success;
}

static bool oled_cmd(uint8_t addr7, uint8_t c)
{
    return oled_write(addr7, 0x00U, &c, 1U);
}

static bool oled_init(uint8_t a)
{
    static const uint8_t seq[] = {
        0xAE,             /* display off */
        0xD5, 0x80,       /* clock divide */
        0xA8, 0x3F,       /* multiplex 64 */
        0xD3, 0x00,       /* display offset */
        0x40,             /* start line 0 */
        0x8D, 0x14,       /* charge pump on */
        0x20, 0x00,       /* horizontal addressing */
        0xA1,             /* segment remap */
        0xC8,             /* COM scan direction */
        0xDA, 0x12,       /* COM pins */
        0x81, 0xCF,       /* contrast */
        0xD9, 0xF1,       /* pre-charge */
        0xDB, 0x40,       /* VCOM detect */
        0xA4,             /* resume from RAM */
        0xA6,             /* normal (not inverted) */
        0xAF              /* display on */
    };
    for (size_t i = 0; i < sizeof(seq); i++)
    {
        if (!oled_cmd(a, seq[i])) return false;
    }
    return true;
}

static bool oled_fill(uint8_t a, uint8_t pattern)
{
    uint8_t buf[16];
    for (size_t i = 0; i < sizeof(buf); i++) buf[i] = pattern;

    if (!oled_cmd(a, 0x21) || !oled_cmd(a, 0) || !oled_cmd(a, 127)) return false; /* columns */
    if (!oled_cmd(a, 0x22) || !oled_cmd(a, 0) || !oled_cmd(a, 7))   return false; /* pages   */

    for (uint32_t n = 0; n < (128U * 8U) / sizeof(buf); n++)
    {
        if (!oled_write(a, 0x40U, buf, sizeof(buf))) return false;
    }
    return true;
}

int main(void)
{
#if USE_BOARD_INIT_HARDWARE
    BOARD_InitHardware();
#else
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitDebugConsole();
#endif

    i2c_pins_init();
    i2c_bus_init();

    PRINTF("\r\n=== OLED bring-up test (I2C: SCL=PTC1, SDA=PTC2) ===\r\n");

    for (;;)
    {
        uint8_t oled = 0;
        uint32_t found = 0;

        PRINTF("Scanning I2C...\r\n");
        for (uint8_t a = 0x08; a <= 0x77; a++)
        {
            if (i2c_probe(a))
            {
                PRINTF("  device at 0x%02X\r\n", a);
                found++;
                if ((a == 0x3C) || (a == 0x3D)) oled = a;
            }
        }
        PRINTF("Scan done: %u device(s)\r\n", (unsigned int)found);

        if (oled == 0)
        {
            PRINTF("OLED NOT FOUND. Check: VCC on 3V3, GND, SCL->PTC1, SDA->PTC2, jumper contacts,\r\n"
                   "and Pins-tool I2C instance/ALT for PTC1/PTC2.\r\n\r\n");
            delay_ms(2000);
            continue;
        }

        PRINTF("OLED at 0x%02X, initialising...\r\n", oled);
        if (!oled_init(oled))
        {
            PRINTF("OLED init failed (command write NACK)\r\n");
            delay_ms(2000);
            continue;
        }

        PRINTF("Cycling white / black / stripes. Watch the screen.\r\n");
        for (;;)
        {
            if (!oled_fill(oled, 0xFF))
            {
                break;
            }
            PRINTF("white\r\n");
            delay_ms(1000);

            if (!oled_fill(oled, 0x00))
            {
                break;
            }
            PRINTF("black\r\n");
            delay_ms(1000);

            if (!oled_fill(oled, 0xAA))
            {
                break;
            }
            PRINTF("stripes\r\n");
            delay_ms(1000);
        }
        PRINTF("Display write failed, rescanning\r\n");
    }
}
