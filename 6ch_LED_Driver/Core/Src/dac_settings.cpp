#include "dac_settings.h"
#include "firmware_update.h"
#include "flash_spi.h"
#include "pwm_controller.h"
#include "crc.h"
#include "cmsis_os.h"
#include "main.h"
#include <string.h>

extern flash mem_spi;

// Отдельная область flash для настроек каналов (не пересекается с зоной
// OTA-прошивок и калибровками ACS712/DAC, см. dac_settings.h)
#define DAC_SETTINGS_FLASH_ADDR SPI_FLASH_APP_DATA_ADDRESS

static uint32_t DAC_Settings_CRC(const channel_settings_t *s)
{
    return crc32_calculate((const uint8_t *)s, sizeof(channel_settings_t) - sizeof(uint32_t), 0);
}

void DAC_Settings_Load(void)
{
    channel_settings_t settings;
    mem_spi.W25qxx_ReadBytes(DAC_SETTINGS_FLASH_ADDR, (uint8_t *)&settings, sizeof(channel_settings_t));

    bool valid = (settings.magic_key == DAC_SETTINGS_MAGIC) &&
                 (settings.crc == DAC_Settings_CRC(&settings));

    if (!valid)
    {
        STM_LOG("DAC settings: no valid data in flash, using defaults");
        memset(&settings, 0, sizeof(settings)); // percent=0, pwm_value=0 (выключено)
        settings.fade_duration_ms = FADE_DEFAULT_DURATION_MS;
    }

    for (uint8_t ch = 0U; ch < DAC_CH_COUNT; ch++)
    {
        uint16_t percent = settings.percent[ch];
        if (percent > DAC_PERCENT_MAX) percent = DAC_PERCENT_MAX;

        // Канал ещё выключен (g_dac_enabled=false) — DAC_SetPercent только
        // запомнит уставку, в железо она уйдёт при следующем включении.
        // Попутно выставляет PWM в максимум, поэтому восстанавливаем
        // сохранённый raw PWM duty СЛЕДОМ, перезаписывая это значение.
        DAC_SetPercent(ch, percent);

        uint16_t pwm_value = settings.pwm_value[ch];
        if (pwm_value > PWM_MAX_VALUE) pwm_value = PWM_MAX_VALUE;
        PWM_SetValue((PWM_Channel_t)ch, pwm_value);

        STM_LOG("DAC settings: ch%u restored percent=%u.%u%% pwm=%u",
                ch, percent / 10U, percent % 10U, pwm_value);
    }

    uint16_t fade_ms = settings.fade_duration_ms;
    if (fade_ms < 50U || fade_ms > 10000U) fade_ms = FADE_DEFAULT_DURATION_MS;
    DAC_SetFadeDuration(fade_ms);
    STM_LOG("DAC settings: fade duration restored to %u ms", fade_ms);
}

void DAC_Settings_Save(void)
{
    channel_settings_t settings;
    settings.magic_key = DAC_SETTINGS_MAGIC;
    for (uint8_t ch = 0U; ch < DAC_CH_COUNT; ch++)
    {
        settings.percent[ch]   = g_percent_setpoints[ch];
        settings.pwm_value[ch] = PWM_GetValue((PWM_Channel_t)ch);
    }
    settings.fade_duration_ms = DAC_GetFadeDuration();
    settings.crc = DAC_Settings_CRC(&settings);

    uint32_t sector_num = DAC_SETTINGS_FLASH_ADDR / 4096U;
    mem_spi.W25qxx_EraseSector(sector_num);
    osDelay(50);

    mem_spi.W25qxx_Write(DAC_SETTINGS_FLASH_ADDR, (uint8_t *)&settings, sizeof(settings));
    osDelay(50);

    channel_settings_t verify;
    mem_spi.W25qxx_ReadBytes(DAC_SETTINGS_FLASH_ADDR, (uint8_t *)&verify, sizeof(verify));

    if (verify.magic_key != settings.magic_key || verify.crc != settings.crc)
        STM_LOG("DAC settings: flash write verification FAILED");
    else
        STM_LOG("DAC settings: saved OK");
}
