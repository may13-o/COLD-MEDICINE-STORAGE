
/*
 * Project : SHT40 Temperature and Humidity Monitor
 * Board   : FRDM-MCXC444
 * Sensor  : 7Semi SHT40
 * I2C     : 100 kHz
 * UART    : 115200 baud
 */

#include "app.h"
#include "board.h"
#include "clock_config.h"
#include "pin_mux.h"

#include "fsl_common.h"
#include "fsl_i2c.h"
#include "fsl_debug_console.h"

#include <stdint.h>
#include <stdbool.h>

/* SHT40 CONFIGURATION */
#define SHT40_ADDRESS          0x44U
#define SHT40_CMD_RESET        0x94U
#define SHT40_CMD_MEASURE      0xFDU

#define I2C_BAUDRATE           100000U
#define MEASUREMENT_DELAY_US   10000U
#define LOOP_DELAY_US          2000000U



/* ---------------- SENSOR VARIABLES ---------------- */

static float temperature = 0.0f;
static float humidity = 0.0f;


/* ============================================================
 * CRC-8 CALCULATION
 *
 * Polynomial : 0x31
 * Initial CRC: 0xFF
 * ============================================================ */

static uint8_t SHT40_CRC8(uint8_t *data, uint8_t length)
{
    uint8_t crc = 0xFFU;

    for (uint8_t i = 0; i < length; i++)
    {
        crc ^= data[i];

        for (uint8_t bit = 0; bit < 8; bit++)
        {
            if (crc & 0x80U)
            {
                crc = (uint8_t)((crc << 1) ^ 0x31U);
            }
            else
            {
                crc <<= 1;
            }
        }
    }

    return crc;
}


/* ============================================================
 * I2C WRITE FUNCTION
 * ============================================================ */

static status_t SHT40_WriteCommand(uint8_t command)
{
    i2c_master_transfer_t transfer = {0};

    transfer.slaveAddress = SHT40_ADDRESS;
    transfer.direction = kI2C_Write;
    transfer.data = &command;
    transfer.dataSize = 1U;
    transfer.flags = kI2C_TransferDefaultFlag;

    return I2C_MasterTransferBlocking(I2C1, &transfer);
}


/* ============================================================
 * I2C READ FUNCTION
 * ============================================================ */

static status_t SHT40_ReadBytes(uint8_t *data, uint8_t length)
{
    i2c_master_transfer_t transfer = {0};

    transfer.slaveAddress = SHT40_ADDRESS;
    transfer.direction = kI2C_Read;
    transfer.data = data;
    transfer.dataSize = length;
    transfer.flags = kI2C_TransferDefaultFlag;

    return I2C_MasterTransferBlocking(I2C1, &transfer);
}


/* ============================================================
 * SENSOR INITIALIZATION
 * ============================================================ */

static bool SHT40_Init(void)
{
    PRINTF("\r\nInitializing SHT40...\r\n");

    /* Send soft reset command */
    if (SHT40_WriteCommand(SHT40_CMD_RESET) != kStatus_Success)
    {
        PRINTF("SHT40 reset command failed!\r\n");
        return false;
    }

    /* Wait for sensor reset */
    SDK_DelayAtLeastUs(1000U,
                       CLOCK_GetFreq(kCLOCK_CoreSysClk));

    PRINTF("SHT40 initialized successfully!\r\n");

    return true;
}


/* ============================================================
 * TEMPERATURE AND HUMIDITY MEASUREMENT
 * ============================================================ */

static bool SHT40_ReadTemperatureHumidity(float *temp,
                                           float *hum)
{
    uint8_t data[6] = {0};

    /* Start high precision measurement */
    if (SHT40_WriteCommand(SHT40_CMD_MEASURE)
        != kStatus_Success)
    {
        PRINTF("Measurement command failed!\r\n");
        return false;
    }

    /* Wait for measurement completion */
    SDK_DelayAtLeastUs(MEASUREMENT_DELAY_US,
                       CLOCK_GetFreq(kCLOCK_CoreSysClk));

    /* Read six bytes */
    if (SHT40_ReadBytes(data, 6U) != kStatus_Success)
    {
        PRINTF("Sensor data read failed!\r\n");
        return false;
    }

    /* Validate temperature CRC */
    if (SHT40_CRC8(&data[0], 2U) != data[2])
    {
        PRINTF("Temperature CRC error!\r\n");
        return false;
    }

    /* Validate humidity CRC */
    if (SHT40_CRC8(&data[3], 2U) != data[5])
    {
        PRINTF("Humidity CRC error!\r\n");
        return false;
    }

    /* Convert received bytes into raw values */
    uint16_t rawTemperature =
        ((uint16_t)data[0] << 8) | data[1];

    uint16_t rawHumidity =
        ((uint16_t)data[3] << 8) | data[4];

    /* Convert raw temperature to Celsius */
    *temp = -45.0f +
            (175.0f * (float)rawTemperature / 65535.0f);

    /* Convert raw humidity to relative humidity */
    *hum = -6.0f +
           (125.0f * (float)rawHumidity / 65535.0f);

    /* Limit humidity to valid percentage range */
    if (*hum > 100.0f)
    {
        *hum = 100.0f;
    }

    if (*hum < 0.0f)
    {
        *hum = 0.0f;
    }

    return true;
}


/* ============================================================
 * PRINT SENSOR VALUES
 * Integer formatting avoids float printf dependency.
 * ============================================================ */

static void SHT40_PrintValues(float temp, float hum)
{
    int32_t t = (int32_t)(temp * 100.0f);
    int32_t h = (int32_t)(hum * 100.0f);

    int32_t tWhole = t / 100;
    int32_t tFraction = t % 100;

    int32_t hWhole = h / 100;
    int32_t hFraction = h % 100;

    if (tFraction < 0)
    {
        tFraction = -tFraction;
    }

    if (hFraction < 0)
    {
        hFraction = -hFraction;
    }

    PRINTF("--------------------------------\r\n");

    PRINTF("Temperature : %ld.%02ld C\r\n",
           (long)tWhole, (long)tFraction);

    PRINTF("Humidity    : %ld.%02ld %%RH\r\n",
           (long)hWhole, (long)hFraction);

    PRINTF("--------------------------------\r\n");
}


/* ============================================================
 * MAIN APPLICATION
 * ============================================================ */


int main(void)
{
    /* Initialize board pins and clocks */
    BOARD_InitHardware();

    /* Initialize UART debug console */
    BOARD_InitDebugConsole();

    PRINTF("\r\n");
    PRINTF("====================================\r\n");
    PRINTF("   FRDM-MCXC444 SHT40 MONITOR\r\n");
    PRINTF("====================================\r\n");

    /* Initialize I2C */
    i2c_master_config_t i2cConfig;

    I2C_MasterGetDefaultConfig(&i2cConfig);
    i2cConfig.baudRate_Bps = I2C_BAUDRATE;

    I2C_MasterInit(
        I2C1,
        &i2cConfig,
        CLOCK_GetFreq(kCLOCK_BusClk)
    );

    PRINTF("I2C1 initialized at 100 kHz\r\n");

    /* Initialize sensor */
    if (!SHT40_Init())
    {
        PRINTF("Check sensor wiring and I2C address.\r\n");

        while (1)
        {
        }
    }

    PRINTF("\r\nStarting sensor measurements...\r\n");

    /* Continuous measurement */
    while (1)
    {
        if (SHT40_ReadTemperatureHumidity(
                &temperature, &humidity))
        {
            SHT40_PrintValues(temperature, humidity);
        }
        else
        {
            PRINTF("SHT40 reading error!\r\n");
        }

        SDK_DelayAtLeastUs(
            LOOP_DELAY_US,
            CLOCK_GetFreq(kCLOCK_CoreSysClk)
        );
    }
}

