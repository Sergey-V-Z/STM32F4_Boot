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
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include "stdio.h"
#include "FreeRTOS.h"
#include "cmsis_os.h"

// Forward declarations для избежания циклических зависимостей
#ifdef __cplusplus
class led_t;
class flash;
#else
typedef struct led_t led_t;
typedef struct flash flash;
#endif

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

// Константы метаданных
#define FIRMWARE_VERSION    0x00000001 // Версия
#define FIRMWARE_NAME       "6ch_LED_Driver"  // Название устройства
#define METADATA_KEY        0xDEADBEEF  // Ключ для идентификации

// Константы
#define MAX_CELLS 6

// Структура метаданных
typedef struct {
    uint32_t key_start;       // Магическое число (0xDEADBEEF)
    uint32_t version;         // Версия прошивки
    char name_proj[140];      // Название проекта или устройства
    uint32_t reserved;        // Зарезервировано для будущего использования
} meta_t;

// Объявление переменной метаданных
extern const meta_t firmware_metadata;

// Структура для IP настроек
typedef struct
{
	uint8_t 	ip[4];        // IP адрес
	uint8_t		mask[4];      // Маска сети
	uint8_t 	gateway[4];   // Шлюз
} setIP_t;

// Структура настроек устройства
#pragma pack(push, 1)
typedef struct
{
	uint8_t devices_depth;            // Глубина иерархии устройств (не используется)
	uint8_t	MAC[6];                   // MAC адрес
	uint8_t isON_from_settings;       // Включено из настроек
	uint8_t IP_end_from_settings;     // Последний октет IP из настроек
	setIP_t	saveIP;                   // Сохраненные IP настройки
	uint8_t DHCPset;                  // Включен DHCP
	uint8_t version;                  // Версия настроек
	uint32_t reserved[8];             // Зарезервировано (вместо bridge_sett)
} settings_t;
#pragma pack(pop)

// Максимальный размер сообщения
#define MAX_MESSAGE_SIZE 1536
#define QUEUE_SIZE 8

// Структура сообщения
typedef struct {
    char data[MAX_MESSAGE_SIZE];
    uint16_t length;
} LogMessage_t;

// Структура логгера
typedef struct {
    UART_HandleTypeDef* huart;
    osMessageQId messageQueue;
    char* txBuffer;
    volatile uint8_t isTransmitting;
    uint8_t started;
    volatile uint8_t currentMsgIndex;
} UartLogger_t;

typedef struct {
    uint8_t totalSlots;
    uint8_t usedSlots;
    uint8_t freeSlots;
    uint8_t isTransmitting;
    uint32_t queueLength;
} LoggerStats_t;

// Типы команд для обмена данными
typedef enum cmd_t
{
	data = 0,
	firmware_data,
	status,
	metadata_current,
	boot_data,
	fin_write,
  	prepare_write,
  	enter_boot_mode,
} cmd_t;

// Статусы прошивки
typedef enum {
    FIRMWARE_STATUS_READY = 0x52454144,
    FIRMWARE_STATUS_IDLE = 0x49444C45,
    FIRMWARE_STATUS_ERASING = 0x45524153,
    FIRMWARE_STATUS_ERASED = 0x45525344,
    FIRMWARE_STATUS_WRITING = 0x57524954,
    FIRMWARE_STATUS_WRITTEN = 0x57525444,
    FIRMWARE_STATUS_VERIFYING = 0x56455246,
    FIRMWARE_STATUS_VERIFIED = 0x56524644,
    FIRMWARE_STATUS_ERROR = 0x4552524F
} s_status_flash_t;

// Типы печатных плат
typedef enum {
	NoInit = 0,
	LED_DRV = 10,
    LED_DRV_v2 = 11,
	PCB_PWR = 20,
	MOSFET_6CH = 30,
} PCBType;

// Структура для передачи данных прошивки
typedef struct {
	uint32_t firmwareSize;
	uint32_t firmwareCRC;
	uint32_t firmwareVersion;
	uint32_t type_pcb;
	uint32_t reserved1;
	uint32_t reserved2;
	uint32_t reserved3;
  	uint8_t name_proj[140];
} Firmware_data_t;

// Режимы работы
typedef enum {
	MODE_NON = 0x00000000,
	MODE_BOOTLOADER = 0x00000001,
	MODE_APP = 0x00000002
} secondary_mode_t;

// Структура состояния ячейки с прошивкой
typedef struct {
	uint32_t cell_address;
	uint32_t fw_size;
	uint32_t fw_crc;
	meta_t metadata;
	uint32_t load_permission;
} FirmwareUpdateCellState;

typedef struct
{
	FirmwareUpdateCellState cells[MAX_CELLS];
	uint32_t active_cell;
	uint32_t crc;
} SecondaryFirmwareConfig;

// Структуры для управления PWM каналами
#pragma pack(push, 1)
typedef struct{
	uint8_t en1;
	uint8_t en2;
	uint8_t	en3;
	uint8_t	en4;
	uint8_t	en5;
	uint8_t	en6;

	uint32_t PWM1;
	uint32_t PWM2;
	uint32_t PWM3;
	uint32_t PWM4;
	uint32_t PWM5;
	uint32_t PWM6;
} pwm_ch_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
	uint8_t en1;
	uint8_t en2;
	uint8_t	en3;
	uint8_t	en4;
	uint8_t	en5;
	uint8_t	en6;

	uint16_t ADC_CH1;
	uint16_t ADC_CH2;
	uint16_t ADC_CH3;
	uint16_t ADC_CH4;
	uint16_t ADC_CH5;
	uint16_t ADC_CH6;
	uint16_t ADC_Termo;

	uint32_t PWM1;
	uint32_t PWM2;
	uint32_t PWM3;
	uint32_t PWM4;
	uint32_t PWM5;
	uint32_t PWM6;
} ret_pwm_ch_t;
#pragma pack(pop)

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

extern UART_HandleTypeDef huart6;
extern DMA_HandleTypeDef hdma_usart6_rx;
extern DMA_HandleTypeDef hdma_usart6_tx;
extern SPI_HandleTypeDef hspi3;

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

#define CURENT_VERSION 1
#define ID_CTRL 1
#define NAME "6ch LED Driver"
#define LWIP_DHCP 1

#define DBG_PORT huart6
#define DBG_PORT_NAME USART6

#define LOG_ERR "Err: "
#define LOG_WARN "Warn: "
#define LOG_OK "OK"

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

// Функции инициализации и работы с логгером
void freeSlotAtomic(uint8_t slot);
int getFreeSlotAtomic(void);
void Logger_Init(UART_HandleTypeDef* huart);
void Logger_Process(void);
void Logger_TxCpltCallback(void);
void Logger_Log(const char* format, ...);
void Logger_Log_xx(const char* format, ...);
void Logger_Stop(void);
void Logger_GetStats(LoggerStats_t* stats);

// Макрос для логирования
#define STM_LOG(...) Logger_Log(__VA_ARGS__)
#define STM_LOG_xx(...) Logger_Log_xx(__VA_ARGS__)

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define MAC_IP_Pin_Pin GPIO_PIN_2
#define MAC_IP_Pin_GPIO_Port GPIOE
#define R_Pin GPIO_PIN_13
#define R_GPIO_Port GPIOC
#define G_Pin GPIO_PIN_14
#define G_GPIO_Port GPIOC
#define B_Pin GPIO_PIN_15
#define B_GPIO_Port GPIOC
#define eth_NRST_Pin GPIO_PIN_0
#define eth_NRST_GPIO_Port GPIOA
#define SPI3_CS_Pin GPIO_PIN_15
#define SPI3_CS_GPIO_Port GPIOA
#define WP_Pin GPIO_PIN_0
#define WP_GPIO_Port GPIOD
#define HOLD_Pin GPIO_PIN_1
#define HOLD_GPIO_Port GPIOD
#define DE_M_Pin GPIO_PIN_4
#define DE_M_GPIO_Port GPIOD

/* USER CODE BEGIN Private defines */

// Глобальные переменные
extern uint32_t count_tic;

// LED индикаторы
extern led_t LED_IPadr;
extern led_t LED_error;
extern led_t LED_OSstart;

// SPI Flash
extern flash mem_spi;
extern settings_t settings;
extern bool resetSettings;

// Глобальный экземпляр логгера
extern UartLogger_t logger;

// Локальный буфер для DMA передачи
extern char txBuffer[MAX_MESSAGE_SIZE];

// Пул сообщений и буфер для него
extern LogMessage_t messagePool[QUEUE_SIZE];
extern uint8_t messagePoolUsed[QUEUE_SIZE];
extern osMutexId poolMutexHandle;

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
