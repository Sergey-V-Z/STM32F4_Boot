/*
 * utils.h
 *
 *  Created on: 10 сент. 2022 г.
 *      Author: Ierixon-HP
 */

#ifndef INC_UTILS_H_
#define INC_UTILS_H_


#include "stm32f4xx_hal.h"

//----------------------------------------------------------------
// Helper function to control timers
//----------------------------------------------------------------

/**
 * Enable timer clock
 */
void enableTimerClock(TIM_TypeDef* tim);

/**
 * Set value of the compare/capture register for channel of the specified timer.
 */

void setTimerChannelValue(TIM_TypeDef* tim, uint32_t channel, uint32_t value);

/**
 * Get value of the compare/capture register for channel of the specified timer.
 */
uint32_t getTimerChannelValue(TIM_TypeDef* tim, uint32_t channel);

/**
 * Enable specified GPIO timer output
 */
void enableGPIOForTimer(GPIO_TypeDef* GPIOx, uint32_t gpioPins,

uint32_t alternativeFunctionCode);

/**
 * Get interrupt code
 *
 * @param tim - timer instance
 * @param interruptType  - TIM_IT_UPDATE, TIM_IT_CC1, TIM_IT_CC2, TIM_IT_CC3 or TIM_IT_CC4
 */
IRQn_Type getIRQCode(TIM_TypeDef* tim, uint32_t interruptType);

/**
 * Get interrupt code of the corresponding timer channel
 *
 * @param channel - TIM_CHANNEL_1, TIM_CHANNEL_2, TIM_CHANNEL_3 or TIM_CHANNEL_4
 * @return - TIM_IT_CC1, TIM_IT_CC2, TIM_IT_CC3 or TIM_IT_CC4
 */
uint32_t getTimInterruptTypeCode(uint32_t channel);

//----------------------------------------------------------------
// Other helper functions
//----------------------------------------------------------------
void systemClockConfig();
void initLeds();
void setIndicatorPosition(float angle);

#endif /* INC_UTILS_H_ */
