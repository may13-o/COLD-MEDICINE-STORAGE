
/*
 * Project: External OLED Display Test
 * Board:   NXP FRDM-MCXC444
 * Display: SSD1306 0.96-inch OLED
 * Size:    128 x 64
 * Bus:     I2C1
 * Address: 0x3C
 *
 * mikroBUS connections:
 * OLED VCC -> 3.3V
 * OLED GND -> GND
 * OLED SCL -> PTC1
 * OLED SDA -> PTC2
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

void BOARD_InitHardware(void);

/* OLED configuration */
#define OLED_I2C             I2C1
#define OLED_ADDRESS         0x3CU
#define OLED_WIDTH           128U
#define OLED_HEIGHT          64U

#define OLED_SCL_PIN         1U
#define OLED_SDA_PIN         2U

/* -------------------------------------------------
 * Configure I2C pins
 * ------------------------------------------------- */
static void OLED_PinsInit(void)
{
    port_pin_config_t config = {0};

    CLOCK_EnableClock(kCLOCK_PortC);

    config.pullSelect = kPORT_PullUp;
    config.mux = kPORT_MuxAlt2;

    PORT_SetPinConfig(PORTC, OLED_SCL_PIN, &config);
    PORT_SetPinConfig(PORTC, OLED_SDA_PIN, &config);
}

/* -------------------------------------------------
 * Initialise I2C peripheral
 * ------------------------------------------------- */
static void OLED_I2CInit(void)
{
    i2c_master_config_t config;

    I2C_MasterGetDefaultConfig(&config);

    config.baudRate_Bps = 100000U;

    I2C_MasterInit(
        OLED_I2C,
        &config,
        CLOCK_GetBusClkFreq()
    );
}

/* -------------------------------------------------
 * Send command/data to OLED
 *
 * Control byte:
 * 0x00 = command
 * 0x40 = display data
 * ------------------------------------------------- */
static bool OLED_Write(uint8_t control,
                       const uint8_t *data,
                       size_t length)
{
    i2c_master_transfer_t transfer = {0};

    transfer.flags = kI2C_TransferDefaultFlag;
    transfer.slaveAddress = OLED_ADDRESS;
    transfer.direction = kI2C_Write;

    transfer.subaddress = control;
    transfer.subaddressSize = 1U;

    transfer.data = (uint8_t *)data;
    transfer.dataSize = length;

    status_t status = I2C_MasterTransferBlocking(
        OLED_I2C,
        &transfer
    );

    return (status == kStatus_Success);
}

/* -------------------------------------------------
 * Send multiple OLED commands in one transaction
 * ------------------------------------------------- */
static bool OLED_SendCommands(const uint8_t *commands,
                              size_t length)
{
    return OLED_Write(0x00U, commands, length);
}

/* -------------------------------------------------
 * SSD1306 initialisation
 * ------------------------------------------------- */
static bool OLED_Init(void)
{
    static const uint8_t initCommands[] =
    {
        0xAE,             /* Display OFF */

        0xD5, 0x80,       /* Display clock */
        0xA8, 0x3F,       /* Multiplex ratio */
        0xD3, 0x00,       /* Display offset */
        0x40,             /* Start line */

        0x8D, 0x14,       /* Charge pump */

        0x20, 0x00,       /* Horizontal addressing */

        0xA1,             /* Segment remap */
        0xC8,             /* COM scan direction */

        0xDA, 0x12,       /* COM pins */

        0x81, 0xCF,       /* Contrast */

        0xD9, 0xF1,       /* Pre-charge */
        0xDB, 0x40,       /* VCOMH */

        0xA4,             /* Normal display */
        0xA6,             /* Non-inverted display */

        0x2E,             /* Disable scrolling */

        0xAF              /* Display ON */
    };

    return OLED_SendCommands(
        initCommands,
        sizeof(initCommands)
    );
}

/* -------------------------------------------------
 * Set display memory window
 * ------------------------------------------------- */
static bool OLED_SetWindow(void)
{
    static const uint8_t windowCommands[] =
    {
        0x21, 0x00, 0x7F,
        0x22, 0x00, 0x07
    };

    return OLED_SendCommands(
        windowCommands,
        sizeof(windowCommands)
    );
}

/* -------------------------------------------------
 * Fill complete OLED display
 * ------------------------------------------------- */
static bool OLED_Fill(uint8_t pattern)
{
    uint8_t buffer[16];

    for (uint32_t i = 0; i < sizeof(buffer); i++)
    {
        buffer[i] = pattern;
    }

    if (!OLED_SetWindow())
    {
        return false;
    }

    for (uint32_t i = 0;
         i < (OLED_WIDTH * OLED_HEIGHT / 8U) / sizeof(buffer);
         i++)
    {
        if (!OLED_Write(0x40U, buffer, sizeof(buffer)))
        {
            return false;
        }
    }

    return true;
}

/* -------------------------------------------------
 * Main application
 * ------------------------------------------------- */
int main(void)
{
    BOARD_InitHardware();

    PRINTF("\r\n");
    PRINTF("================================\r\n");
    PRINTF(" FRDM-MCXC444 OLED APPLICATION\r\n");
    PRINTF(" SSD1306 128x64 I2C DISPLAY\r\n");
    PRINTF("================================\r\n");

    /* Hardware initialisation */
    OLED_PinsInit();
    OLED_I2CInit();

    PRINTF("I2C initialised.\r\n");

    /* Initialise display */
    if (!OLED_Init())
    {
        PRINTF("OLED initialisation FAILED!\r\n");

        while (1)
        {
            /* Stop here */
        }
    }

    PRINTF("OLED initialised successfully.\r\n");

    /* Main display test */
    while (1)
    {
        PRINTF("OLED WHITE TEST\r\n");

        if (!OLED_Fill(0xFFU))
        {
            PRINTF("OLED write error!\r\n");
            break;
        }

        SDK_DelayAtLeastUs(1500000U, SystemCoreClock);

        PRINTF("OLED BLACK TEST\r\n");

        if (!OLED_Fill(0x00U))
        {
            PRINTF("OLED write error!\r\n");
            break;
        }

        SDK_DelayAtLeastUs(1500000U, SystemCoreClock);

        PRINTF("OLED STRIPE TEST\r\n");

        if (!OLED_Fill(0xAAU))
        {
            PRINTF("OLED write error!\r\n");
            break;
        }

        SDK_DelayAtLeastUs(1500000U, SystemCoreClock);
    }

    while (1)
    {
        /* Error state */
    }
}
