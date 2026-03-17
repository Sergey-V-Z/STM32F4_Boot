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

/* Global PWM controller instance */
PWM_Controller_t pwm_controller = {0};

/* Private function prototypes */
static bool PWM_ConfigureTimers(void);
static void PWM_UpdateChannel(PWM_Channel_t channel);

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
        pwm_controller.channels[i].value = 0;
        pwm_controller.channels[i].enabled = false;
    }

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
 * @brief Update PWM duty cycle for a specific channel
 */
static void PWM_UpdateChannel(PWM_Channel_t channel)
{
    uint16_t value = pwm_controller.channels[channel].enabled ? 
                     pwm_controller.channels[channel].value : 0;

    // Clamp value to max
    if (value > PWM_MAX_VALUE) {
        value = PWM_MAX_VALUE;
    }
    
    // Scale value from 0-1000 to 0-1000 (CCR value)
    // For 100% duty cycle (value=1000), CCR must be > ARR (999)
    // So CCR = 1000 gives constant HIGH output
    uint16_t compare_value = value;

    // Map channel to timer and channel
    switch (channel) {
        case PWM_CH1: // TIM4_CH2 - PD13
            __HAL_TIM_SET_COMPARE(pwm_controller.htim4, TIM_CHANNEL_2, compare_value);
            break;

        case PWM_CH2: // TIM4_CH1 - PD12
            __HAL_TIM_SET_COMPARE(pwm_controller.htim4, TIM_CHANNEL_1, compare_value);
            break;

        case PWM_CH3: // TIM1_CH4 - PE14
            __HAL_TIM_SET_COMPARE(pwm_controller.htim1, TIM_CHANNEL_4, compare_value);
            break;

        case PWM_CH4: // TIM1_CH3 - PE13
            __HAL_TIM_SET_COMPARE(pwm_controller.htim1, TIM_CHANNEL_3, compare_value);
            break;

        case PWM_CH5: // TIM1_CH2 - PE11
            __HAL_TIM_SET_COMPARE(pwm_controller.htim1, TIM_CHANNEL_2, compare_value);
            break;

        case PWM_CH6: // TIM1_CH1 - PE9
            __HAL_TIM_SET_COMPARE(pwm_controller.htim1, TIM_CHANNEL_1, compare_value);
            break;

        default:
            break;
    }
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
    PWM_UpdateChannel(channel);

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
 * @brief Enable PWM channel
 */
bool PWM_Enable(PWM_Channel_t channel)
{
    if (channel >= PWM_CH_COUNT || !pwm_controller.initialized) {
        return false;
    }

    pwm_controller.channels[channel].enabled = true;
    PWM_UpdateChannel(channel);

    return true;
}

/**
 * @brief Disable PWM channel
 */
bool PWM_Disable(PWM_Channel_t channel)
{
    if (channel >= PWM_CH_COUNT || !pwm_controller.initialized) {
        return false;
    }

    pwm_controller.channels[channel].enabled = false;
    PWM_UpdateChannel(channel);

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
 * @brief Enable all channels
 */
void PWM_EnableAll(void)
{
    for (int i = 0; i < PWM_CH_COUNT; i++) {
        PWM_Enable((PWM_Channel_t)i);
    }
}

/**
 * @brief Disable all channels
 */
void PWM_DisableAll(void)
{
    for (int i = 0; i < PWM_CH_COUNT; i++) {
        PWM_Disable((PWM_Channel_t)i);
    }
}
