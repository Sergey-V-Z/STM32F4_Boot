/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stm32f4xx.h"
#include "bootloader.h"
#include "spi_flash.h"
#include "flash_utils.h"
#include "crc.h"
#include "string.h"
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */
typedef enum dir
{
    CW = 0,
    CCW = 1,
    END_OF_LIST = 3
}dir;

typedef enum motor_t
{
	stepper_motor = 0,
    bldc = 1
}motor_t;

// Режимы работы
typedef enum mode_rotation_t {
    bldc_inf = 0,         // Бесконечное вращение BLDC
	bldc_limit = 1,           // Бесконечное вращение без энкодера

    step_inf = 2,       // По счетчику с таймера

	//step_by_meter_timer_limit = 3,  // По концевикам с таймером
	//step_by_meter_enc_limit = 4,    // По концевикам с энкодером

	step_by_meter_timer_intermediate = 3,   // С промежуточными остановками по таймеру
	step_by_meter_enc_intermediate = 4,      // С промежуточными остановками по энкодеру

	calibration_timer = 5,							//режим калибровки
	calibration_enc = 6							//режим калибровки
} mode_rotation_t;

typedef struct
{
	uint8_t 	ip[4];// = {192, 168, 0, 2};
	uint8_t		mask[4];//  = {255, 255, 255, 0};
	uint8_t 	gateway[4];// = {192, 168, 0, 1};
}setIP_t;

typedef struct inMessageParam_t
{
	uint16_t 	Start_data;
	uint16_t	Size;
}inMessageParam_t;

// Карта датчиков
typedef struct sensors_map_t {
    uint16_t CW_sensor;     // GPIO_Pin датчика по часовой
    uint16_t CCW_sensor;    // GPIO_Pin датчика против часовой
    uint8_t detected;       // флаг успешного определения датчиков
    uint8_t error_state;    // состояние ошибки датчиков
} sensors_map_t;

// Основные настройки
typedef struct {
    // Сетевые параметры
    uint8_t MAC[6];
    setIP_t saveIP;
    uint8_t DHCPset;

    // Параметры движения
    dir Direct;                  // направление вращения
    mode_rotation_t mod_rotation;// режим вращения
    motor_t res2;              // тип мотора
    uint32_t Speed;             // скорость
    uint32_t StartSpeed;        // начальная скорость
    uint32_t Accel;             // ускорение шагов в милисекунду
    uint32_t Slowdown;          // торможение шагов
    uint32_t res1;
    uint32_t res;

    // Параметры энкодера
    uint32_t stepsENC;          // шаги энкодера между датчиками
    uint32_t stepsENCtoOneStepMotor; // шаги энкодера на шаг мотора

    // Временные параметры
    uint32_t TimeOut;           // таймаут при отсутствии движения

    // Конфигурация датчиков
    sensors_map_t sensors_map;

    // Промежуточные позиции
    uint32_t intermediate_positions[3];  // позиции промежуточных остановок
    uint8_t use_intermediate;            // флаг использования промежуточных остановок

    struct points_map {
        uint32_t points[10];     // массив точек
        uint32_t count;          // количество точек
        uint32_t target_point;   // целевая точка
        uint32_t current_point;  // текущая точка
        uint8_t is_calibrated;      // флаг калибровки
    } points;

    // Системные параметры
    uint8_t version;            // версия конфигурации

} settings_t;

extern SPI_HandleTypeDef hspi3;
extern UART_HandleTypeDef huart2;
extern TIM_HandleTypeDef htim14;
extern W25QXX_Device_t w25qxx_dev;
extern Settings_t settings;
extern DMA_HandleTypeDef hdma_usart2_tx;
extern DMA_HandleTypeDef hdma_usart2_rx;
/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */
// Bootloader key
#define BOOTLOADER_KEY_VALUE                    0xAAAA5555

// Maximum number of reset attempts before entering recovery mode
#define MAX_RESET_COUNT                         3
/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define R_Pin GPIO_PIN_13
#define R_GPIO_Port GPIOC
#define G_Pin GPIO_PIN_14
#define G_GPIO_Port GPIOC
#define B_Pin GPIO_PIN_15
#define B_GPIO_Port GPIOC
#define SPI3_CS_Pin GPIO_PIN_15
#define SPI3_CS_GPIO_Port GPIOA
#define WP_Pin GPIO_PIN_0
#define WP_GPIO_Port GPIOD
#define HOLD_Pin GPIO_PIN_1
#define HOLD_GPIO_Port GPIOD

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
