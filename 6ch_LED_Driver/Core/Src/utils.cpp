/*
 * utils.cpp
 *
 *  Created on: 10 сент. 2022 г.
 *      Author: Ierixon-HP
 */


#include "utils.h"
#include <cmath>

using std::round;

void enableTimerClock(TIM_TypeDef* tim)
{
    if (tim == TIM1) {
        __HAL_RCC_TIM1_CLK_ENABLE();
    } else if (tim == TIM2) {
        __HAL_RCC_TIM2_CLK_ENABLE();
    } else if (tim == TIM3) {
        __HAL_RCC_TIM3_CLK_ENABLE();
    } else if (tim == TIM4) {
        __HAL_RCC_TIM4_CLK_ENABLE();
    } else if (tim == TIM6) {
        __HAL_RCC_TIM6_CLK_ENABLE();
    } else if (tim == TIM7) {
        __HAL_RCC_TIM7_CLK_ENABLE();
    } else if (tim == TIM8) {
        __HAL_RCC_TIM8_CLK_ENABLE();
    }
}

void setTimerChannelValue(TIM_TypeDef* tim, uint32_t channel, uint32_t value)
{
    switch (channel) {
        case TIM_CHANNEL_1:
            tim->CCR1 = value;
            break;
        case TIM_CHANNEL_2:
            tim->CCR2 = value;
            break;
        case TIM_CHANNEL_3:
            tim->CCR3 = value;
            break;
        case TIM_CHANNEL_4:
            tim->CCR4 = value;
        break;
    }
}

uint32_t getTimerChannelValue(TIM_TypeDef* tim, uint32_t channel)
{
    uint32_t value = 0;
    switch (channel) {
        case TIM_CHANNEL_1:
            value = tim->CCR1;
            break;
        case TIM_CHANNEL_2:
            value = tim->CCR2;
            break;
        case TIM_CHANNEL_3:
            value = tim->CCR3;
            break;
        case TIM_CHANNEL_4:
            value = tim->CCR4;
            break;
    }

    return value;
}

IRQn_Type getIRQCode(TIM_TypeDef* tim, uint32_t interruptType)
{
    IRQn_Type irqCode;
    if (tim == TIM1) {
        if (interruptType == TIM_IT_UPDATE) {
            irqCode = TIM1_UP_TIM10_IRQn;
        } else if (interruptType == TIM_IT_CC1 || interruptType == TIM_IT_CC2 || interruptType == TIM_IT_CC3 || interruptType == TIM_IT_CC4) {
            irqCode = TIM1_CC_IRQn;
        }
    } else if (tim == TIM2) {
        irqCode = TIM2_IRQn;
    } else if (tim == TIM3) {
        irqCode = TIM3_IRQn;
    } else if (tim == TIM4) {
        irqCode = TIM4_IRQn;
    } else if (tim == TIM6) {
        irqCode = TIM6_DAC_IRQn;
    } else if (tim == TIM7) {
        irqCode = TIM7_IRQn;
    } else if (tim == TIM8) {
        if (interruptType == TIM_IT_UPDATE) {
            irqCode = TIM8_UP_TIM13_IRQn;
        } else if (interruptType == TIM_IT_CC1 || interruptType == TIM_IT_CC2 || interruptType == TIM_IT_CC3 || interruptType == TIM_IT_CC4) {
            irqCode = TIM8_CC_IRQn;
        }
    }

    return irqCode;
}

uint32_t getTimInterruptTypeCode(uint32_t channel)
{
    uint32_t value = 0;
    switch (channel) {
        case TIM_CHANNEL_1:
            value = TIM_IT_CC1;
            break;
        case TIM_CHANNEL_2:
            value = TIM_IT_CC2;
            break;
        case TIM_CHANNEL_3:
            value = TIM_IT_CC3;
            break;
        case TIM_CHANNEL_4:
            value = TIM_IT_CC3;
        break;
    }
    return value;
}

void enableGPIOForTimer(GPIO_TypeDef* gpio, uint32_t gpioPins,

uint32_t alternativeFunctionCode)
{
    if (gpio == GPIOA) {
        __HAL_RCC_GPIOA_CLK_ENABLE();
    } else if (gpio == GPIOB) {
        __HAL_RCC_GPIOB_CLK_ENABLE();
    } else if (gpio == GPIOC) {
        __HAL_RCC_GPIOC_CLK_ENABLE();
    } else if (gpio == GPIOD) {
        __HAL_RCC_GPIOD_CLK_ENABLE();
    } else if (gpio == GPIOE) {
        __HAL_RCC_GPIOE_CLK_ENABLE();
    } else if (gpio == GPIOF) {
        __HAL_RCC_GPIOF_CLK_ENABLE();
    }

    GPIO_InitTypeDef gpioInit;
    gpioInit.Pin = gpioPins;
    gpioInit.Mode = GPIO_MODE_AF_PP;
    gpioInit.Pull = GPIO_NOPULL;
    gpioInit.Speed = GPIO_SPEED_FREQ_LOW;

    // code of the alternative function can be found in the stm32f3datasheet.pdf
    gpioInit.Alternate = alternativeFunctionCode;
    HAL_GPIO_Init(gpio, &gpioInit);
}

void initLeds()
{
    __HAL_RCC_GPIOE_CLK_ENABLE();
    GPIO_InitTypeDef gpioInit;
    gpioInit.Pin = 0xFF00;
    gpioInit.Mode = GPIO_MODE_OUTPUT_PP;
    gpioInit.Pull = GPIO_NOPULL;
    gpioInit.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOE, &gpioInit);
    HAL_GPIO_WritePin(GPIOE, 0xFF00, GPIO_PIN_RESET);
}

void setIndicatorPosition(float angle)
{
    // convert angle to pin number
    volatile int8_t pinNo = round(angle / 45.0f);
    pinNo += 5;

    while (pinNo < 0) {
        pinNo++;
    }

    while (pinNo > 8) {
        pinNo--;
    }

    pinNo += 8;

    // switch leds
    GPIOE->ODR = ((GPIOE->ODR) & 0x00FFU) | (0x0001U << pinNo);
}

