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
#define FIRMWARE_VERSION    0x00000005 // Версия
#define FIRMWARE_NAME       "Control PWR"  // Название устройства
#define METADATA_KEY        0xDEADBEEF  // Ключ для идентификации

// Структура метаданных
typedef struct {
    uint32_t key_start;       // Магическое число (0xDEADBEEF)
    uint32_t version;         // Версия прошивки
    uint8_t name_proj[140];   // Название проекта или устройства
    uint32_t reserved;        // Зарезервировано для будущего использования
} meta_t;

// Объявление переменной метаданных
extern const meta_t firmware_metadata;

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */
static const uint16_t crc16_table[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
    0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
    0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
    0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
    0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
    0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
    0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
    0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
    0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
    0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
};

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart6;
extern DMA_HandleTypeDef hdma_usart1_rx;
extern DMA_HandleTypeDef hdma_usart6_rx;
extern DMA_HandleTypeDef hdma_usart6_tx;
extern SPI_HandleTypeDef hspi3;
/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

#define START_ADR_I2C 1
#define MAX_ADR_DEV 16 // 16 with 0
#define MAX_CH_NAME MAX_ADR_DEV * 6 //

#define ARRAY_LEN(x)            (sizeof(x) / sizeof((x)[0]))

#define DBG_PORT huart6
#define DBG_PORT_NAME USART6
//#define LOG_TX_BUF_SIZE 1024*4

#define CURENT_VERSION 46
#define ID_CTRL 1
#define NAME "pwr controller"
#define LWIP_DHCP 1

#define UART6_RX_LENGTH 512
#define message_RX_LENGTH 512

#define LOG_ERR "Err: "      // Вместо "Error: "
#define LOG_OK "OK"          // Вместо "Success" или "OK: "

// обмен данными с другими платами
#define UART_RX_LENGTH 256
#define UART_TX_LENGTH 256
// NVS key

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
typedef enum cmd_t
{
	data = 0,
	clear_flash,
	w_firmware_data,
	r_status,
	r_metadata_curent,
	r_metadata_next,
	r_boot_data,
	fin_write,

}cmd_t;

typedef enum status_flash_t
{
	idle = 0,
	write,
	ready,

}status_flash_t;

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
	NoInit = 0,
	LED_DRV = 10,
    LED_DRV_v2 = 11,
	RELE = 20,
	MOSFET_6CH = 30,
}PCBType;

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
	//DEV_t devices[MAX_ADR_DEV]; что бы не сохранять на флешку
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
extern DEV_t devices[MAX_ADR_DEV];
extern chName_t NameCH[MAX_CH_NAME];

// для обработки полученных данных от других устройств
extern DMA_HandleTypeDef hdma_usart1_rx;
extern UART_HandleTypeDef huart1;
extern osMessageQId rxDataUART1Handle;
extern uint8_t UART_tx[];
extern uint8_t UART_rx[];

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
