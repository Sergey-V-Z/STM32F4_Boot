/*
 * pwm_controller.c
 *
 *  Created on: 4 дек. 2025 г.
 *      Author: Copilot
 *  Description: PWM controller implementation for 6 channels.
 *               Duty is applied directly and immediately — fade and
 *               channel on/off live on the LD (DAC) side, see tca9548a.c.
 */

#include "pwm_controller.h"
#include "stm32f4xx_hal.h"
#include "tim.h"

/* Global PWM controller instance */
PWM_Controller_t pwm_controller = {0};

/* Private function prototypes */
static bool PWM_ConfigureTimers(void);
static void PWM_SetCCR(PWM_Channel_t channel, uint16_t value);

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

    // Configure and start timers
    if (!PWM_ConfigureTimers()) {
        return false;
    }

    pwm_controller.initialized = true;

    // Duty starts at 0: even though on/off is normally gated by the LD pin
    // (DAC), MCP4725 keeps its last Fast-Write value across an MCU-only
    // reset, so LD alone isn't a guaranteed-safe boot state. Keeping PWM at
    // 0 here guarantees dark LEDs from power-on until a channel is actually
    // turned on (DAC_ChannelOn/FadeOn/SetPercent then raise it to max).
    for (int i = 0; i < PWM_CH_COUNT; i++) {
        PWM_SetValue((PWM_Channel_t)i, 0);
    }

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
 * @brief Set PWM duty for a channel — applied immediately, no fade/gating
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
    PWM_SetCCR(channel, value);

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
 * @brief Set all channels to the same value
 */
void PWM_SetAllChannels(uint16_t value)
{
    for (int i = 0; i < PWM_CH_COUNT; i++) {
        PWM_SetValue((PWM_Channel_t)i, value);
    }
}
