#ifndef INC_DAC_SETTINGS_H_
#define INC_DAC_SETTINGS_H_

#include "stm32f4xx_hal.h"
#include "tca9548a.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Сохранение настроек каналов в SPI Flash:
 *  - уставка тока ЦАП (g_percent_setpoints, 0-1000 на канал, cmd 21);
 *  - raw PWM duty (0-1000 на канал, cmd 5);
 *  - длительность fade-перехода (мс, общая для всех каналов, cmd 7).
 *
 * Бит включения/выключения канала (cmd 4, LD on/off-fade-state, см.
 * tca9548a.h) НЕ персистится и всегда стартует выключенным
 * (g_dac_enabled[] инициализируется нулём при старте).
 *
 * Запись во flash выполняется только по явной команде (см. cmd 27 в
 * action_commands/Сommand_execution), а не на каждое изменение — чтобы не
 * расходовать ресурс циклов перезаписи сектора при частых изменениях/fade.
 *
 * Используется отдельная область SPI_FLASH_APP_DATA_ADDRESS (firmware_update.h),
 * не пересекающаяся с зоной OTA-прошивок и калибровками ACS712/DAC
 * (см. dac_calibration.h, sector 2/4 — те координаты не трогаем).
 * ------------------------------------------------------------------------- */

#define DAC_SETTINGS_MAGIC 0x44414332U // "DAC2" (v2: + pwm_value[] и fade_duration_ms)

#pragma pack(push, 1)
typedef struct {
    uint32_t magic_key;
    uint16_t percent[DAC_CH_COUNT];   // уставки тока 0-1000 на канал (ЦАП/LD)
    uint16_t pwm_value[DAC_CH_COUNT]; // raw PWM duty 0-1000 на канал (cmd 5)
    uint16_t fade_duration_ms;        // длительность fade-перехода, мс (cmd 7)
    uint32_t crc;
} channel_settings_t; // настройки канала в целом (ЦАП + PWM + fade), не только ЦАП
#pragma pack(pop)

// Загрузить настройки из flash и применить их:
//  - уставка ЦАП/LD каждого канала (DAC_SetPercent) — канал выключен
//    (g_dac_enabled=false), поэтому значение только запоминается, в
//    железо не пишется — применится при следующем включении (cmd 4);
//  - raw PWM (PWM_SetValue) — восстанавливается ПОСЛЕ DAC_SetPercent,
//    т.к. DAC_SetPercent попутно выставляет PWM в максимум;
//  - длительность fade (DAC_SetFadeDuration).
// Если данных нет/CRC не совпал — дефолты: percent=0, pwm=0 (выключено),
// fade=FADE_DEFAULT_DURATION_MS.
void DAC_Settings_Load(void);

// Сохранить текущие g_percent_setpoints[], raw PWM duty и fade duration во flash.
void DAC_Settings_Save(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_DAC_SETTINGS_H_ */
