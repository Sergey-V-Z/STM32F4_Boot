#ifndef INC_TCA9548A_H_
#define INC_TCA9548A_H_

#include "stm32f4xx_hal.h"
#include "cmsis_os.h"
#include "i2c.h"
#include "mcp4725.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------------------------
// Режим обхода TCA9548A:
// Раскомментировать для прямого доступа на шину (один MCP4725, без мультиплексора)
// ---------------------------------------------------------------------------
//#define MCP4725_BYPASS_TCA9548A

// ---------------------------------------------------------------------------
// Адрес TCA9548A (A2,A1,A0 — к GND по умолчанию → 0x70)
// Изменить значения 0/1 для настройки адреса через пайку
// ---------------------------------------------------------------------------
#define TCA9548A_A0   0
#define TCA9548A_A1   0
#define TCA9548A_A2   0
#define TCA9548A_ADDR (uint8_t)(0x70U | ((TCA9548A_A2) << 2) | ((TCA9548A_A1) << 1) | (TCA9548A_A0))

#define DAC_CH_COUNT  6U

// Шкала процентов: 0 = DAC_MIN_COUNT (0.2В на LD), 1000 = DAC_MAX_COUNT (1.5В на LD)
#define DAC_PERCENT_MAX  1000U

// Fade (плавное зажигание/гашение через LD) — тайминги перехода
#define FADE_TICK_MS             10         // Период обновления fade, мс
#define FADE_DEFAULT_DURATION_MS 500        // Длительность перехода по умолчанию, мс

// Выбрать канал мультиплексора (0-7)
HAL_StatusTypeDef TCA9548A_SelectChannel(uint8_t channel);
// Закрыть все каналы мультиплексора
HAL_StatusTypeDef TCA9548A_DisableAll(void);

// Мьютекс на "выбор канала мультиплексора + транзакция с ЦАП" как одну
// атомарную операцию. Нужен, потому что TCP (eth_Task) и UART (uart_Task)
// делят один I2C3 + один мультиплексор и могут вызывать DAC_SetRaw()
// для разных каналов одновременно — без мьютекса один поток может
// переключить мультиплексор до того, как другой успеет записать ЦАП,
// и значение уйдёт не на тот канал. Создаётся в MX_FREERTOS_Init (freertos.cpp).
extern osMutexId dacI2cMutexHandle;

// ---------------------------------------------------------------------------
// Поиск адресов MCP4725 на каждом канале мультиплексора
// ---------------------------------------------------------------------------

// Диапазон возможных 7-bit адресов MCP4725xx-Ex/CH (A2 задаётся резистором)
#define MCP4725_ADDR_MIN  0x60U
#define MCP4725_ADDR_MAX  0x67U

// Найденный адрес MCP4725 для каждого канала (валиден, если g_mcp4725_found[ch] != 0)
extern uint8_t g_mcp4725_addr[DAC_CH_COUNT];
extern uint8_t g_mcp4725_found[DAC_CH_COUNT];

// ---------------------------------------------------------------------------
// Надёжность записи в DAC при помехах на I2C: каждая запись в DAC_SetRaw
// проверяется обратным чтением (readback) и при несовпадении повторяется
// до DAC_WRITE_MAX_RETRIES раз, прежде чем вернуть ошибку вызывающему коду.
// ---------------------------------------------------------------------------
#define DAC_WRITE_MAX_RETRIES  3U

// Однократный поиск адресов MCP4725 на всех каналах при старте.
// Не изменяет выходы DAC (используется HAL_I2C_IsDeviceReady).
HAL_StatusTypeDef DAC_ScanAddresses(void);

// ---------------------------------------------------------------------------
// Высокоуровневые функции управления DAC
// ---------------------------------------------------------------------------

// Установить уровень тока канала в процентах (0-1000) в пределах рабочего
// диапазона LD пина HV9961: 0 = DAC_MIN_COUNT (0.2В), 1000 = DAC_MAX_COUNT (1.5В).
// Также выставляет ШИМ в максимум (PWM_SetValue) — духа PWM больше не гасит
// канал. Если канал сейчас включён — уровень LD выставляется сразу, без
// плавного перехода (см. DAC_SetRaw — ретраи и проверка обратным чтением
// внутри неё же); возвращает реальный результат записи в железо. Если
// выключен — уставка только запоминается и применится при следующем
// включении (см. DAC_ChannelOn/FadeOn), возвращает HAL_OK.
HAL_StatusTypeDef DAC_SetPercent(uint8_t channel, uint16_t percent);

// Установить сырое значение DAC (0-4095) — для отладки, пишет немедленно,
// в обход состояния "включён"/fade
// PWM выставляется в 1000 автоматически
HAL_StatusTypeDef DAC_SetRaw(uint8_t channel, uint16_t dac_value);

// Силовое выключение канала: DAC=0, PWM=0 (жёстче обычного выключения —
// используется калибровкой тока и cmd 25, не обычным вкл/выкл cmd 4)
HAL_StatusTypeDef DAC_DisableChannel(uint8_t channel);

// Силовое выключение всех каналов
HAL_StatusTypeDef DAC_DisableAll(void);

// Текущие уставки: DAC count (реально выводимое значение, в т.ч. по ходу
// fade) и проценты (целевая уставка, 0-1000)
extern uint16_t g_dac_setpoints[DAC_CH_COUNT];
extern uint16_t g_percent_setpoints[DAC_CH_COUNT];

// ---------------------------------------------------------------------------
// Включение/выключение канала и плавное зажигание/гашение (fade) через LD.
// ШИМ (pwm_controller) при этом не трогается — он остаётся на независимо
// заданном значении (по умолчанию максимум), см. pwm_controller.h.
// ---------------------------------------------------------------------------

// Состояние "включён" по LD
extern bool g_dac_enabled[DAC_CH_COUNT];

// Включить канал мгновенно: LD сразу на сохранённую уставку percent
HAL_StatusTypeDef DAC_ChannelOn(uint8_t channel);
// Выключить канал мгновенно: LD = 0
HAL_StatusTypeDef DAC_ChannelOff(uint8_t channel);
// Включить канал с плавным нарастанием LD от 0 до уставки percent
HAL_StatusTypeDef DAC_ChannelFadeOn(uint8_t channel);
// Выключить канал с плавным спадом LD до 0
HAL_StatusTypeDef DAC_ChannelFadeOff(uint8_t channel);
// Проверить, включён ли канал
bool DAC_ChannelIsOn(uint8_t channel);

// То же самое сразу для всех каналов — возвращают HAL_ERROR, если хотя бы
// один канал не удалось применить (после ретраев в DAC_SetRaw)
HAL_StatusTypeDef DAC_ChannelOnAll(void);
HAL_StatusTypeDef DAC_ChannelOffAll(void);
HAL_StatusTypeDef DAC_ChannelFadeOnAll(void);
HAL_StatusTypeDef DAC_ChannelFadeOffAll(void);

// Длительность fade-перехода (мс, общая для всех каналов, см. cmd 7)
void DAC_SetFadeDuration(uint16_t ms);
uint16_t DAC_GetFadeDuration(void);

// Продвинуть все активные LD fade-переходы на один тик
// (вызывать раз в FADE_TICK_MS мс из отдельной задачи, см. fadeTask)
void DAC_FadeTick(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_TCA9548A_H_ */
