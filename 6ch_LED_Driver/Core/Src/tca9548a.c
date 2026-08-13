#include "tca9548a.h"
#include "pwm_controller.h"
#include "main.h"

uint16_t g_dac_setpoints[DAC_CH_COUNT]    = {0};
uint16_t g_percent_setpoints[DAC_CH_COUNT] = {0};

uint8_t g_mcp4725_addr[DAC_CH_COUNT]  = {0};
uint8_t g_mcp4725_found[DAC_CH_COUNT] = {0};

#define I2C_TIMEOUT_MS 10U

HAL_StatusTypeDef TCA9548A_SelectChannel(uint8_t channel)
{
    if (channel > 7U) return HAL_ERROR;
    uint8_t reg = (uint8_t)(1U << channel);
    HAL_StatusTypeDef ret = HAL_I2C_Master_Transmit(&hi2c3, (uint16_t)(TCA9548A_ADDR << 1),
                                                     &reg, 1, I2C_TIMEOUT_MS);
    if (ret != HAL_OK)
        STM_LOG("TCA9548A SelectCh %u ERR err=%d state=0x%02X",
                channel, (int)ret, (unsigned)HAL_I2C_GetState(&hi2c3));
    return ret;
}

HAL_StatusTypeDef TCA9548A_DisableAll(void)
{
    uint8_t reg = 0x00U;
    HAL_StatusTypeDef ret = HAL_I2C_Master_Transmit(&hi2c3, (uint16_t)(TCA9548A_ADDR << 1),
                                                     &reg, 1, I2C_TIMEOUT_MS);
    if (ret != HAL_OK)
        STM_LOG("TCA9548A DisableAll ERR err=%d", (int)ret);
    return ret;
}

static HAL_StatusTypeDef DAC_PrepareChannel(uint8_t channel)
{
#ifdef MCP4725_BYPASS_TCA9548A
    (void)channel;
    return HAL_OK;
#else
    return TCA9548A_SelectChannel(channel);
#endif
}

// ---------------------------------------------------------------------------
// Поиск адресов MCP4725 на каждом канале мультиплексора.
// Выполняется один раз при старте, не изменяет выходы DAC
// (HAL_I2C_IsDeviceReady не пишет данные в устройство).
// ---------------------------------------------------------------------------
HAL_StatusTypeDef DAC_ScanAddresses(void)
{
    HAL_StatusTypeDef overall = HAL_OK;

    // Захватываем мьютекс на весь скан: иначе команда от другого источника
    // (TCP/UART) может переключить мультиплексор между выбором канала и
    // опросом адреса, и адрес будет приписан не тому каналу.
    osMutexWait(dacI2cMutexHandle, osWaitForever);

    for (uint8_t channel = 0U; channel < DAC_CH_COUNT; channel++)
    {
        g_mcp4725_found[channel] = 0U;
        g_mcp4725_addr[channel]  = MCP4725_ADDR;

        if (DAC_PrepareChannel(channel) != HAL_OK)
        {
            STM_LOG("DAC scan ch%u: mux select failed", channel);
            overall = HAL_ERROR;
            continue;
        }

        for (uint8_t addr = MCP4725_ADDR_MIN; addr <= MCP4725_ADDR_MAX; addr++)
        {
            if (HAL_I2C_IsDeviceReady(&hi2c3, (uint16_t)(addr << 1), 1U, I2C_TIMEOUT_MS) == HAL_OK)
            {
                g_mcp4725_addr[channel]  = addr;
                g_mcp4725_found[channel] = 1U;
                STM_LOG("DAC scan ch%u: found MCP4725 at 0x%02X", channel, addr);
                break;
            }
        }

        if (!g_mcp4725_found[channel])
        {
            overall = HAL_ERROR;
            STM_LOG("DAC scan ch%u: MCP4725 not found", channel);
        }
    }

#ifndef MCP4725_BYPASS_TCA9548A
    TCA9548A_DisableAll();
#endif

    osMutexRelease(dacI2cMutexHandle);

    return overall;
}

HAL_StatusTypeDef DAC_SetRaw(uint8_t channel, uint16_t dac_value)
{
    if (channel >= DAC_CH_COUNT) return HAL_ERROR;

    if (!g_mcp4725_found[channel])
    {
        STM_LOG("DAC_SetRaw ch%u: no MCP4725 found", channel);
        return HAL_ERROR;
    }

    // "Выбор канала мультиплексора + запись ЦАП" — атомарная операция,
    // иначе параллельная команда с другого транспорта (TCP/UART) может
    // переключить мультиплексор на свой канал между этими двумя шагами,
    // и значение уйдёт не туда.
    osMutexWait(dacI2cMutexHandle, osWaitForever);

    HAL_StatusTypeDef ret = HAL_ERROR;

    for (uint8_t attempt = 0U; attempt < DAC_WRITE_MAX_RETRIES; attempt++)
    {
        ret = DAC_PrepareChannel(channel);
        if (ret != HAL_OK)
        {
            STM_LOG("DAC_SetRaw ch%u: mux select failed (try %u/%u)",
                    channel, attempt + 1U, DAC_WRITE_MAX_RETRIES);
            continue;
        }

        ret = MCP4725_SetDAC(&hi2c3, g_mcp4725_addr[channel], dac_value);
        if (ret != HAL_OK)
        {
            STM_LOG("DAC_SetRaw ch%u val=%u: write failed (try %u/%u)",
                    channel, dac_value, attempt + 1U, DAC_WRITE_MAX_RETRIES);
            continue;
        }

        // Помеха на шине может исказить данные, но при этом HAL всё равно
        // отчитается HAL_OK (ACK пришёл, а не то, что легло в регистр ЦАП
        // совпадает с отправленным) — поэтому реальный успех подтверждаем
        // обратным чтением, а не статусом передачи.
        uint16_t readback = 0U;
        ret = MCP4725_GetDAC(&hi2c3, g_mcp4725_addr[channel], &readback);
        if (ret != HAL_OK)
        {
            STM_LOG("DAC_SetRaw ch%u: readback failed (try %u/%u)",
                    channel, attempt + 1U, DAC_WRITE_MAX_RETRIES);
            continue;
        }

        if (readback != dac_value)
        {
            STM_LOG("DAC_SetRaw ch%u: verify mismatch want=%u got=%u (try %u/%u)",
                    channel, dac_value, readback, attempt + 1U, DAC_WRITE_MAX_RETRIES);
            ret = HAL_ERROR;
            continue;
        }

        break; // записано и подтверждено обратным чтением
    }

    if (ret == HAL_OK)
    {
        g_dac_setpoints[channel] = dac_value;
    }
    else
    {
        STM_LOG("DAC_SetRaw ch%u val=%u: FAILED after %u attempts",
                channel, dac_value, DAC_WRITE_MAX_RETRIES);
    }

    osMutexRelease(dacI2cMutexHandle);
    return ret;
}

static uint16_t DAC_PercentToRaw(uint16_t percent)
{
    if (percent > DAC_PERCENT_MAX) percent = DAC_PERCENT_MAX;
    // 0-1000 → DAC_MIN_COUNT..DAC_MAX_COUNT (0.2В..1.5В на LD, см. mcp4725.h).
    return (uint16_t)(DAC_MIN_COUNT +
        ((uint32_t)(DAC_MAX_COUNT - DAC_MIN_COUNT) * percent) / DAC_PERCENT_MAX);
}

// ---------------------------------------------------------------------------
// Состояние включения/fade канала по LD (см. DAC_ChannelOn/Off/FadeOn/FadeOff
// ниже) — объявлено здесь, т.к. используется уже в DAC_SetPercent.
// ---------------------------------------------------------------------------
bool g_dac_enabled[DAC_CH_COUNT] = {0};

static float    s_dac_current_f[DAC_CH_COUNT]       = {0};
static float    s_dac_start_f[DAC_CH_COUNT]         = {0};
static uint16_t s_dac_fade_target[DAC_CH_COUNT]     = {0};
static bool     s_dac_fade_active[DAC_CH_COUNT]     = {0};
static bool     s_dac_fade_to_disable[DAC_CH_COUNT] = {0};
static uint16_t s_fade_duration_ms = FADE_DEFAULT_DURATION_MS;

static void DAC_StartFade(uint8_t channel, uint16_t target_raw, bool to_disable)
{
    s_dac_start_f[channel]         = s_dac_current_f[channel];
    s_dac_fade_target[channel]     = target_raw;
    s_dac_fade_active[channel]     = true;
    s_dac_fade_to_disable[channel] = to_disable;
}

HAL_StatusTypeDef DAC_SetPercent(uint8_t channel, uint16_t percent)
{
    if (channel >= DAC_CH_COUNT) return HAL_ERROR;
    if (percent > DAC_PERCENT_MAX) percent = DAC_PERCENT_MAX;

    g_percent_setpoints[channel] = percent;
    uint16_t dac_val = DAC_PercentToRaw(percent);

    if (!g_dac_enabled[channel])
    {
        // Канал выключен: уставка только запомнена, ни ЦАП, ни ШИМ не трогаем —
        // применится при включении (DAC_ChannelOn/FadeOn).
        return HAL_OK;
    }

    // Канал уже включён — уровень LD выставляется сразу, без плавного
    // перехода (ретраи и проверка обратным чтением — внутри DAC_SetRaw).
    // ШИМ на максимум: duty больше не ограничивает ток (см. pwm_controller.h).
    PWM_SetValue((PWM_Channel_t)channel, PWM_MAX_VALUE);
    HAL_StatusTypeDef ret = DAC_SetRaw(channel, dac_val);
    STM_LOG("DAC ch%u %u.%u%% -> count=%u %s", channel, percent / 10U, percent % 10U, dac_val,
            (ret == HAL_OK) ? "OK" : "FAILED");
    return ret;
}

HAL_StatusTypeDef DAC_DisableChannel(uint8_t channel)
{
    if (channel >= DAC_CH_COUNT) return HAL_ERROR;

    g_dac_enabled[channel]         = false;
    s_dac_fade_active[channel]     = false;
    s_dac_fade_to_disable[channel] = false;
    s_dac_current_f[channel]       = 0.0f;
    g_percent_setpoints[channel]   = 0U;

    // Сначала обнуляем LD пин, потом отключаем ШИМ (силовое выключение,
    // жёстче обычного cmd 4 — используется калибровкой тока и cmd 25)
    HAL_StatusTypeDef ret = DAC_SetRaw(channel, 0U);
    PWM_SetValue((PWM_Channel_t)channel, 0U);
    return ret;
}

HAL_StatusTypeDef DAC_DisableAll(void)
{
    HAL_StatusTypeDef ret = HAL_OK;
    for (uint8_t i = 0U; i < DAC_CH_COUNT; i++)
    {
        if (DAC_DisableChannel(i) != HAL_OK) ret = HAL_ERROR;
    }
    return ret;
}

// ---------------------------------------------------------------------------
// Включение/выключение канала и плавное зажигание/гашение (fade) через LD.
// ---------------------------------------------------------------------------

HAL_StatusTypeDef DAC_ChannelOn(uint8_t channel)
{
    if (channel >= DAC_CH_COUNT) return HAL_ERROR;

    g_dac_enabled[channel]         = true;
    s_dac_fade_active[channel]     = false;
    s_dac_fade_to_disable[channel] = false;

    uint16_t dac_val = DAC_PercentToRaw(g_percent_setpoints[channel]);
    s_dac_current_f[channel] = (float)dac_val;

    PWM_SetValue((PWM_Channel_t)channel, PWM_MAX_VALUE);
    return DAC_SetRaw(channel, dac_val);
}

HAL_StatusTypeDef DAC_ChannelOff(uint8_t channel)
{
    if (channel >= DAC_CH_COUNT) return HAL_ERROR;

    g_dac_enabled[channel]         = false;
    s_dac_fade_active[channel]     = false;
    s_dac_fade_to_disable[channel] = false;
    s_dac_current_f[channel]       = 0.0f;

    return DAC_SetRaw(channel, 0U);
}

HAL_StatusTypeDef DAC_ChannelFadeOn(uint8_t channel)
{
    if (channel >= DAC_CH_COUNT) return HAL_ERROR;

    PWM_SetValue((PWM_Channel_t)channel, PWM_MAX_VALUE);

    if (g_dac_enabled[channel] && !s_dac_fade_active[channel])
    {
        // Канал уже включён и горит ровно (не в процессе fade) — повторная
        // fade-on команда не должна перезапускать зажигание с нуля.
        return HAL_OK;
    }

    g_dac_enabled[channel] = true;

    if (!s_dac_fade_active[channel])
    {
        // Был выключен — стартуем зажигание с нуля.
        s_dac_current_f[channel] = 0.0f;
        DAC_SetRaw(channel, 0U);
    }
    // Если fade уже идёт (например, шло гашение) — едем вверх от текущего
    // значения, не прыгаем на 0.

    DAC_StartFade(channel, DAC_PercentToRaw(g_percent_setpoints[channel]), false);
    return HAL_OK;
}

HAL_StatusTypeDef DAC_ChannelFadeOff(uint8_t channel)
{
    if (channel >= DAC_CH_COUNT) return HAL_ERROR;

    DAC_StartFade(channel, 0U, true);
    return HAL_OK;
}

bool DAC_ChannelIsOn(uint8_t channel)
{
    if (channel >= DAC_CH_COUNT) return false;
    return g_dac_enabled[channel];
}

HAL_StatusTypeDef DAC_ChannelOnAll(void)
{
    HAL_StatusTypeDef ret = HAL_OK;
    for (uint8_t i = 0U; i < DAC_CH_COUNT; i++)
        if (DAC_ChannelOn(i) != HAL_OK) ret = HAL_ERROR;
    return ret;
}

HAL_StatusTypeDef DAC_ChannelOffAll(void)
{
    HAL_StatusTypeDef ret = HAL_OK;
    for (uint8_t i = 0U; i < DAC_CH_COUNT; i++)
        if (DAC_ChannelOff(i) != HAL_OK) ret = HAL_ERROR;
    return ret;
}

HAL_StatusTypeDef DAC_ChannelFadeOnAll(void)
{
    HAL_StatusTypeDef ret = HAL_OK;
    for (uint8_t i = 0U; i < DAC_CH_COUNT; i++)
        if (DAC_ChannelFadeOn(i) != HAL_OK) ret = HAL_ERROR;
    return ret;
}

HAL_StatusTypeDef DAC_ChannelFadeOffAll(void)
{
    HAL_StatusTypeDef ret = HAL_OK;
    for (uint8_t i = 0U; i < DAC_CH_COUNT; i++)
        if (DAC_ChannelFadeOff(i) != HAL_OK) ret = HAL_ERROR;
    return ret;
}

void DAC_SetFadeDuration(uint16_t ms)
{
    if (ms < 50U)    ms = 50U;
    if (ms > 10000U) ms = 10000U;
    s_fade_duration_ms = ms;
}

uint16_t DAC_GetFadeDuration(void)
{
    return s_fade_duration_ms;
}

void DAC_FadeTick(void)
{
    uint16_t total_ticks = s_fade_duration_ms / FADE_TICK_MS;
    if (total_ticks == 0U) total_ticks = 1U;

    for (uint8_t ch = 0U; ch < DAC_CH_COUNT; ch++)
    {
        if (!s_dac_fade_active[ch]) continue;

        float step = ((float)s_dac_fade_target[ch] - s_dac_start_f[ch]) / (float)total_ticks;
        s_dac_current_f[ch] += step;

        bool done = false;
        if (step > 0.0f && s_dac_current_f[ch] >= (float)s_dac_fade_target[ch]) {
            done = true;
        } else if (step < 0.0f && s_dac_current_f[ch] <= (float)s_dac_fade_target[ch]) {
            done = true;
        } else if (step == 0.0f) {
            done = true;
        }

        if (done)
        {
            s_dac_current_f[ch] = (float)s_dac_fade_target[ch];
            s_dac_fade_active[ch] = false;
            if (s_dac_fade_to_disable[ch])
            {
                g_dac_enabled[ch] = false;
                s_dac_fade_to_disable[ch] = false;
            }
        }

        // Блокирующая I2C-транзакция — вне критической секции (см. DAC_SetRaw:
        // мьютекс + HAL с таймаутом, критическая секция тут запретила бы IRQ/SysTick).
        DAC_SetRaw(ch, (uint16_t)s_dac_current_f[ch]);
    }
}
