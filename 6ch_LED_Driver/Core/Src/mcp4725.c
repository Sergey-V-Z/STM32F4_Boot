#include "mcp4725.h"
#include "main.h"

#define I2C_TIMEOUT_MS 10U

HAL_StatusTypeDef MCP4725_SetDAC(I2C_HandleTypeDef *hi2c, uint8_t addr7bit, uint16_t value)
{
    if (value > MCP4725_DAC_MAX) value = MCP4725_DAC_MAX;

    // Fast Write: [C1=0,C0=0,PD1=0,PD0=0,D11-D8] [D7-D0]
    uint8_t buf[2];
    buf[0] = (uint8_t)((value >> 8) & 0x0FU);
    buf[1] = (uint8_t)(value & 0xFFU);

    HAL_StatusTypeDef ret = HAL_I2C_Master_Transmit(hi2c, (uint16_t)(addr7bit << 1),
                                                     buf, 2, I2C_TIMEOUT_MS);
    if (ret != HAL_OK)
        STM_LOG("MCP4725 SetDAC ERR addr=0x%02X val=%u err=%d", addr7bit, value, (int)ret);
    return ret;
}

HAL_StatusTypeDef MCP4725_GetDAC(I2C_HandleTypeDef *hi2c, uint8_t addr7bit, uint16_t *value)
{
    uint8_t buf[3];
    HAL_StatusTypeDef ret = HAL_I2C_Master_Receive(hi2c, (uint16_t)(addr7bit << 1),
                                                    buf, 3, I2C_TIMEOUT_MS);
    if (ret == HAL_OK)
    {
        // buf[0] — статус, buf[1] — D11-D4, buf[2] — [D3-D0,x,x,x,x]
        *value = ((uint16_t)buf[1] << 4) | (buf[2] >> 4);
    }
    else
    {
        STM_LOG("MCP4725 GetDAC ERR addr=0x%02X err=%d", addr7bit, (int)ret);
    }
    return ret;
}
