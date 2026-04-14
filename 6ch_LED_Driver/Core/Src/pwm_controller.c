/*
 * pwm_controller.c
 *
 *  Created on: 4 дек. 2025 г.
 *      Author: Copilot
 *  Description: PWM controller implementation for 6 channels
 */

#include "pwm_controller.h"
#include "stm32f4xx_hal.h"
#include "tim.h"
#include "FreeRTOS.h"
#include "task.h"

/* Global PWM controller instance */
PWM_Controller_t pwm_controller = {0};

/* Private function prototypes */
static bool PWM_ConfigureTimers(void);
static void PWM_SetCCR(PWM_Channel_t channel, uint16_t value);
static void PWM_StartFade(PWM_Channel_t channel, uint16_t target, bool to_disable);

/**
 * @brief Initialize PWM controller
 */
bool PWM_Init(TIM_HandleTypeDef *htim1, TIM_HandleTypeDef *htim4)
{
    if (htim1 == NULL || htim4 == NULL) {
        return false;
    }

    pwm_controller.htim1 = htim1;
    pwm_controller.htim4 = htim4;

    // Initialize all channels to 0 and disabled
    for (int i = 0; i < PWM_CH_COUNT; i++) {
        pwm_controller.channels[i].value         = 0;
        pwm_controller.channels[i].enabled       = false;
        pwm_controller.channels[i].current_pwm_f = 0.0f;
        pwm_controller.channels[i].start_pwm_f   = 0.0f;
        pwm_controller.channels[i].target_value  = 0;
        pwm_controller.channels[i].fade_active   = false;
        pwm_controller.channels[i].fade_to_disable = false;
    }

    pwm_controller.fade_duration_ms = FADE_DEFAULT_DURATION_MS;

    // Configure and start timers
    if (!PWM_ConfigureTimers()) {
        return false;
    }

    pwm_controller.initialized = true;
    return true;
}

/**
 * @brief Configure TIM1 and TIM4 for PWM generation
 */
static bool PWM_ConfigureTimers(void)
{
    // Reconfigure timers for 1kHz PWM with 0-1000 range
    PWM_ReconfigureTimers();

    // Start PWM on all channels
    // TIM1 channels (CH1, CH2, CH3, CH4)
    if (HAL_TIM_PWM_Start(pwm_controller.htim1, TIM_CHANNEL_1) != HAL_OK) return false;
    if (HAL_TIM_PWM_Start(pwm_controller.htim1, TIM_CHANNEL_2) != HAL_OK) return false;
    if (HAL_TIM_PWM_Start(pwm_controller.htim1, TIM_CHANNEL_3) != HAL_OK) return false;
    if (HAL_TIM_PWM_Start(pwm_controller.htim1, TIM_CHANNEL_4) != HAL_OK) return false;

    // TIM4 channels (CH1, CH2)
    if (HAL_TIM_PWM_Start(pwm_controller.htim4, TIM_CHANNEL_1) != HAL_OK) return false;
    if (HAL_TIM_PWM_Start(pwm_controller.htim4, TIM_CHANNEL_2) != HAL_OK) return false;

    return true;
}

/**
 * @brief Write CCR register directly for a channel
 */
static void PWM_SetCCR(PWM_Channel_t channel, uint16_t value)
{
    if (value > PWM_MAX_VALUE) {
        value = PWM_MAX_VALUE;
    }

    switch (channel) {
        case PWM_CH1:
            __HAL_TIM_SET_COMPARE(pwm_controller.htim4, TIM_CHANNEL_2, value);
            break;
        case PWM_CH2:
            __HAL_TIM_SET_COMPARE(pwm_controller.htim4, TIM_CHANNEL_1, value);
            break;
        case PWM_CH3:
            __HAL_TIM_SET_COMPARE(pwm_controller.htim1, TIM_CHANNEL_4, value);
            break;
        case PWM_CH4:
            __HAL_TIM_SET_COMPARE(pwm_controller.htim1, TIM_CHANNEL_3, value);
            break;
        case PWM_CH5:
            __HAL_TIM_SET_COMPARE(pwm_controller.htim1, TIM_CHANNEL_2, value);
            break;
        case PWM_CH6:
            __HAL_TIM_SET_COMPARE(pwm_controller.htim1, TIM_CHANNEL_1, value);
            break;
        default:
            break;
    }
}

/**
 * @brief Start a fade transition for a channel
 * @param channel  Channel to fade
 * @param target   Destination PWM value (0-1000)
 * @param to_disable  If true, set enabled=false after fade reaches 0
 */
static void PWM_StartFade(PWM_Channel_t channel, uint16_t target, bool to_disable)
{
    pwm_controller.channels[channel].start_pwm_f    = pwm_controller.channels[channel].current_pwm_f;
    pwm_controller.channels[channel].target_value   = target;
    pwm_controller.channels[channel].fade_active    = true;
    pwm_controller.channels[channel].fade_to_disable = to_disable;
}

/**
 * @brief Set PWM value for a channel
 */
bool PWM_SetValue(PWM_Channel_t channel, uint16_t value)
{
    if (channel >= PWM_CH_COUNT || !pwm_controller.initialized) {
        return false;
    }

    if (value > PWM_MAX_VALUE) {
        value = PWM_MAX_VALUE;
    }

    pwm_controller.channels[channel].value = value;

    if (pwm_controller.channels[channel].enabled) {
        // Start smooth fade from current position to new target
        PWM_StartFade(channel, value, false);
    }
    // If disabled: just save value; it will be used when Enable is called

    return true;
}

/**
 * @brief Get current PWM value
 */
uint16_t PWM_GetValue(PWM_Channel_t channel)
{
    if (channel >= PWM_CH_COUNT) {
        return 0;
    }

    return pwm_controller.channels[channel].value;
}

/**
 * @brief Enable PWM channel instantly (no fade)
 */
bool PWM_Enable(PWM_Channel_t channel)
{
    if (channel >= PWM_CH_COUNT || !pwm_controller.initialized) {
        return false;
    }

    pwm_controller.channels[channel].enabled       = true;
    pwm_controller.channels[channel].fade_active   = false;
    pwm_controller.channels[channel].fade_to_disable = false;
    pwm_controller.channels[channel].current_pwm_f = (float)pwm_controller.channels[channel].value;
    PWM_SetCCR(channel, pwm_controller.channels[channel].value);

    return true;
}

/**
 * @brief Disable PWM channel instantly (no fade)
 */
bool PWM_Disable(PWM_Channel_t channel)
{
    if (channel >= PWM_CH_COUNT || !pwm_controller.initialized) {
        return false;
    }

    pwm_controller.channels[channel].enabled       = false;
    pwm_controller.channels[channel].fade_active   = false;
    pwm_controller.channels[channel].fade_to_disable = false;
    pwm_controller.channels[channel].current_pwm_f = 0.0f;
    PWM_SetCCR(channel, 0);

    return true;
}

/**
 * @brief Enable PWM channel with smooth fade-in to stored brightness
 */
bool PWM_EnableFade(PWM_Channel_t channel)
{
    if (channel >= PWM_CH_COUNT || !pwm_controller.initialized) {
        return false;
    }

    pwm_controller.channels[channel].enabled = true;
    pwm_controller.channels[channel].current_pwm_f = 0.0f;
    PWM_SetCCR(channel, 0);
    PWM_StartFade(channel, pwm_controller.channels[channel].value, false);

    return true;
}

/**
 * @brief Disable PWM channel with smooth fade-out to 0
 */
bool PWM_DisableFade(PWM_Channel_t channel)
{
    if (channel >= PWM_CH_COUNT || !pwm_controller.initialized) {
        return false;
    }

    PWM_StartFade(channel, 0, true);

    return true;
}

/**
 * @brief Check if channel is enabled
 */
bool PWM_IsEnabled(PWM_Channel_t channel)
{
    if (channel >= PWM_CH_COUNT) {
        return false;
    }

    return pwm_controller.channels[channel].enabled;
}

/**
 * @brief Set all channels to the same value
 */
void PWM_SetAllChannels(uint16_t value)
{
    for (int i = 0; i < PWM_CH_COUNT; i++) {
        PWM_SetValue((PWM_Channel_t)i, value);
    }
}

/**
 * @brief Enable all channels instantly (no fade)
 */
void PWM_EnableAll(void)
{
    for (int i = 0; i < PWM_CH_COUNT; i++) {
        PWM_Enable((PWM_Channel_t)i);
    }
}

/**
 * @brief Disable all channels instantly (no fade)
 */
void PWM_DisableAll(void)
{
    for (int i = 0; i < PWM_CH_COUNT; i++) {
        PWM_Disable((PWM_Channel_t)i);
    }
}

/**
 * @brief Enable all channels with smooth fade-in
 */
void PWM_EnableFadeAll(void)
{
    for (int i = 0; i < PWM_CH_COUNT; i++) {
        PWM_EnableFade((PWM_Channel_t)i);
    }
}

/**
 * @brief Disable all channels with smooth fade-out
 */
void PWM_DisableFadeAll(void)
{
    for (int i = 0; i < PWM_CH_COUNT; i++) {
        PWM_DisableFade((PWM_Channel_t)i);
    }
}

/**
 * @brief Set fade duration (time from current brightness to target)
 */
void PWM_SetFadeDuration(uint16_t ms)
{
    if (ms < 50)  ms = 50;
    if (ms > 10000) ms = 10000;
    pwm_controller.fade_duration_ms = ms;
}

/**
 * @brief Get current fade duration
 */
uint16_t PWM_GetFadeDuration(void)
{
    return pwm_controller.fade_duration_ms;
}

/**
 * @brief Advance all active fades by one tick.
 *        Call this function every FADE_TICK_MS milliseconds from a dedicated task.
 */
void PWM_FadeTick(void)
{
    if (!pwm_controller.initialized) {
        return;
    }

    uint16_t total_ticks = pwm_controller.fade_duration_ms / FADE_TICK_MS;
    if (total_ticks == 0) total_ticks = 1;

    taskENTER_CRITICAL();
    for (int i = 0; i < PWM_CH_COUNT; i++) {
        PWM_ChannelState_t *ch = &pwm_controller.channels[i];

        if (!ch->fade_active) {
            continue;
        }

        float step = ((float)ch->target_value - ch->start_pwm_f) / (float)total_ticks;
        ch->current_pwm_f += step;

        bool done = false;
        if (step > 0.0f && ch->current_pwm_f >= (float)ch->target_value) {
            done = true;
        } else if (step < 0.0f && ch->current_pwm_f <= (float)ch->target_value) {
            done = true;
        } else if (step == 0.0f) {
            done = true;
        }

        if (done) {
            ch->current_pwm_f = (float)ch->target_value;
            ch->fade_active = false;
            if (ch->fade_to_disable) {
                ch->enabled = false;
                ch->fade_to_disable = false;
            }
        }

        PWM_SetCCR((PWM_Channel_t)i, (uint16_t)ch->current_pwm_f);
    }
    taskEXIT_CRITICAL();
}
