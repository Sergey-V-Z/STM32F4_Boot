/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2022 STMicroelectronics.
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
#include "cJSON.h"
#include <stdarg.h>
#include <string.h>
#include "stdio.h"
#include "FreeRTOS.h"
#include "cmsis_os.h"
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */
// Константы метаданных
#define FIRMWARE_VERSION    32UL // Версия
#define FIRMWARE_NAME       "6ch_LED_Driver"  // Название устройства
#define METADATA_KEY        0xDEADBEEF  // Ключ для идентификации

// Структура метаданных
typedef struct {
    uint32_t key_start;       // Магическое число (0xDEADBEEF)
    uint32_t version;         // Версия прошивки
    char name_proj[140];   // Название проекта или устройства
    uint32_t reserved;        // Зарезервировано для будущего использования
} meta_t;

// Объявление переменной метаданных
extern const meta_t firmware_metadata;

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

extern UART_HandleTypeDef huart6;
extern DMA_HandleTypeDef hdma_usart6_rx;
extern DMA_HandleTypeDef hdma_usart6_tx;
extern SPI_HandleTypeDef hspi3;
extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim4;
/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

#define ARRAY_LEN(x)            (sizeof(x) / sizeof((x)[0]))

#define DBG_PORT huart6
#define DBG_PORT_NAME USART6

#define CURENT_VERSION 46
#define ID_CTRL 4
#define NAME FIRMWARE_NAME
#define LWIP_DHCP 1

#define UART6_RX_LENGTH 512
#define message_RX_LENGTH 512

#define LOG_ERR "Err: "      // Вместо "Error: "
#define LOG_WARN "Warn: "    // Вместо "Warning: "
#define LOG_OK "OK"          // Вместо "Success" или "OK: "
// NVS key

/*
// Команды API для работы с точками (добавить в enum)
#define CMD_SAVE_POINT     20  // Сохранить текущую позицию как точку
#define CMD_GET_POSITION   21  // Получить текущую позицию в шагах
#define CMD_GOTO_SW0   		22  // Установить текущую позицию
#define CMD_GOTO_SW1   		23  // Установить текущую позицию
#define CMD_GOTO_POINT     24  // Перейти на точку
#define CMD_GET_POINT     25  // Получить позицию в шагах из точки
#define CMD_GOTO_POSITION   27  // Перейти на позицию в шагах
#define CMD_GET_MAX_POSITION 28 // Получить максимальную позицию
#define CMD_GET_MIN_POSITION 29 // Получить минимальную позицию
#define CMD_SET_POINT 30 // Получить минимальную позицию

#define MAX_POINTS			10
*/

// Структура ответа на запрос позиции
struct position_response_t {
    uint32_t current_steps;    // Текущая позиция в шагах
    uint32_t current_point;    // Номер текущей точки
    uint8_t is_calibrated;        // Статус калибровки
};

// Структура для работы с точками
struct points_response_t {
    uint32_t points[10];       // Массив точек
    uint32_t count;            // Количество точек
};
/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
int set_i2c_dev(uint8_t Addr, uint8_t CH, uint8_t Name);
int del_Name_dev(uint8_t Name);
void del_all_dev();
void setRange_i2c_dev(uint8_t startAddres, uint8_t quantity);
void cleanAll_i2c_dev();
uint8_t ReadStraps();
void finishedBlink();
void timoutBlink();

uint16_t usMBCRC16(uint8_t *pucFrame, uint16_t usLen);

//void STM_LOG(const char* format, ...);
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
#define TCA9548A_RST_Pin GPIO_PIN_9
#define TCA9548A_RST_GPIO_Port GPIOA

/* USER CODE BEGIN Private defines */

#define MAX_CELLS 6

typedef enum {
	NoInit = 0,
	LED_DRV = 10,
    LED_DRV_v2 = 11,
	PCB_PWR = 20,
	MOSFET_6CH = 30,
}PCBType;

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

/*
typedef enum {
	BOOTLOADER_STATE_INIT, // Инициализация загрузчика
	BOOTLOADER_STATE_LOAD, // загрузка прошивки через UART
	BOOTLOADER_STATE_ERROR, // ошибка загрузки
	BOOTLOADER_STATE_JMP_APP // можно переходить в приложение.
} s_Bootloader_State_t;


typedef struct __attribute__((packed, aligned(4))){
	struct s_Bootloader_Status_t {
		s_Bootloader_State_t State; // Current state of bootloader
		uint32_t ErrorCode; // Error code if any
		uint8_t IsAppValid; // Is the application valid
		uint8_t reserved; 
		uint32_t LastFlashAddress; // Last address flashed
		uint32_t BytesFlashed; // Number of bytes flashed
	} status;

	meta_t metadata;

	uint32_t flashInitialized;
	uint32_t reset_counter;
	uint32_t structCRC; // CRC of this structure (except structCRC field)

} s_boot_data_t;
*/

typedef enum {
    FIRMWARE_STATUS_READY = 0x52454144, // Подтверждение выполнения команды (READ) 
    FIRMWARE_STATUS_IDLE = 0x49444C45, // Ожидание начала прошивки (IDLE)
    FIRMWARE_STATUS_ERASING = 0x45524153, // очистка флешки (ERAS)
    FIRMWARE_STATUS_ERASED = 0x45525344,   // очистка флешки закончена (ERSD)
    FIRMWARE_STATUS_WRITING = 0x57524954,  // запись во флешку (WRIT)
    FIRMWARE_STATUS_WRITTEN = 0x57525444,  // запись во флешку закончена (WRTD)
    FIRMWARE_STATUS_VERIFYING = 0x56455246, // завершение , проверка прошивки (VERF)
    FIRMWARE_STATUS_VERIFIED = 0x56524644, // проверка прошивки прошла успешно (VRFD)
    FIRMWARE_STATUS_ERROR = 0x4552524F // ошибка (ERRO)
} s_status_flash_t;

// структура получаемых данных от основного контроллера
typedef struct {
	uint32_t firmwareSize;// размера прошивки
	uint32_t firmwareCRC;// CRC прошивки
	uint32_t firmwareVersion;// версия прошивки
	uint32_t type_pcb; // тип платы
	 uint32_t reserved1; // зарезервировано для будущего использования
	uint32_t reserved2; // зарезервировано для будущего использования
	uint32_t reserved3; // зарезервировано для будущего использования
  	uint8_t name_proj[140]; // название проекта или устройства
} Firmware_data_t;

typedef enum {
	MODE_NON = 0x00000000,
	MODE_BOOTLOADER = 0x00000001,
	MODE_APP = 0x00000002
} secondary_mode_t;

typedef struct{
  uint32_t key; 			// ключ
  uint32_t type_pcb;    	// тип платы
  uint32_t count_ch;   		// количество каналов
  s_status_flash_t status; 	// статус
  secondary_mode_t mode; 	// режим 2 значит мы в приложении 1 значит в загрузчике
  uint32_t reserved; 		// зарезервировано
} secondary_status_t;

typedef struct
{
	uint8_t 	ip[4];// = {192, 168, 0, 2};
	uint8_t		mask[4];//  = {255, 255, 255, 0};
	uint8_t 	gateway[4];// = {192, 168, 0, 1};
}setIP_t;

#pragma pack(push, 1)
typedef struct
{
	uint8_t devices_depth;
	uint8_t	MAC[6];
	uint8_t isON_from_settings;
	uint8_t IP_end_from_settings;
	setIP_t	saveIP;
	uint8_t DHCPset;
	uint8_t version;
}settings_t;
#pragma pack(pop)

// Структура для калибровочных данных (сохраняется отдельно от settings_t)
#pragma pack(push, 1)
typedef struct
{
	uint32_t magic_key;           // Магическое число для проверки валидности (0xCAFEBABE)
	float current_zero_offset;    // Калибровочное смещение нуля (в амперах)
	uint32_t reserved1;           // Резерв для будущих калибровок
	uint32_t reserved2;           // Резерв
	uint32_t crc;                 // CRC структуры
}calibration_t;
#pragma pack(pop)

#define CALIBRATION_MAGIC 0xCAFEBABE  // Магическое число для калибровки

// Структура для передачи параметров обновления прошивки
typedef struct {
    uint32_t fw_size;         // Размер прошивки
    uint32_t fw_crc;          // CRC прошивки
    uint32_t cell_num;        // номер ячейки
    uint32_t reserved;        // Зарезервировано
} FWUpdateParams;

// Максимальное количество ячеек прошивок
#define MAX_CELLS 6

// Структура состояния ячейки прошивки
typedef struct {
    uint32_t cell_address;              // Адрес ячейки
    uint32_t fw_size;                   // Размер прошивки
    uint32_t fw_crc;                    // CRC прошивки
    meta_t metadata;                    // Метаданные прошивки в ячейке
    uint32_t load_permission;           // Разрешение на загрузку прошивки в контроллер
} FirmwareUpdateCellState;

// Структура конфигурации всех ячеек прошивок
typedef struct {
    FirmwareUpdateCellState cells[MAX_CELLS]; // массив состояний ячеек
    uint32_t active_cell;                     // активная ячейка
    uint32_t crc;                             // CRC структуры
} SecondaryFirmwareConfig;

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
