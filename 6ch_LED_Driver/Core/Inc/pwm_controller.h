/*
 * pwm_controller.h
 *
 *  Created on: 4 дек. 2025 г.
 *      Author: Copilot
 *  Description: PWM controller for 6 channels using TIM1 and TIM4
 *               Connected to HV9961LG-G LED driver
 */

#ifndef INC_PWM_CONTROLLER_H_
#define INC_PWM_CONTROLLER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

/* PWM Configuration */
#define PWM_FREQUENCY_HZ        1000        // 1 kHz PWM frequency
#define PWM_MAX_VALUE          1000         // 0-1000 range (0.1% resolution)
#define PWM_PERIOD             999          // Timer ARR value (Period-1)
#define PWM_TIMER_CLOCK        84000000     // 84 MHz (APB2 for TIM1, APB1*2 for TIM4)

/* Channel mapping to timer outputs */
typedef enum {
    PWM_CH1 = 0,  // TIM4_CH2 - PD13
    PWM_CH2 = 1,  // TIM4_CH1 - PD12
    PWM_CH3 = 2,  // TIM1_CH4 - PE14
    PWM_CH4 = 3,  // TIM1_CH3 - PE13
    PWM_CH5 = 4,  // TIM1_CH2 - PE11
    PWM_CH6 = 5,  // TIM1_CH1 - PE9
    PWM_CH_COUNT = 6
} PWM_Channel_t;

/* PWM Channel state structure */
typedef struct {
    uint16_t value;          // Current PWM value (0-1000)
    bool enabled;            // Channel enabled state
} PWM_ChannelState_t;

/* PWM Controller structure */
typedef struct {
    PWM_ChannelState_t channels[PWM_CH_COUNT];
    TIM_HandleTypeDef *htim1;
    TIM_HandleTypeDef *htim4;
    bool initialized;
} PWM_Controller_t;

/* Global PWM controller instance */
extern PWM_Controller_t pwm_controller;

/* Function prototypes */

/**
 * @brief Initialize PWM controller and configure timers
 * @param htim1 Pointer to TIM1 handle
 * @param htim4 Pointer to TIM4 handle
 * @return true if initialization successful, false otherwise
 */
bool PWM_Init(TIM_HandleTypeDef *htim1, TIM_HandleTypeDef *htim4);

/**
 * @brief Set PWM value for a specific channel
 * @param channel Channel number (PWM_CH1 to PWM_CH6)
 * @param value PWM value (0-1000, where 1 = 0.1%)
 * @return true if successful, false if invalid parameters
 */
bool PWM_SetValue(PWM_Channel_t channel, uint16_t value);

/**
 * @brief Get current PWM value for a specific channel
 * @param channel Channel number (PWM_CH1 to PWM_CH6)
 * @return Current PWM value (0-1000), or 0 if invalid channel
 */
uint16_t PWM_GetValue(PWM_Channel_t channel);

/**
 * @brief Enable PWM output on a specific channel
 * @param channel Channel number (PWM_CH1 to PWM_CH6)
 * @return true if successful, false if invalid channel
 */
bool PWM_Enable(PWM_Channel_t channel);

/**
 * @brief Disable PWM output on a specific channel
 * @param channel Channel number (PWM_CH1 to PWM_CH6)
 * @return true if successful, false if invalid channel
 */
bool PWM_Disable(PWM_Channel_t channel);

/**
 * @brief Check if channel is enabled
 * @param channel Channel number (PWM_CH1 to PWM_CH6)
 * @return true if enabled, false if disabled or invalid channel
 */
bool PWM_IsEnabled(PWM_Channel_t channel);

/**
 * @brief Set PWM value for all channels
 * @param value PWM value (0-1000)
 */
void PWM_SetAllChannels(uint16_t value);

/**
 * @brief Enable all PWM channels
 */
void PWM_EnableAll(void);

/**
 * @brief Disable all PWM channels
 */
void PWM_DisableAll(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_PWM_CONTROLLER_H_ */
