#include "dac_calibration.h"
#include "tca9548a.h"
#include "pwm_controller.h"
#include "flash_spi.h"
#include "cmsis_os.h"
#include <string.h>

// ---------------------------------------------------------------------------
// Внешние зависимости из freertos.cpp
// ---------------------------------------------------------------------------
extern flash mem_spi;

// ACS712 — ADC DMA переменные (определены в freertos.cpp)
extern uint16_t          adc_dma_buffer[16];
extern volatile uint8_t  adc_conversion_complete;
extern float             g_current_zero_offset;

void    ACS712_StartDMA_Conversion(void);
uint16_t ACS712_MovingAverage(uint16_t *data, uint16_t size);

// ---------------------------------------------------------------------------
// Глобальный экземпляр калибровки
// ---------------------------------------------------------------------------
dac_calibration_t g_dac_cal = {0};

// ---------------------------------------------------------------------------
// CRC32 (тот же алгоритм, что и для ACS712 calibration_t)
// ---------------------------------------------------------------------------
uint32_t DAC_Cal_CRC(dac_calibration_t *cal)
{
    uint32_t crc  = 0U;
    uint8_t *data = (uint8_t *)cal;
    uint32_t size = sizeof(dac_calibration_t) - sizeof(uint32_t); // без поля crc

    for (uint32_t i = 0U; i < size; i++)
    {
        crc ^= data[i];
        for (uint8_t j = 0U; j < 8U; j++)
        {
            crc = (crc & 1U) ? ((crc >> 1) ^ 0xEDB88320U) : (crc >> 1);
        }
    }
    return crc;
}

// ---------------------------------------------------------------------------
// Загрузка из SPI Flash
// ---------------------------------------------------------------------------
void DAC_Cal_Load(void)
{
    dac_calibration_t cal;
    mem_spi.W25qxx_ReadBytes(DAC_CAL_FLASH_ADDR, (uint8_t *)&cal, sizeof(dac_calibration_t));

    if (cal.magic_key != DAC_CAL_MAGIC)
    {
        memset(&g_dac_cal, 0, sizeof(g_dac_cal));
        return;
    }
    if (DAC_Cal_CRC(&cal) != cal.crc)
    {
        memset(&g_dac_cal, 0, sizeof(g_dac_cal));
        return;
    }
    memcpy(&g_dac_cal, &cal, sizeof(dac_calibration_t));
}

// ---------------------------------------------------------------------------
// Сохранение в SPI Flash
// ---------------------------------------------------------------------------
void DAC_Cal_Save(void)
{
    g_dac_cal.magic_key = DAC_CAL_MAGIC;
    g_dac_cal.crc       = DAC_Cal_CRC(&g_dac_cal);

    uint32_t sector = DAC_CAL_FLASH_ADDR / 4096U;
    mem_spi.W25qxx_EraseSector(sector);
    osDelay(50);
    mem_spi.W25qxx_Write(DAC_CAL_FLASH_ADDR, (uint8_t *)&g_dac_cal, sizeof(dac_calibration_t));
    osDelay(50);
}

// ---------------------------------------------------------------------------
// Сброс одного канала
// ---------------------------------------------------------------------------
void DAC_Cal_Reset(uint8_t channel)
{
    if (channel >= DAC_CAL_CH_COUNT) return;
    memset(&g_dac_cal.channels[channel], 0, sizeof(ch_dac_cal_t));
    DAC_Cal_Save();
}

// ---------------------------------------------------------------------------
// Вспомогательная функция: одиночное измерение ACS712 → ток в амперах
// (без применения g_current_zero_offset — возвращает "сырой" ток)
// ---------------------------------------------------------------------------
static float measure_raw_current(void)
{
    adc_conversion_complete = 0U;
    ACS712_StartDMA_Conversion();

    uint32_t t = osKernelSysTick();
    while (!adc_conversion_complete && (osKernelSysTick() - t) < 100U)
    {
        osDelay(1);
    }
    if (!adc_conversion_complete) return 0.0f;

    uint16_t avg    = ACS712_MovingAverage(adc_dma_buffer, 16U);
    float v_adc     = (float)avg * 3.3f / 4096.0f;
    float v_sens    = v_adc * 2.0f;          // делитель напряжения 2:1 на плате
    return (v_sens - 2.5f) / 0.1f;           // 100 мВ/А (ACS712-20A)
}

// ---------------------------------------------------------------------------
// Процедура калибровки канала
// ---------------------------------------------------------------------------
int32_t DAC_Calibrate(uint8_t channel, uint16_t test_dac)
{
    if (channel >= DAC_CAL_CH_COUNT)          return -1;
    if (test_dac < DAC_MIN_COUNT)             return -2;
    if (test_dac > DAC_MAX_COUNT)             return -3;

    // 1. Выключить все каналы: DAC=0, PWM=0
    DAC_DisableAll();
    osDelay(200);

    // 2. Измерить нулевой ток (N выборок)
    float zero_sum = 0.0f;
    for (uint8_t i = 0U; i < DAC_CAL_SAMPLES; i++)
    {
        zero_sum += measure_raw_current();
        osDelay(DAC_CAL_SAMPLE_MS);
    }
    float I_zero = zero_sum / (float)DAC_CAL_SAMPLES;

    // 3. Включить целевой канал: PWM=1000 (постоянная 1), DAC=test_dac
    PWM_SetValue((PWM_Channel_t)channel, PWM_MAX_VALUE);
    DAC_SetRaw(channel, test_dac);
    osDelay(500);  // ждём стабилизации тока

    // 4. Измерить ток под нагрузкой
    float meas_sum = 0.0f;
    for (uint8_t i = 0U; i < DAC_CAL_SAMPLES; i++)
    {
        meas_sum += measure_raw_current();
        osDelay(DAC_CAL_SAMPLE_MS);
    }
    float I_meas = meas_sum / (float)DAC_CAL_SAMPLES;

    // 5. Нетто ток канала (убираем ноль)
    float I_net_A  = I_meas - I_zero;
    int32_t I_net_mA = (int32_t)(I_net_A * 1000.0f);

    if (I_net_mA <= 0) return -4;   // некорректный результат — нет тока

    // 6. Коэффициент: мА/count
    g_dac_cal.channels[channel].coefficient    = (float)I_net_mA / (float)test_dac;
    g_dac_cal.channels[channel].cal_dac_value  = test_dac;
    g_dac_cal.channels[channel].is_calibrated  = 1U;

    DAC_Cal_Save();

    return I_net_mA;
}

// ---------------------------------------------------------------------------
// Перевод тока (мА) в DAC count
// ---------------------------------------------------------------------------
uint16_t DAC_CurrentMA_to_Count(uint8_t channel, uint16_t current_ma)
{
    if (channel >= DAC_CAL_CH_COUNT) return DAC_MIN_COUNT;

    if (!g_dac_cal.channels[channel].is_calibrated || current_ma == 0U)
        return 0U;

    float coeff = g_dac_cal.channels[channel].coefficient; // мА/count
    if (coeff <= 0.0f) return DAC_MIN_COUNT;

    uint16_t dac = (uint16_t)((float)current_ma / coeff);

    // Ограничение в рабочий диапазон LD пина
    if (dac < DAC_MIN_COUNT) dac = DAC_MIN_COUNT;
    if (dac > DAC_MAX_COUNT) dac = DAC_MAX_COUNT;
    return dac;
}
