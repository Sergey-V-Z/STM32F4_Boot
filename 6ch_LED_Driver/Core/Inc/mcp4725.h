#ifndef INC_MCP4725_H_
#define INC_MCP4725_H_

#include "stm32f4xx_hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MCP4725_ADDR      0x60U   // 7-bit адрес (A0=GND)
#define MCP4725_DAC_MAX   4095U   // Максимальное значение 12-bit ЦАП

// Питание MCP4725 и рабочий диапазон LD пина HV9961.
// По даташиту Microchip HV9961: гейт выключается ниже 0.15В, включается обратно
// при 0.2В (гистерезис), а при 1.5В ток уже максимальный (выше — без эффекта).
#define MCP4725_VDD_MV    3300U   // Питание в мВ
#define MCP4725_LD_MIN_MV 200U    // Минимум LD для HV9961 (с запасом от порога выключения 0.15/0.2В)
#define MCP4725_LD_MAX_MV 1500U   // Максимум LD для HV9961 (выше — ток уже максимальный)

// Границы DAC count для LD пина: 200мВ=~248, 1500мВ=~1862
#define DAC_MIN_COUNT  ((uint16_t)((uint32_t)MCP4725_LD_MIN_MV * MCP4725_DAC_MAX / MCP4725_VDD_MV))
#define DAC_MAX_COUNT  ((uint16_t)((uint32_t)MCP4725_LD_MAX_MV * MCP4725_DAC_MAX / MCP4725_VDD_MV))

// Fast Write — 2 байта, без записи в EEPROM
HAL_StatusTypeDef MCP4725_SetDAC(I2C_HandleTypeDef *hi2c, uint8_t addr7bit, uint16_t value);
// Чтение текущего значения DAC (3 байта ответа)
HAL_StatusTypeDef MCP4725_GetDAC(I2C_HandleTypeDef *hi2c, uint8_t addr7bit, uint16_t *value);

#ifdef __cplusplus
}
#endif

#endif /* INC_MCP4725_H_ */
