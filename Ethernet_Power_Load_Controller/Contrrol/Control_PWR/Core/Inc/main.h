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
#define FIRMWARE_VERSION    0x00000008 // Версия
#define FIRMWARE_NAME       "Control PWR"  // Название устройства
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

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart6;
extern DMA_HandleTypeDef hdma_usart1_rx;
extern DMA_HandleTypeDef hdma_usart6_rx;
extern DMA_HandleTypeDef hdma_usart6_tx;
extern SPI_HandleTypeDef hspi3;
/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

#define START_ADR_DEV 1
#define MAX_ADR_DEV 16 // 16 with 0
#define MAX_CH_NAME MAX_ADR_DEV * 6 //

#define ARRAY_LEN(x)            (sizeof(x) / sizeof((x)[0]))

#define DBG_PORT huart6
#define DBG_PORT_NAME USART6
//#define LOG_TX_BUF_SIZE 1024*4

#define CURENT_VERSION 46
#define ID_CTRL 1
#define NAME FIRMWARE_NAME
#define LWIP_DHCP 1

#define UART6_RX_LENGTH 512
#define message_RX_LENGTH 512

#define LOG_ERR "Err: "      // Вместо "Error: "
#define LOG_WARN "Warn: "    // Вместо "Warning: "
#define LOG_OK "OK"          // Вместо "Success" или "OK: "

// обмен данными с другими платами
#define UART_RX_LENGTH 256 + 5 // addres, cmd, size, crc*2
#define UART_TX_LENGTH 256 + 5 // addres, cmd, size, crc*2
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
#define MAC_b0_Pin GPIO_PIN_8
#define MAC_b0_GPIO_Port GPIOD
#define MAC_b1_Pin GPIO_PIN_9
#define MAC_b1_GPIO_Port GPIOD
#define MAC_b2_Pin GPIO_PIN_10
#define MAC_b2_GPIO_Port GPIOD
#define MAC_b3_Pin GPIO_PIN_11
#define MAC_b3_GPIO_Port GPIOD
#define MAC_b4_Pin GPIO_PIN_12
#define MAC_b4_GPIO_Port GPIOD
#define MAC_b5_Pin GPIO_PIN_13
#define MAC_b5_GPIO_Port GPIOD
#define MAC_b6_Pin GPIO_PIN_14
#define MAC_b6_GPIO_Port GPIOD
#define MAC_b7_Pin GPIO_PIN_15
#define MAC_b7_GPIO_Port GPIOD
#define SPI3_CS_Pin GPIO_PIN_15
#define SPI3_CS_GPIO_Port GPIOA
#define WP_Pin GPIO_PIN_0
#define WP_GPIO_Port GPIOD
#define HOLD_Pin GPIO_PIN_1
#define HOLD_GPIO_Port GPIOD
#define DE_M_Pin GPIO_PIN_4
#define DE_M_GPIO_Port GPIOD

/* USER CODE BEGIN Private defines */

#define MAX_CELLS 6

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
  secondary_mode_t mode; 			// режим 2 значит мы в приложении 1 значит в загрузчике
  uint32_t reserved; 		// зарезервировано
} secondary_status_t;

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

typedef enum {
	NoInit      = 0,
	LED_DRV     = 10,
	LED_DRV_v2  = 11,
	PCB_PWR     = 20,
	MOSFET_3CH  = 30,   // chanels_pwr: 3-канальная плата (STM32F030, 8-bit UART, count_ch=3)
	MOSFET_6CH  = 31,   // зарезервировано (не используется)
}PCBType;
// Примечание: driverPCB_v2_boot имеет TYPE_PCB=11 → LED_DRV_v2

// Структура заголовка UART протокола: [address][cmd][size]
typedef struct
{
	uint32_t 	PWM;			// шим на каннале
	uint32_t 	PWM_out;		// шим на каннале для передачи
	uint16_t 	Current;		// ток в канале
	uint8_t 	IsOn;			// включился ?
	uint8_t 	On_off;			// включение или выключение канаала
	uint8_t		Name_ch;		// "имя" каннала цыфра в общей системе
	uint8_t		used;			// используется ли этот канал на плате
}chanel;

typedef struct
{
	chanel 		ch[6];
	uint8_t 	Addr;		// адрес платы
	uint8_t 	AddrFromDev;
	uint32_t	ERR_counter;
	uint32_t	last_ERR;
	PCBType 	TypePCB;		// тип платы считывается с самой платы (группы)
}DEV_t;

typedef struct
{
	DEV_t* 		dev;
	uint8_t		Channel_number;	// номер канала в пределах одного i2c
}chName_t;

typedef struct
{
	uint8_t 	ip[4];// = {192, 168, 0, 2};
	uint8_t		mask[4];//  = {255, 255, 255, 0};
	uint8_t 	gateway[4];// = {192, 168, 0, 1};
}setIP_t;

typedef enum {
	RTU = 0,
	STREAMER = 1,
}mode_bridge_t;

typedef struct
{
	mode_bridge_t mode_rs485;
	uint16_t port;
	UART_HandleTypeDef *RS485;
	uint32_t reserv1;
	uint32_t reserv2;
	uint32_t reserv3;
	uint32_t reserv4;
	uint32_t reserv5;

}set_bridge_t;

// Структура заголовка ModbusTCP
typedef struct {
    uint16_t transaction_id;
    uint16_t protocol_id;
    uint16_t length;
    uint8_t unit_id;
} ModbusTCPHeader;

// Структура для хранения измерений времени
typedef struct {
    uint32_t tcp_to_rtu_start;    // Когда получено TCP сообщение
    uint32_t rtu_tx_start;        // Когда отправлено RTU сообщение
    uint32_t rtu_rx_start;        // Когда получен RTU ответ
    uint32_t tcp_tx_start;        // Когда отправлен TCP ответ

    uint32_t tcp_to_rtu_time;     // Время от получения TCP до отправки RTU
    uint32_t rtu_response_time;   // Время, которое устройство затратило на ответ
    uint32_t rtu_to_tcp_time;     // Время от получения RTU до отправки TCP
} timing_info_t;

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
	set_bridge_t bridge_sett;
}settings_t;
#pragma pack(pop)

// Структура для передачи параметров обновления прошивки
typedef struct {
    uint32_t fw_size;         // Размер прошивки
    uint32_t fw_crc;          // CRC прошивки
    uint32_t cell_num;        // номер ячейки
    uint32_t reserved;        // Зарезервировано
} FWUpdateParams;

// структура для хранения состояния ячеек с прошивками
typedef struct {
	uint32_t cell_address;              // Адрес ячейки
	uint32_t fw_size;                   // Размер прошивки
	uint32_t fw_crc;                    // CRC прошивки
	meta_t metadata;                    // Метаданные прошивки в ячейке
	uint32_t load_permission;           // Разрешение на загрузку прошивки в контроллер
} FirmwareUpdateCellState;

typedef struct
{
	FirmwareUpdateCellState cells[MAX_CELLS]; // массив состояний ячеек
	uint32_t active_cell;					  // активная ячейка
	uint32_t crc;							  // CRC структуры
} SecondaryFirmwareConfig;

// Глобальный экземпляр
extern timing_info_t mb_timing;

// Вспомогательная функция для расчета прошедшего времени в миллисекундах
static inline uint32_t get_elapsed_ms(uint32_t start) {
    uint32_t now = HAL_GetTick();
    return (now >= start) ? (now - start) : (UINT32_MAX - start + now);
}

// Расчёт CRC16 для ModbusRTU
uint16_t calculate_crc16(uint8_t *buffer, int length);

// Проверка CRC входящего RTU пакета
uint8_t verify_rtu_crc(uint8_t *rtu_data, int rtu_length);

// Преобразование ModbusTCP в ModbusRTU
int tcp_to_rtu(uint8_t *tcp_data, int tcp_length, uint8_t *rtu_data, uint16_t *transaction_id);

// Преобразование ModbusRTU в ModbusTCP
int rtu_to_tcp(uint8_t *rtu_data, int rtu_length, uint8_t *tcp_data, uint16_t transaction_id);

// Вспомогательная функция для вывода содержимого пакета
void print_packet(uint8_t *data, int length, const char *prefix);

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
extern osMutexId deviceMutexHandle;
extern osMutexId varMutexDevicesHandle;

extern DEV_t devices[MAX_ADR_DEV];
extern chName_t NameCH[MAX_CH_NAME];

// для обработки полученных данных от других устройств
extern DMA_HandleTypeDef hdma_usart1_rx;
extern UART_HandleTypeDef huart1;
extern osMessageQId rxDataUART1Handle;
extern uint8_t UART_tx[];
extern uint8_t UART_rx[];

extern SecondaryFirmwareConfig secondaryConfig;

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
