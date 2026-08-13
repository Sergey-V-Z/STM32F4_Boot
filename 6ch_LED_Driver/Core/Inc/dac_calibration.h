#ifndef INC_DAC_CALIBRATION_H_
#define INC_DAC_CALIBRATION_H_

#include "stm32f4xx_hal.h"
#include "mcp4725.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Адрес в SPI Flash для калибровки DAC (Sector 4, 4KB = 0x1000)
// Sector 0: 0x0000 — settings
// Sector 2: 0x2000 — ACS712 zero calibration
// Sector 4: 0x4000 — DAC channel calibration (этот файл)
#define DAC_CAL_FLASH_ADDR  0x4000U
#define DAC_CAL_MAGIC       0xDAC12345U
#define DAC_CAL_CH_COUNT    6U

// Число измерений при калибровке и интервал между ними
#define DAC_CAL_SAMPLES       5U
#define DAC_CAL_SAMPLE_MS     50U

#pragma pack(push, 1)
typedef struct {
    float    coefficient;    // мА/count — наклон линии тока
    uint16_t cal_dac_value;  // DAC count, использованный при калибровке
    uint8_t  is_calibrated;  // 1 = данные действительны
    uint8_t  reserved;
} ch_dac_cal_t;              // 8 байт на канал

typedef struct {
    uint32_t     magic_key;                    // 4 байта
    ch_dac_cal_t channels[DAC_CAL_CH_COUNT];   // 48 байт
    uint32_t     crc;                           // 4 байта
} dac_calibration_t;                           // итого 56 байт
#pragma pack(pop)

// Глобальный экземпляр (заполняется при загрузке из flash)
extern dac_calibration_t g_dac_cal;

// Загрузить калибровку из SPI Flash
void DAC_Cal_Load(void);

// Сохранить калибровку в SPI Flash
void DAC_Cal_Save(void);

// CRC32 структуры (без поля crc)
uint32_t DAC_Cal_CRC(dac_calibration_t *cal);

// Сбросить калибровку одного канала
void DAC_Cal_Reset(uint8_t channel);

// Запустить процедуру калибровки канала:
//   channel   — номер канала (0-5)
//   test_dac  — DAC count для теста (рекомендуется DAC_MIN_COUNT..DAC_MAX_COUNT)
// Возвращает измеренный ток в мА (> 0) или отрицательное число при ошибке
int32_t DAC_Calibrate(uint8_t channel, uint16_t test_dac);

// Перевести ток (мА) в DAC count на основе калибровки
// Если канал не откалиброван — возвращает DAC_MIN_COUNT
uint16_t DAC_CurrentMA_to_Count(uint8_t channel, uint16_t current_ma);

#ifdef __cplusplus
}
#endif

#endif /* INC_DAC_CALIBRATION_H_ */
