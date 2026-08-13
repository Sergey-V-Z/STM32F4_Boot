/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * File Name          : freertos.c
 * Description        : Code for freertos applications
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

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "flash_spi.h"
#include "LED.h"
#include "lwip.h"
#include "adc.h"
using namespace std;
#include <string>
#include "api.h"
#include <iostream>
#include <vector>
#include "device_API.h"
#include <iomanip>
#include "tcp_server.h"
#include "firmware_update.h"
#include "protocol.h"
#include "pwm_controller.h"
#include "i2c.h"
#include "tca9548a.h"
#include "dac_settings.h"
#include "uart_link.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

extern settings_t settings;
extern ADC_HandleTypeDef hadc1;  // АЦП handle для работы с DMA
extern flash mem_spi;  // Flash память для сохранения калибровки

// Адрес во flash для калибровочных данных (отдельно от settings)
// ВАЖНО: Должен быть в отдельном секторе! W25Qxx сектор = 4KB (0x1000)
// Сектор 0: 0x0000-0x0FFF (SPI_FLASH_CONFIG_ADDRESS для settings)
// Сектор 1: 0x1000-0x1FFF 
// Сектор 2: 0x2000-0x2FFF (используем для калибровки)
#define CALIBRATION_FLASH_ADDR  0x2000  // Адрес в памяти (сектор 2)

uint16_t sensBuff[8] = {0};
uint8_t sensState = 255; // битовое поле

uint32_t freqSens = HAL_RCC_GetHCLKFreq()/30000u;
uint32_t pwmSens;

uint16_t adc_buffer[24] = {0};
uint16_t adc_buffer2[24] = {0};

// ACS712 current sensor variables - DMA mode
#define ADC_SAMPLES 16  // Количество измерений для усреднения
#define CURRENT_FILTER_ALPHA 0.15f  // Коэффициент экспоненциального фильтра (0.0-1.0)
                                    // Меньше = плавнее, но медленнее реакция
                                    // Больше = быстрее реакция, но меньше фильтрация
uint16_t adc_dma_buffer[ADC_SAMPLES] = {0};  // Буфер для DMA
volatile uint8_t adc_conversion_complete = 0;  // Флаг завершения конверсии
float g_current_amperes = 0.0f;  // Глобальная переменная для хранения тока в амперах
float g_current_filtered = 0.0f; // Фильтрованное значение тока (EMA фильтр)
volatile uint8_t g_current_filter_initialized = 0;  // Флаг инициализации фильтра
float g_current_zero_offset = 0.0f;  // Калибровочное смещение нуля (в амперах)

extern led_t LED_IPadr;
extern led_t LED_error;
extern led_t LED_OSstart;

void action_ip(cJSON *obj, bool save);
void action_settings_data(cJSON *obj, struct netconn *tcp_conn = nullptr);
void action_commands(cJSON *obj, struct netconn *tcp_conn = nullptr);
//структуры для netcon
extern struct netif gnetif;

//TCP_IP
string strIP;
string in_str;

// обмен данными с компом по UART6 — см. uart_link.cpp/uart_link.h

//переменные переферии
uint32_t Start = 0;
extern flash mem_spi;

SecondaryFirmwareConfig secondaryConfig;

//переменные для тестов

uint8_t txRedy = 1;

/* USER CODE END Variables */
osThreadId MainTaskHandle;
osThreadId LEDHandle;
osThreadId ethTasHandle;
osThreadId MBRTUTaskHandle;
osThreadId MBETHTaskHandle;
osThreadId uart_taskHandle;
osThreadId loggerTaskHandle;
osMessageQId rxDataUART6Handle;
osMessageQId rxDataUART1Handle;
osSemaphoreId ADC_endHandle;
osSemaphoreId ADC_end2Handle;
osSemaphoreId Resive_USARTHandle;
osSemaphoreId mulicom_uartHandle;
osMutexId dacI2cMutexHandle; // защищает "выбор канала TCA9548A + транзакция MCP4725" (см. tca9548a.h)

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
osThreadId fadeTasHandle;
// extern "C"

// Функции для работы с датчиком тока ACS712 (DMA режим)
void ACS712_StartDMA_Conversion(void);
float ACS712_ProcessData(void);
uint16_t ACS712_MovingAverage(uint16_t* data, uint16_t size);
void ACS712_CalibrateZero(void);  // Калибровка нуля

// Функции для сохранения/загрузки калибровки во flash
void Calibration_Save(void);
void Calibration_Load(void);
uint32_t Calibration_CalculateCRC(calibration_t* cal);

void fadeTask(void const * argument);
/* USER CODE END FunctionPrototypes */

void mainTask(void const * argument);
void led(void const * argument);
void eth_Task(void const * argument);
void mbrtuTask(void const * argument);
void mbethTask(void const * argument);
void uart_Task(void const * argument);
void LoggerTask(void const * argument);

extern void MX_LWIP_Init(void);
void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* GetIdleTaskMemory prototype (linked to static allocation support) */
extern "C" void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize );

/* USER CODE BEGIN GET_IDLE_TASK_MEMORY */
static StaticTask_t xIdleTaskTCBBuffer;
static StackType_t xIdleStack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize )
{
	*ppxIdleTaskTCBBuffer = &xIdleTaskTCBBuffer;
	*ppxIdleTaskStackBuffer = &xIdleStack[0];
	*pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
	/* place for user code */
}
/* USER CODE END GET_IDLE_TASK_MEMORY */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
	/* add mutexes, ... */
	osMutexDef(dacI2cMutex);
	dacI2cMutexHandle = osMutexCreate(osMutex(dacI2cMutex));
  /* USER CODE END RTOS_MUTEX */

  /* Create the semaphores(s) */
  /* definition and creation of ADC_end */
  osSemaphoreDef(ADC_end);
  ADC_endHandle = osSemaphoreCreate(osSemaphore(ADC_end), 1);

  /* definition and creation of ADC_end2 */
  osSemaphoreDef(ADC_end2);
  ADC_end2Handle = osSemaphoreCreate(osSemaphore(ADC_end2), 1);

  /* definition and creation of Resive_USART */
  osSemaphoreDef(Resive_USART);
  Resive_USARTHandle = osSemaphoreCreate(osSemaphore(Resive_USART), 1);

  /* definition and creation of mulicom_uart */
  osSemaphoreDef(mulicom_uart);
  mulicom_uartHandle = osSemaphoreCreate(osSemaphore(mulicom_uart), 1);

  /* USER CODE BEGIN RTOS_SEMAPHORES */
	/* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
	/* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* definition and creation of rxDataUART6 */
  osMessageQDef(rxDataUART6, 16, uint8_t);
  rxDataUART6Handle = osMessageCreate(osMessageQ(rxDataUART6), NULL);

  /* definition and creation of rxDataUART1 */
  osMessageQDef(rxDataUART1, 16, uint16_t);
  rxDataUART1Handle = osMessageCreate(osMessageQ(rxDataUART1), NULL);

  /* USER CODE BEGIN RTOS_QUEUES */
	/* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of MainTask */
  osThreadDef(MainTask, mainTask, osPriorityNormal, 0, 256);
  MainTaskHandle = osThreadCreate(osThread(MainTask), NULL);

  /* definition and creation of LED */
  osThreadDef(LED, led, osPriorityNormal, 0, 128);
  LEDHandle = osThreadCreate(osThread(LED), NULL);

  /* definition and creation of ethTas */
  osThreadDef(ethTas, eth_Task, osPriorityNormal, 0, 768);
  ethTasHandle = osThreadCreate(osThread(ethTas), NULL);

  /* definition and creation of MBRTUTask */
  osThreadDef(MBRTUTask, mbrtuTask, osPriorityNormal, 0, 256);
  MBRTUTaskHandle = osThreadCreate(osThread(MBRTUTask), NULL);

  /* definition and creation of MBETHTask */
  osThreadDef(MBETHTask, mbethTask, osPriorityNormal, 0, 512);
  MBETHTaskHandle = osThreadCreate(osThread(MBETHTask), NULL);

  /* definition and creation of uart_task */
  osThreadDef(uart_task, uart_Task, osPriorityNormal, 0, 1024);
  uart_taskHandle = osThreadCreate(osThread(uart_task), NULL);

  /* definition and creation of loggerTask */
  osThreadDef(loggerTask, LoggerTask, osPriorityNormal, 0, 128);
  loggerTaskHandle = osThreadCreate(osThread(loggerTask), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
	/* add threads, ... */
	osThreadDef(fadeTas, fadeTask, osPriorityNormal, 0, 128);
	fadeTasHandle = osThreadCreate(osThread(fadeTas), NULL);
  /* USER CODE END RTOS_THREADS */

}

/* USER CODE BEGIN Header_mainTask */
/**
 * @brief  Function implementing the MainTask thread.
 * @param  argument: Not used
 * @retval None
 */
/* USER CODE END Header_mainTask */
void mainTask(void const * argument)
{
  /* init code for LWIP */
  MX_LWIP_Init();
  /* USER CODE BEGIN mainTask */

  	//нициализируем логгер
    Logger_Init(&huart6);

    STM_LOG("Start %s app. Ver %d", FIRMWARE_NAME, FIRMWARE_VERSION);

	if (GetSecondaryFirmwareConfig(&secondaryConfig))
	{
		// Конфигурация успешно получена
		STM_LOG("Secondary firmware config OK");
	}
	else
	{
		// Ошибка получения конфигурации
		STM_LOG("Failed to get secondary firmware config");
		ResetSecondaryFirmwareConfig(); // записывает дефолтные значения
		STM_LOG("Secondary firmware config reset");
	}

  	/* Инициализируем модуль обновления прошивки */
  	FirmwareUpdate_Init(&mem_spi);

  	/* Инициализируем TCP-сервер обновления прошивки */
  	FirmwareUpdateServer_Init();

  	/* Запускаем TCP-сервер (он будет работать всегда) */
  	FirmwareUpdateServer_Start();

  	STM_LOG("Firmware updater ready");
  	STM_LOG("PWM controller initialized with 6 channels");
  	STM_LOG("ACS712 current sensor initialized (DMA mode)");
  	
  	// Загружаем калибровочные данные ACS712 из flash
  	Calibration_Load();

  	// Поиск адресов MCP4725 на каждом канале мультиплексора (один раз при старте)
  	DAC_ScanAddresses();

  	// Явно гасим LD на всех каналах: MCP4725 хранит последнее записанное
  	// значение (Fast Write, не EEPROM) и не сбрасывается при ресете STM32 —
  	// без этой команды на выводе LD может остаться остаточное напряжение
  	// с прошлого запуска, и диоды окажутся включены ещё до первой команды.
  	DAC_ChannelOffAll();

  	// Восстанавливаем уставки (percent/raw PWM/fade duration) из flash.
  	// Каналы остаются выключенными (бит включения не персистится, см.
  	// dac_settings.h) — уставки только запоминаются, в ЦАП не пишутся.
  	DAC_Settings_Load();

	/* Infinite loop */
	for(;;)
	{
		// Запускаем преобразование АЦП с DMA (16 измерений канала PA4)
		ACS712_StartDMA_Conversion();
		
		// Ждем завершения конверсии (callback установит флаг и обработает данные)
		uint32_t timeout = 100; // таймаут в мс
		uint32_t start_tick = osKernelSysTick();
		while (!adc_conversion_complete && (osKernelSysTick() - start_tick) < timeout)
		{
			osDelay(1);
		}
		
		// Значение тока уже обработано в callback и сохранено в g_current_amperes
		// Можно использовать для мониторинга или защиты
		
		osDelay(100); // Период опроса 100 мс
	}
  /* USER CODE END mainTask */
}

/* USER CODE BEGIN Header_led */
/**
 * @brief Function implementing the LED thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_led */
void led(void const * argument)
{
  /* USER CODE BEGIN led */
	/* Infinite loop */
	HAL_GPIO_WritePin(R_GPIO_Port, R_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(G_GPIO_Port, G_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(B_GPIO_Port, B_Pin, GPIO_PIN_SET);

	LED_IPadr.Init(G_GPIO_Port, G_Pin);
	LED_error.Init(R_GPIO_Port, R_Pin);
	LED_OSstart.Init(B_GPIO_Port, B_Pin);

	LED_IPadr.setParameters(mode::ON_OFF);
	LED_error.setParameters(mode::ON_OFF);
	LED_OSstart.setParameters(mode::BLINK, 2000, 100);
	LED_OSstart.LEDon();

	//uint32_t tickcount = osKernelSysTick();// переменная для точной задержки
	/* Infinite loop */
	for(;;)
	{
		LED_IPadr.poll();
		LED_error.poll();
		LED_OSstart.poll();

		osDelay(1);
		//taskYIELD();
		//osDelayUntil(&tickcount, 1); // задача будет вызываься ровро через 1 милисекунду
	}
  /* USER CODE END led */
}

/* USER CODE BEGIN Header_eth_Task */
/**
 * @brief Function implementing the ethTas thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_eth_Task */
void eth_Task(void const * argument)
{
  /* USER CODE BEGIN eth_Task */

	while(gnetif.ip_addr.addr == 0){osDelay(1);}	//ждем получение адреса
	LED_IPadr.LEDon();
	osDelay(1000);
	LED_IPadr.LEDoff();
	strIP = ip4addr_ntoa(&gnetif.ip_addr);

	//структуры для netcon
	struct netconn *conn;
	struct netconn *newconn;
	struct netbuf *netbuf;
	volatile err_t err, accept_err;
	//ip_addr_t local_ip;
	//ip_addr_t remote_ip;
	void 		*in_data = NULL;
	uint16_t 		data_size = 0;


	/* Infinite loop */
	for(;;)
	{

		conn = netconn_new(NETCONN_TCP);
		if (conn!=NULL)
		{
			err = netconn_bind(conn,NULL,81);//assign port number to connection
			if (err==ERR_OK)
			{
				netconn_listen(conn);//set port to listening mode
				while(1)
				{
					accept_err=netconn_accept(conn,&newconn);//suspend until new connection
					if (accept_err==ERR_OK)
					{
						LED_IPadr.LEDon();
						STM_LOG("Connect open");
						while ((accept_err=netconn_recv(newconn,&netbuf))==ERR_OK)//работаем до тех пор пока клиент не разорвет соеденение
						{

							do
							{
								netbuf_data(netbuf,&in_data,&data_size);//get pointer and data size of the buffer
								in_str.assign((char*)in_data, data_size);//copy in string
								/*-----------------------------------------------------------------------------------------------------------------------------*/
								STM_LOG("TCP CMD: %s", in_str.c_str());

								if (!in_str.empty()) {
									if (in_str[0] == '{') {
										// JSON protocol (new clients)
										cJSON *j = cJSON_Parse(in_str.c_str());
										if (j != NULL) {
											cJSON *j_id   = cJSON_GetObjectItemCaseSensitive(j, "id");
											cJSON *j_type = cJSON_GetObjectItemCaseSensitive(j, "type_data");
											cJSON *j_save = cJSON_GetObjectItemCaseSensitive(j, "save_settings");
											cJSON *j_obj  = cJSON_GetObjectItemCaseSensitive(j, "obj");
											if (cJSON_IsNumber(j_id)) {
												uint32_t rid = (uint32_t)cJSON_GetNumberValue(j_id);
												if (rid == ID_CTRL || rid == 0) {
													bool save_set = cJSON_IsTrue(j_save);
													if (cJSON_IsNumber(j_type)) {
														switch (j_type->valueint) {
														case 1: action_ip(j_obj, save_set);           break;
														case 3: action_commands(j_obj, newconn);      break;
														case 4: action_settings_data(j_obj, newconn); break;
														default: break;
														}
													}
												}
											}
											cJSON_Delete(j);
										} else {
											STM_LOG("TCP: invalid JSON");
										}
									} else {
										// Text protocol CxxAxDyxNzx (third-party software)
										string resp = Сommand_execution(in_str);
										netconn_write(newconn, resp.c_str(), resp.size(), NETCONN_COPY);
									}
								}

							} while (netbuf_next(netbuf) >= 0);
							netbuf_delete(netbuf);

						}
						netconn_close(newconn);
						netconn_delete(newconn);
						STM_LOG("Connect close");
						LED_IPadr.LEDoff();
					} else netconn_delete(newconn);
					osDelay(20);
				}
			}
		}
		osDelay(1);
	}
  /* USER CODE END eth_Task */
}

/* USER CODE BEGIN Header_mbrtuTask */
/**
* @brief Function implementing the MBRTUTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_mbrtuTask */
void mbrtuTask(void const * argument)
{
  /* USER CODE BEGIN mbrtuTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END mbrtuTask */
}

/* USER CODE BEGIN Header_mbethTask */
/**
* @brief Function implementing the MBETHTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_mbethTask */
void mbethTask(void const * argument)
{
  /* USER CODE BEGIN mbethTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END mbethTask */
}

/* USER CODE BEGIN Header_uart_Task */
/**
* связь с компьютером обработка пришедших сообщений
* @brief Function implementing the uart_task thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_uart_Task */
void uart_Task(void const * argument)
{
  /* USER CODE BEGIN uart_Task */
  // Связь с компьютером по UART6 — приём, разбор JSON и диспетчеризация
  // вынесены в uart_link.cpp (UART_Link_Init/UART_Link_Process)
  UART_Link_Init();

  /* Infinite loop */
  for(;;)
  {
    UART_Link_Process();
    osDelay(10);
  }
  /* USER CODE END uart_Task */
}

/* USER CODE BEGIN Header_LoggerTask */
/**
* @brief Function implementing the loggerTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_LoggerTask */
void LoggerTask(void const * argument)
{
  /* USER CODE BEGIN LoggerTask */
  /* Infinite loop */
  for(;;)
  {
	Logger_Process();
    osDelay(1);
  }
  /* USER CODE END LoggerTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

void fadeTask(void const * argument)
{
    for (;;)
    {
        DAC_FadeTick();
        osDelay(FADE_TICK_MS);
    }
}

// ---------------------------------------------------------------------------
// action_ip / action_commands / action_settings_data — общие обработчики
// JSON-протокола {"id":.., "type_data":N, "obj":{...}}.
//
// Вызываются из ДВУХ мест:
//  - eth_Task (TCP, см. выше) — tcp_conn указывает на активное соединение,
//    ответ отправляется через netconn_write;
//  - uart_Task -> UART_Link_Process (UART6, см. uart_link.cpp) — tcp_conn
//    равен nullptr, ответ уходит обратно на ПК через UART_Link_SendResponse
//    (см. tcp_or_uart_send ниже).
// ---------------------------------------------------------------------------

// Получение настроек сети от хоста
void action_ip(cJSON *obj, bool save)
{
    cJSON *j_IP = cJSON_GetObjectItemCaseSensitive(obj, "IP");
    cJSON *j_setIP = cJSON_GetObjectItemCaseSensitive(obj, "setIP");
    cJSON *j_MAC = cJSON_GetObjectItemCaseSensitive(obj, "MAC");
    cJSON *j_setMAC = cJSON_GetObjectItemCaseSensitive(obj, "setMAC");
    cJSON *j_GATEWAY = cJSON_GetObjectItemCaseSensitive(obj, "GATEWAY");
    cJSON *j_setGATEWAY = cJSON_GetObjectItemCaseSensitive(obj, "setGATEWAY");
    cJSON *j_MASK = cJSON_GetObjectItemCaseSensitive(obj, "MASK");
    cJSON *j_setMASK = cJSON_GetObjectItemCaseSensitive(obj, "setMASK");
    cJSON *j_DNS = cJSON_GetObjectItemCaseSensitive(obj, "DNS");
    cJSON *j_setDNS = cJSON_GetObjectItemCaseSensitive(obj, "setDNS");
    cJSON *j_DHCP = cJSON_GetObjectItemCaseSensitive(obj, "DHCP");
    cJSON *j_setDHCP = cJSON_GetObjectItemCaseSensitive(obj, "setDHCP");

    bool settingsChanged = false;

    // Обработка IP
    if ((j_setIP != NULL) && cJSON_IsTrue(j_setIP) && (j_IP != NULL) && cJSON_IsString(j_IP)) {
        char sep = '.';
        std::string s = j_IP->valuestring;
        if (!s.empty()) {
            std::string sepIP[4];
            int i = 0;
            bool parseSuccess = true;

            for (size_t p = 0, q = 0; (p != s.npos) && (i < 4); p = q, i++) {
                sepIP[i] = s.substr(p + (p != 0),
                        (q = s.find(sep, p + 1)) - p - (p != 0));
            }

            // Проверка, что удалось разделить строку на 4 части
            if (i == 4) {
                uint8_t ipParts[4] = {0};
                for (i = 0; i < 4; i++) {
                    char* endptr = nullptr;
                    long value = strtol(sepIP[i].c_str(), &endptr, 10);

                    // Проверка успешности преобразования и диапазона значений
                    if (*endptr != '\0' || value < 0 || value > 255) {
                        parseSuccess = false;
                        break;
                    }
                    ipParts[i] = (uint8_t)value;
                }

                if (parseSuccess) {
                    // Сохраняем новые значения только если парсинг успешен
                    for (i = 0; i < 4; i++) {
                        settings.saveIP.ip[i] = ipParts[i];
                    }
                    settingsChanged = true;
                } else {
                    STM_LOG(LOG_ERR "Invalid IP address format");
                }
            } else {
                STM_LOG(LOG_ERR "IP address must have 4 octets");
            }
        }
    }

    // Обработка MAC
    if ((j_setMAC != NULL) && cJSON_IsTrue(j_setMAC) && (j_MAC != NULL) && cJSON_IsString(j_MAC)) {
        char sep = ':';
        std::string s = j_MAC->valuestring;
        if (!s.empty()) {
            std::string sepMAC[6];
            int i = 0;
            bool parseSuccess = true;

            for (size_t p = 0, q = 0; (p != s.npos) && (i < 6); p = q, i++) {
                sepMAC[i] = s.substr(p + (p != 0),
                        (q = s.find(sep, p + 1)) - p - (p != 0));
            }

            // Проверка, что удалось разделить строку на 6 частей
            if (i == 6) {
                uint8_t macParts[6] = {0};
                for (i = 0; i < 6; i++) {
                    char* endptr = nullptr;
                    long value = strtol(sepMAC[i].c_str(), &endptr, 16);

                    // Проверка успешности преобразования и диапазона значений
                    if (*endptr != '\0' || value < 0 || value > 255) {
                        parseSuccess = false;
                        break;
                    }
                    macParts[i] = (uint8_t)value;
                }

                if (parseSuccess) {
                    // Сохраняем новые значения только если парсинг успешен
                    for (i = 0; i < 6; i++) {
                        settings.MAC[i] = macParts[i];
                    }
                    settingsChanged = true;
                } else {
                    STM_LOG(LOG_ERR "Invalid MAC address format");
                }
            } else {
                STM_LOG(LOG_ERR "MAC address must have 6 octets");
            }
        }
    }

    // Обработка GATEWAY
    if ((j_setGATEWAY != NULL) && cJSON_IsTrue(j_setGATEWAY) && (j_GATEWAY != NULL) && cJSON_IsString(j_GATEWAY)) {
        char sep = '.';
        std::string s = j_GATEWAY->valuestring;
        if (!s.empty()) {
            std::string sepGATEWAY[4];
            int i = 0;
            bool parseSuccess = true;

            for (size_t p = 0, q = 0; (p != s.npos) && (i < 4); p = q, i++) {
                sepGATEWAY[i] = s.substr(p + (p != 0),
                        (q = s.find(sep, p + 1)) - p - (p != 0));
            }

            // Проверка, что удалось разделить строку на 4 части
            if (i == 4) {
                uint8_t gatewayParts[4] = {0};
                for (i = 0; i < 4; i++) {
                    char* endptr = nullptr;
                    long value = strtol(sepGATEWAY[i].c_str(), &endptr, 10);

                    // Проверка успешности преобразования и диапазона значений
                    if (*endptr != '\0' || value < 0 || value > 255) {
                        parseSuccess = false;
                        break;
                    }
                    gatewayParts[i] = (uint8_t)value;
                }

                if (parseSuccess) {
                    // Сохраняем новые значения только если парсинг успешен
                    for (i = 0; i < 4; i++) {
                        settings.saveIP.gateway[i] = gatewayParts[i];
                    }
                    settingsChanged = true;
                } else {
                    STM_LOG(LOG_ERR "Invalid gateway address format");
                }
            } else {
                STM_LOG(LOG_ERR "Gateway address must have 4 octets");
            }
        }
    }

    // Обработка MASK
    if ((j_setMASK != NULL) && cJSON_IsTrue(j_setMASK) && (j_MASK != NULL) && cJSON_IsString(j_MASK)) {
        char sep = '.';
        std::string s = j_MASK->valuestring;
        if (!s.empty()) {
            std::string sepMASK[4];
            int i = 0;
            bool parseSuccess = true;

            for (size_t p = 0, q = 0; (p != s.npos) && (i < 4); p = q, i++) {
                sepMASK[i] = s.substr(p + (p != 0),
                        (q = s.find(sep, p + 1)) - p - (p != 0));
            }

            // Проверка, что удалось разделить строку на 4 части
            if (i == 4) {
                uint8_t maskParts[4] = {0};
                for (i = 0; i < 4; i++) {
                    char* endptr = nullptr;
                    long value = strtol(sepMASK[i].c_str(), &endptr, 10);

                    // Проверка успешности преобразования и диапазона значений
                    if (*endptr != '\0' || value < 0 || value > 255) {
                        parseSuccess = false;
                        break;
                    }
                    maskParts[i] = (uint8_t)value;
                }

                if (parseSuccess) {
                    // Сохраняем новые значения только если парсинг успешен
                    for (i = 0; i < 4; i++) {
                        settings.saveIP.mask[i] = maskParts[i];
                    }
                    settingsChanged = true;
                } else {
                    STM_LOG(LOG_ERR "Invalid subnet mask format");
                }
            } else {
                STM_LOG(LOG_ERR "Subnet mask must have 4 octets");
            }
        }
    }

    // Обработка DNS
    if ((j_setDNS != NULL) && cJSON_IsTrue(j_setDNS) && (j_DNS != NULL) && cJSON_IsString(j_DNS)) {
        char sep = '.';
        std::string s = j_DNS->valuestring;
        if (!s.empty()) {
            std::string sepDNS[4];
            int i = 0;
            bool parseSuccess = true;

            for (size_t p = 0, q = 0; (p != s.npos) && (i < 4); p = q, i++) {
                sepDNS[i] = s.substr(p + (p != 0),
                        (q = s.find(sep, p + 1)) - p - (p != 0));
            }

            // Проверка, что удалось разделить строку на 4 части
            if (i == 4) {
                // Закомментировано, так как в оригинальном коде это не реализовано
                // Оставляем для совместимости с исходным кодом
                //uint8_t dnsParts[4] = {0};
                //for (i = 0; i < 4; i++) {
                //    char* endptr = nullptr;
                //    long value = strtol(sepDNS[i].c_str(), &endptr, 10);
                //
                //    // Проверка успешности преобразования и диапазона значений
                //    if (*endptr != '\0' || value < 0 || value > 255) {
                //        parseSuccess = false;
                //        break;
                //    }
                //    dnsParts[i] = (uint8_t)value;
                //}
                //
                //if (parseSuccess) {
                //    // Сохраняем новые значения только если парсинг успешен
                //    //settings.[0] = dnsParts[0];
                //    //settings.[1] = dnsParts[1];
                //    //settings.[2] = dnsParts[2];
                //    //settings.[3] = dnsParts[3];
                //    settingsChanged = true;
                //} else {
                //    STM_LOG(LOG_ERR "Invalid DNS address format");
                //}
            } else {
                STM_LOG(LOG_ERR "DNS address must have 4 octets");
            }
        }
    }

    // Обработка DHCP
    if ((j_setDHCP != NULL) && cJSON_IsTrue(j_setDHCP) && (j_DHCP != NULL)) {
        settings.DHCPset = cJSON_IsTrue(j_DHCP) ? 1 : 0;
        settingsChanged = true;
    }

    if (settingsChanged) {
        STM_LOG("Settings set successful");

        // Сохранение настроек если требуется
        if (save) {
			if(mem_spi.Write(settings))
			{
				STM_LOG("Save settings OK. size %d", sizeof(settings_t));
			}
			else
			{
				STM_LOG("Save settings FAIL");
			}
        }
    } else {
        STM_LOG("No settings were changed");
    }
}

// Отправка ответа на команду: по TCP — в сокет, по UART — через
// UART_Link_SendResponse (uart_link.cpp), которая помечает ответ
// терминатором "\x03\x04", чтобы ПК отличил его от строки лога.
static void tcp_or_uart_send(const char *s, struct netconn *tcp_conn)
{
	if (tcp_conn != nullptr) {
		netconn_write(tcp_conn, s, strlen(s), NETCONN_COPY);
	} else {
		UART_Link_SendResponse(s);
	}
}

// Обработка команд управления от хоста (type_data=3)
void action_commands(cJSON *obj, struct netconn *tcp_conn)
{
	if (obj == NULL) {
		STM_LOG("action_commands: obj is NULL");
		return;
	}
	
	cJSON *cmd = cJSON_GetObjectItemCaseSensitive(obj, "cmd");
	
	if (!cJSON_IsNumber(cmd)) {
		STM_LOG("Invalid command format");
		return;
	}
	
	int cmd_num = cmd->valueint;
	STM_LOG("Received command: %d", cmd_num);
	
	switch (cmd_num)
	{
	case 4: // Включение/выключение каналов (mode: 0=instant off, 1=instant on, 3=fade on, 4=fade off)
	{
		cJSON *all       = cJSON_GetObjectItemCaseSensitive(obj, "all");
		cJSON *mode_json = cJSON_GetObjectItemCaseSensitive(obj, "mode");
		cJSON *enable    = cJSON_GetObjectItemCaseSensitive(obj, "enable");
		cJSON *channels  = cJSON_GetObjectItemCaseSensitive(obj, "channels");

		// Определяем режим (если передан mode — используем его, иначе из enable)
		int mode = -1;
		if (cJSON_IsNumber(mode_json)) {
			mode = mode_json->valueint;
		} else if (cJSON_IsBool(enable)) {
			mode = cJSON_IsTrue(enable) ? 1 : 0;
		}

		if (cJSON_IsTrue(all)) {
			switch (mode) {
				case 0: DAC_ChannelOffAll();     break;
				case 1: DAC_ChannelOnAll();      break;
				case 3: DAC_ChannelFadeOnAll();  break;
				case 4: DAC_ChannelFadeOffAll(); break;
				default: break;
			}
			STM_LOG("All channels mode %d", mode);
		}
		else if (cJSON_IsArray(channels)) {
			// Применить настройки для массива каналов
			int count = cJSON_GetArraySize(channels);
			for (int i = 0; i < count; i++) {
				cJSON *ch = cJSON_GetArrayItem(channels, i);
				if (cJSON_IsObject(ch)) {
					cJSON *num      = cJSON_GetObjectItemCaseSensitive(ch, "num");
					cJSON *pwm      = cJSON_GetObjectItemCaseSensitive(ch, "pwm");
					cJSON *enabled  = cJSON_GetObjectItemCaseSensitive(ch, "enabled");
					cJSON *ch_mode  = cJSON_GetObjectItemCaseSensitive(ch, "mode");

					if (cJSON_IsNumber(num)) {
						int ch_num = num->valueint;
						if (ch_num >= 0 && ch_num < 6) {
							if (cJSON_IsNumber(pwm)) {
								PWM_SetValue((PWM_Channel_t)ch_num, pwm->valueint);
							}
							int ch_m = -1;
							if (cJSON_IsNumber(ch_mode)) {
								ch_m = ch_mode->valueint;
							} else if (cJSON_IsBool(enabled)) {
								ch_m = cJSON_IsTrue(enabled) ? 1 : 0;
							}
							switch (ch_m) {
								case 0: DAC_ChannelOff((uint8_t)ch_num);     break;
								case 1: DAC_ChannelOn((uint8_t)ch_num);      break;
								case 3: DAC_ChannelFadeOn((uint8_t)ch_num);  break;
								case 4: DAC_ChannelFadeOff((uint8_t)ch_num); break;
								default: break;
							}
						}
					}
				}
			}
			STM_LOG("Applied settings for %d channels", count);
		}
		break;
	}
	// -----------------------------------------------------------------------
	// Команда 5: установить сырое значение PWM duty (0-1000) — отдельно от
	// уровня тока ЦАП (cmd 21). Не включает канал (см. PWM_SetValue).
	// {"cmd":5,"channel":N,"value":V}  — один канал
	// {"cmd":5,"all":true,"value":V}   — все каналы
	// -----------------------------------------------------------------------
	case 5:
	{
		cJSON *j_all     = cJSON_GetObjectItemCaseSensitive(obj, "all");
		cJSON *j_channel = cJSON_GetObjectItemCaseSensitive(obj, "channel");
		cJSON *j_value   = cJSON_GetObjectItemCaseSensitive(obj, "value");

		if (!cJSON_IsNumber(j_value)) { STM_LOG("C5: missing value"); break; }

		int val = j_value->valueint;
		if (val < 0) val = 0;
		if (val > (int)PWM_MAX_VALUE) val = (int)PWM_MAX_VALUE;

		if (cJSON_IsTrue(j_all))
		{
			PWM_SetAllChannels((uint16_t)val);
			STM_LOG("C5: all -> pwm=%d", val);
		}
		else if (cJSON_IsNumber(j_channel))
		{
			int ch = j_channel->valueint;
			if (ch >= 0 && ch < (int)PWM_CH_COUNT)
				PWM_SetValue((PWM_Channel_t)ch, (uint16_t)val);
		}

		cJSON *resp = cJSON_CreateObject();
		cJSON_AddNumberToObject(resp, "id", ID_CTRL);
		cJSON_AddNumberToObject(resp, "type_data", 3);
		cJSON *o = cJSON_CreateObject();
		cJSON_AddNumberToObject(o, "cmd", 5);
		cJSON_AddStringToObject(o, "status", "OK");
		if (cJSON_IsNumber(j_channel) && !cJSON_IsTrue(j_all))
		{
			int ch = j_channel->valueint;
			cJSON_AddNumberToObject(o, "channel", ch);
			if (ch >= 0 && ch < (int)PWM_CH_COUNT)
				cJSON_AddNumberToObject(o, "value", (int)PWM_GetValue((PWM_Channel_t)ch));
		}
		cJSON_AddItemToObject(resp, "obj", o);
		char *s = cJSON_Print(resp);
		tcp_or_uart_send(s, tcp_conn);
		cJSON_free(s); cJSON_Delete(resp);
		break;
	}
	// -----------------------------------------------------------------------
	// Команда 6: прочитать сырое значение PWM duty всех или одного канала
	// {"cmd":6}            — все каналы
	// {"cmd":6,"channel":N} — один канал
	// -----------------------------------------------------------------------
	case 6:
	{
		cJSON *j_channel = cJSON_GetObjectItemCaseSensitive(obj, "channel");

		cJSON *resp = cJSON_CreateObject();
		cJSON_AddNumberToObject(resp, "id", ID_CTRL);
		cJSON_AddNumberToObject(resp, "type_data", 3);
		cJSON *o = cJSON_CreateObject();
		cJSON_AddNumberToObject(o, "cmd", 6);

		if (cJSON_IsNumber(j_channel))
		{
			int ch = j_channel->valueint;
			if (ch < 0 || ch >= (int)PWM_CH_COUNT) { STM_LOG("C6: bad channel"); break; }
			cJSON_AddNumberToObject(o, "channel", ch);
			cJSON_AddNumberToObject(o, "value", (int)PWM_GetValue((PWM_Channel_t)ch));
		}
		else
		{
			cJSON *arr = cJSON_CreateArray();
			for (int i = 0; i < (int)PWM_CH_COUNT; i++)
				cJSON_AddItemToArray(arr, cJSON_CreateNumber((int)PWM_GetValue((PWM_Channel_t)i)));
			cJSON_AddItemToObject(o, "value", arr);
		}

		cJSON_AddItemToObject(resp, "obj", o);
		char *s = cJSON_Print(resp);
		tcp_or_uart_send(s, tcp_conn);
		cJSON_free(s); cJSON_Delete(resp);
		break;
	}
	case 7: // Управление временем плавного перехода (fade duration)
	{
		cJSON *duration_ms = cJSON_GetObjectItemCaseSensitive(obj, "duration_ms");
		if (cJSON_IsNumber(duration_ms)) {
			int ms = duration_ms->valueint;
			if (ms >= 50 && ms <= 10000) {
				DAC_SetFadeDuration((uint16_t)ms);
				STM_LOG("Fade duration set to %d ms", ms);
			}
		}
		break;
	}
	// -----------------------------------------------------------------------
	// Команда 8: проверить включён ли канал (по LD, см. cmd 4)
	// {"cmd":8}            — все каналы
	// {"cmd":8,"channel":N} — один канал
	// -----------------------------------------------------------------------
	case 8:
	{
		cJSON *j_channel = cJSON_GetObjectItemCaseSensitive(obj, "channel");

		cJSON *resp = cJSON_CreateObject();
		cJSON_AddNumberToObject(resp, "id", ID_CTRL);
		cJSON_AddNumberToObject(resp, "type_data", 3);
		cJSON *o = cJSON_CreateObject();
		cJSON_AddNumberToObject(o, "cmd", 8);

		if (cJSON_IsNumber(j_channel))
		{
			int ch = j_channel->valueint;
			if (ch < 0 || ch >= (int)PWM_CH_COUNT) { STM_LOG("C8: bad channel"); break; }
			cJSON_AddNumberToObject(o, "channel", ch);
			cJSON_AddNumberToObject(o, "enabled", DAC_ChannelIsOn((uint8_t)ch) ? 1 : 0);
		}
		else
		{
			cJSON *arr = cJSON_CreateArray();
			for (int i = 0; i < (int)PWM_CH_COUNT; i++)
				cJSON_AddItemToArray(arr, cJSON_CreateNumber(DAC_ChannelIsOn((uint8_t)i) ? 1 : 0));
			cJSON_AddItemToObject(o, "enabled", arr);
		}

		cJSON_AddItemToObject(resp, "obj", o);
		char *s = cJSON_Print(resp);
		tcp_or_uart_send(s, tcp_conn);
		cJSON_free(s); cJSON_Delete(resp);
		break;
	}
	case 9: // Сохранение настроек
	{
		mem_spi.Write(settings);
		STM_LOG("Settings saved to flash");
		break;
	}
	case 11: // Перезагрузка
	{
		cJSON *reboot = cJSON_GetObjectItemCaseSensitive(obj, "reboot");
		if (cJSON_IsTrue(reboot)) {
			STM_LOG("Rebooting device...");
			HAL_Delay(100);
			NVIC_SystemReset();
		}
		break;
	}
	case 20: // Калибровка датчика тока ACS712
	{
		cJSON *mode = cJSON_GetObjectItemCaseSensitive(obj, "mode");
		int calibration_mode = cJSON_IsNumber(mode) ? mode->valueint : 0;
		
		if (calibration_mode == 0) {
			// Выполнить калибровку нуля
			STM_LOG("Starting current calibration...");
			ACS712_CalibrateZero();
			
			// Отправляем ответ с результатом
			cJSON *response = cJSON_CreateObject();
			cJSON_AddNumberToObject(response, "id", ID_CTRL);
			cJSON_AddNumberToObject(response, "type_data", 3);
			cJSON *resp_obj = cJSON_CreateObject();
			cJSON_AddNumberToObject(resp_obj, "cmd", 20);
			cJSON_AddStringToObject(resp_obj, "status", "OK");
			int32_t offset_ma = (int32_t)(g_current_zero_offset * 1000.0f);
			cJSON_AddNumberToObject(resp_obj, "offset_ma", offset_ma);
			cJSON_AddItemToObject(response, "obj", resp_obj);
			
			char *resp_str = cJSON_Print(response);
			tcp_or_uart_send(resp_str, tcp_conn);
			cJSON_free(resp_str);
			cJSON_Delete(response);
			
			STM_LOG("Calibration completed. Offset: %d mA", offset_ma);
		}
		else if (calibration_mode == 1) {
			// Прочитать текущее смещение
			int32_t offset_ma = (int32_t)(g_current_zero_offset * 1000.0f);
			
			cJSON *response = cJSON_CreateObject();
			cJSON_AddNumberToObject(response, "id", ID_CTRL);
			cJSON_AddNumberToObject(response, "type_data", 3);
			cJSON *resp_obj = cJSON_CreateObject();
			cJSON_AddNumberToObject(resp_obj, "cmd", 20);
			cJSON_AddStringToObject(resp_obj, "status", "OK");
			cJSON_AddNumberToObject(resp_obj, "offset_ma", offset_ma);
			cJSON_AddItemToObject(response, "obj", resp_obj);
			
			char *resp_str = cJSON_Print(response);
			tcp_or_uart_send(resp_str, tcp_conn);
			cJSON_free(resp_str);
			cJSON_Delete(response);
			
			STM_LOG("Current offset: %d mA", offset_ma);
		}
		else if (calibration_mode == 2) {
			// Сбросить калибровку
			g_current_zero_offset = 0.0f;
			Calibration_Save();
			
			cJSON *response = cJSON_CreateObject();
			cJSON_AddNumberToObject(response, "id", ID_CTRL);
			cJSON_AddNumberToObject(response, "type_data", 3);
			cJSON *resp_obj = cJSON_CreateObject();
			cJSON_AddNumberToObject(resp_obj, "cmd", 20);
			cJSON_AddStringToObject(resp_obj, "status", "OK");
			cJSON_AddNumberToObject(resp_obj, "offset_ma", 0);
			cJSON_AddItemToObject(response, "obj", resp_obj);
			
			char *resp_str = cJSON_Print(response);
			tcp_or_uart_send(resp_str, tcp_conn);
			cJSON_free(resp_str);
			cJSON_Delete(response);
			
			STM_LOG("Calibration reset");
		}
		break;
	}
	// -----------------------------------------------------------------------
	// Команда 21: установить мощность канала в процентах (0-1000)
	// {"cmd":21,"channel":N,"percent":P}  — один канал
	// {"cmd":21,"all":true,"percent":P}   — все каналы
	// -----------------------------------------------------------------------
	case 21:
	{
		cJSON *j_all     = cJSON_GetObjectItemCaseSensitive(obj, "all");
		cJSON *j_channel = cJSON_GetObjectItemCaseSensitive(obj, "channel");
		cJSON *j_percent = cJSON_GetObjectItemCaseSensitive(obj, "percent");

		if (!cJSON_IsNumber(j_percent)) { STM_LOG("C21: missing percent"); break; }

		int pct = j_percent->valueint;
		if (pct < 0) pct = 0;
		if (pct > (int)DAC_PERCENT_MAX) pct = (int)DAC_PERCENT_MAX;

		if (cJSON_IsTrue(j_all))
		{
			for (int i = 0; i < (int)DAC_CH_COUNT; i++)
				DAC_SetPercent((uint8_t)i, (uint16_t)pct);
			STM_LOG("C21: all -> %d.%d%%", pct / 10, pct % 10);
		}
		else if (cJSON_IsNumber(j_channel))
		{
			int ch = j_channel->valueint;
			if (ch >= 0 && ch < (int)DAC_CH_COUNT)
				DAC_SetPercent((uint8_t)ch, (uint16_t)pct);
		}

		cJSON *resp = cJSON_CreateObject();
		cJSON_AddNumberToObject(resp, "id", ID_CTRL);
		cJSON_AddNumberToObject(resp, "type_data", 3);
		cJSON *o = cJSON_CreateObject();
		cJSON_AddNumberToObject(o, "cmd", 21);
		cJSON_AddStringToObject(o, "status", "OK");
		if (cJSON_IsNumber(j_channel) && !cJSON_IsTrue(j_all))
		{
			int ch = j_channel->valueint;
			cJSON_AddNumberToObject(o, "channel", ch);
			cJSON_AddNumberToObject(o, "percent", pct);
			if (ch >= 0 && ch < (int)DAC_CH_COUNT)
				cJSON_AddNumberToObject(o, "dac_value", (int)g_dac_setpoints[ch]);
		}
		cJSON_AddItemToObject(resp, "obj", o);
		char *s = cJSON_Print(resp);
		tcp_or_uart_send(s, tcp_conn);
		cJSON_free(s); cJSON_Delete(resp);
		break;
	}

	// -----------------------------------------------------------------------
	// Команда 22: прочитать уставки DAC всех или одного канала
	// {"cmd":22}            — все каналы
	// {"cmd":22,"channel":N} — один канал
	// -----------------------------------------------------------------------
	case 22:
	{
		cJSON *j_channel = cJSON_GetObjectItemCaseSensitive(obj, "channel");

		cJSON *resp = cJSON_CreateObject();
		cJSON_AddNumberToObject(resp, "id", ID_CTRL);
		cJSON_AddNumberToObject(resp, "type_data", 3);
		cJSON *o = cJSON_CreateObject();
		cJSON_AddNumberToObject(o, "cmd", 22);

		if (cJSON_IsNumber(j_channel))
		{
			int ch = j_channel->valueint;
			if (ch < 0 || ch >= (int)DAC_CH_COUNT) { STM_LOG("C22: bad channel"); break; }
			cJSON_AddNumberToObject(o, "channel", ch);
			cJSON_AddNumberToObject(o, "percent",   (int)g_percent_setpoints[ch]);
			cJSON_AddNumberToObject(o, "dac_value", (int)g_dac_setpoints[ch]);
		}
		else
		{
			cJSON *pct_arr = cJSON_CreateArray();
			cJSON *dac_arr = cJSON_CreateArray();
			for (int i = 0; i < (int)DAC_CH_COUNT; i++)
			{
				cJSON_AddItemToArray(pct_arr, cJSON_CreateNumber((int)g_percent_setpoints[i]));
				cJSON_AddItemToArray(dac_arr, cJSON_CreateNumber((int)g_dac_setpoints[i]));
			}
			cJSON_AddItemToObject(o, "percent",   pct_arr);
			cJSON_AddItemToObject(o, "dac_value", dac_arr);
		}

		cJSON_AddItemToObject(resp, "obj", o);
		char *s = cJSON_Print(resp);
		tcp_or_uart_send(s, tcp_conn);
		cJSON_free(s); cJSON_Delete(resp);
		break;
	}

	// -----------------------------------------------------------------------
	// Команда 23: установить сырое значение DAC (0-4095) — для отладки
	// {"cmd":23,"channel":N,"dac_value":V}
	// -----------------------------------------------------------------------
	case 23:
	{
		cJSON *j_channel   = cJSON_GetObjectItemCaseSensitive(obj, "channel");
		cJSON *j_dac_value = cJSON_GetObjectItemCaseSensitive(obj, "dac_value");

		if (!cJSON_IsNumber(j_channel) || !cJSON_IsNumber(j_dac_value))
		{ STM_LOG("C23: bad params"); break; }

		int ch  = j_channel->valueint;
		int val = j_dac_value->valueint;
		if (ch < 0 || ch >= (int)DAC_CH_COUNT) { STM_LOG("C23: bad channel"); break; }
		if (val < 0) val = 0;
		if (val > (int)MCP4725_DAC_MAX) val = (int)MCP4725_DAC_MAX;

		PWM_SetValue((PWM_Channel_t)ch, PWM_MAX_VALUE);
		DAC_SetRaw((uint8_t)ch, (uint16_t)val);

		cJSON *resp = cJSON_CreateObject();
		cJSON_AddNumberToObject(resp, "id", ID_CTRL);
		cJSON_AddNumberToObject(resp, "type_data", 3);
		cJSON *o = cJSON_CreateObject();
		cJSON_AddNumberToObject(o, "cmd", 23);
		cJSON_AddStringToObject(o, "status", "OK");
		cJSON_AddNumberToObject(o, "channel", ch);
		cJSON_AddNumberToObject(o, "dac_value", val);
		cJSON_AddItemToObject(resp, "obj", o);
		char *s = cJSON_Print(resp);
		tcp_or_uart_send(s, tcp_conn);
		cJSON_free(s); cJSON_Delete(resp);
		break;
	}

	// -----------------------------------------------------------------------
	// Команда 24: прочитать сырое значение DAC канала
	// {"cmd":24,"channel":N}
	// -----------------------------------------------------------------------
	case 24:
	{
		cJSON *j_channel = cJSON_GetObjectItemCaseSensitive(obj, "channel");
		if (!cJSON_IsNumber(j_channel)) { STM_LOG("C24: missing channel"); break; }

		int ch = j_channel->valueint;
		if (ch < 0 || ch >= (int)DAC_CH_COUNT) { STM_LOG("C24: bad channel"); break; }

		cJSON *resp = cJSON_CreateObject();
		cJSON_AddNumberToObject(resp, "id", ID_CTRL);
		cJSON_AddNumberToObject(resp, "type_data", 3);
		cJSON *o = cJSON_CreateObject();
		cJSON_AddNumberToObject(o, "cmd", 24);
		cJSON_AddNumberToObject(o, "channel", ch);
		cJSON_AddNumberToObject(o, "dac_value", (int)g_dac_setpoints[ch]);
		cJSON_AddItemToObject(resp, "obj", o);
		char *s = cJSON_Print(resp);
		tcp_or_uart_send(s, tcp_conn);
		cJSON_free(s); cJSON_Delete(resp);
		break;
	}

	// -----------------------------------------------------------------------
	// Команда 25: выключить канал(ы): DAC=0, PWM=0
	// {"cmd":25,"channel":N}   — один канал
	// {"cmd":25,"all":true}    — все каналы
	// -----------------------------------------------------------------------
	case 25:
	{
		cJSON *j_channel = cJSON_GetObjectItemCaseSensitive(obj, "channel");
		cJSON *j_all     = cJSON_GetObjectItemCaseSensitive(obj, "all");

		if (cJSON_IsTrue(j_all))
		{
			DAC_DisableAll();
			STM_LOG("C25: all channels disabled");
		}
		else if (cJSON_IsNumber(j_channel))
		{
			int ch = j_channel->valueint;
			if (ch >= 0 && ch < (int)DAC_CH_COUNT)
			{
				DAC_DisableChannel((uint8_t)ch);
				STM_LOG("C25: ch%d disabled", ch);
			}
		}

		cJSON *resp = cJSON_CreateObject();
		cJSON_AddNumberToObject(resp, "id", ID_CTRL);
		cJSON_AddNumberToObject(resp, "type_data", 3);
		cJSON *o = cJSON_CreateObject();
		cJSON_AddNumberToObject(o, "cmd", 25);
		cJSON_AddStringToObject(o, "status", "OK");
		cJSON_AddItemToObject(resp, "obj", o);
		char *s = cJSON_Print(resp);
		tcp_or_uart_send(s, tcp_conn);
		cJSON_free(s); cJSON_Delete(resp);
		break;
	}

	// -----------------------------------------------------------------------
	// Команда 26: прочитать результат поиска адресов MCP4725 по каналам
	// {"cmd":26}
	// -----------------------------------------------------------------------
	case 26:
	{
		cJSON *resp = cJSON_CreateObject();
		cJSON_AddNumberToObject(resp, "id", ID_CTRL);
		cJSON_AddNumberToObject(resp, "type_data", 3);
		cJSON *o = cJSON_CreateObject();
		cJSON_AddNumberToObject(o, "cmd", 26);

		cJSON *addr_arr  = cJSON_CreateArray();
		cJSON *found_arr = cJSON_CreateArray();
		for (int i = 0; i < (int)DAC_CH_COUNT; i++)
		{
			cJSON_AddItemToArray(addr_arr,  cJSON_CreateNumber((int)g_mcp4725_addr[i]));
			cJSON_AddItemToArray(found_arr, cJSON_CreateNumber((int)g_mcp4725_found[i]));
		}
		cJSON_AddItemToObject(o, "addr",  addr_arr);
		cJSON_AddItemToObject(o, "found", found_arr);

		cJSON_AddItemToObject(resp, "obj", o);
		char *s = cJSON_Print(resp);
		tcp_or_uart_send(s, tcp_conn);
		cJSON_free(s); cJSON_Delete(resp);
		break;
	}

	// -----------------------------------------------------------------------
	// Команда 27: сохранить текущие уставки тока (percent) всех каналов во flash
	// {"cmd":27}
	// Бит включения/выключения (cmd 4) НЕ сохраняется — см. dac_settings.h
	// -----------------------------------------------------------------------
	case 27:
	{
		DAC_Settings_Save();

		cJSON *resp = cJSON_CreateObject();
		cJSON_AddNumberToObject(resp, "id", ID_CTRL);
		cJSON_AddNumberToObject(resp, "type_data", 3);
		cJSON *o = cJSON_CreateObject();
		cJSON_AddNumberToObject(o, "cmd", 27);
		cJSON_AddStringToObject(o, "status", "OK");
		cJSON_AddItemToObject(resp, "obj", o);
		char *s = cJSON_Print(resp);
		tcp_or_uart_send(s, tcp_conn);
		cJSON_free(s); cJSON_Delete(resp);
		break;
	}

	default:
	{
		STM_LOG("Unknown command: %d", cmd_num);
		break;
	}
	}
}

// Подготовка и отправка настроек сети на хост
void action_settings_data(cJSON *obj, struct netconn *tcp_conn)
{
	cJSON *j_all_settings_obj = cJSON_CreateObject();
	cJSON *obj_ch = cJSON_CreateArray();
	cJSON *obj_ip = cJSON_CreateObject();

	cJSON_AddNumberToObject(j_all_settings_obj, "id", ID_CTRL);
	cJSON_AddStringToObject(j_all_settings_obj, "name_device", NAME);
	cJSON_AddNumberToObject(j_all_settings_obj, "type_data", 4);

	// настройки ip
	string srtIP_to_host = std::to_string(settings.saveIP.ip[0])+"."+
							std::to_string(settings.saveIP.ip[1])+"."+
							std::to_string(settings.saveIP.ip[2])+ "."+
							std::to_string(settings.saveIP.ip[3]);
	cJSON_AddStringToObject(obj_ip, "IP", srtIP_to_host.c_str());

	char srtMAC_to_host[100];
	sprintf(srtMAC_to_host,"%x:%x:%x:%x:%x:%x",settings.MAC[0],settings.MAC[1],settings.MAC[2],
												settings.MAC[3],settings.MAC[4],settings.MAC[5]);
	cJSON_AddStringToObject(obj_ip, "MAC", srtMAC_to_host);

	string srtGATEWAY_to_host = std::to_string(settings.saveIP.gateway[0])+"."+
							std::to_string(settings.saveIP.gateway[1])+"."+
							std::to_string(settings.saveIP.gateway[2])+"."+
							std::to_string(settings.saveIP.gateway[3]);
	cJSON_AddStringToObject(obj_ip, "GATEWAY", srtGATEWAY_to_host.c_str());

	string srtMASK_to_host = std::to_string(settings.saveIP.mask[0])+"."+
							std::to_string(settings.saveIP.mask[1])+"."+
							std::to_string(settings.saveIP.mask[2])+"."+
							std::to_string(settings.saveIP.mask[3]);
	cJSON_AddStringToObject(obj_ip, "MASK", srtMASK_to_host.c_str());

	cJSON_AddStringToObject(obj_ip, "DNS", "0.0.0.0");

	if(settings.DHCPset)
	{
		cJSON_AddTrueToObject(obj_ip, "DHCP");
	}
	else
	{
		cJSON_AddFalseToObject(obj_ip, "DHCP");
	}

	// Отправка данных о 6 PWM каналах
	for (int ch = 0; ch < 6; ch++) {
		cJSON *temp_obj = cJSON_CreateObject();

		cJSON_AddNumberToObject(temp_obj, "num", ch);
		cJSON_AddNumberToObject(temp_obj, "pwm", pwm_controller.channels[ch].value);
		cJSON_AddBoolToObject(temp_obj, "enabled", DAC_ChannelIsOn((uint8_t)ch));

		cJSON_AddItemToArray(obj_ch, temp_obj);
	}

	cJSON_AddItemToObject(j_all_settings_obj, "obj_ch", obj_ch);
	
	// Добавляем текущее значение тока с датчика ACS712 в obj_ip
	int32_t current_ma = (int32_t)(g_current_amperes * 1000.0f);
	cJSON_AddNumberToObject(obj_ip, "current_ma", current_ma);
	
	cJSON_AddItemToObject(j_all_settings_obj, "obj_ip", obj_ip);

	char *str_to_host = cJSON_Print(j_all_settings_obj);

	STM_LOG("Отправка настроек (len=%u)", (unsigned)strlen(str_to_host));

	if (tcp_conn != nullptr) {
		// TCP: LwIP сам разбивает на сегменты
		netconn_write(tcp_conn, str_to_host, strlen(str_to_host), NETCONN_COPY);
	} else {
		// UART: отправляем частями из-за ограничения буфера
		size_t total_len = strlen(str_to_host);
		const size_t chunk_size = MAX_MESSAGE_SIZE - 3;
		size_t offset = 0;
		while (offset < total_len) {
			size_t cur = (total_len - offset > chunk_size) ? chunk_size : (total_len - offset);
			char saved = str_to_host[offset + cur];
			str_to_host[offset + cur] = '\0';
			STM_LOG_xx("%s", str_to_host + offset);
			str_to_host[offset + cur] = saved;
			offset += cur;
		}
		STM_LOG_xx("\x03\x04");
	}

	cJSON_free(str_to_host);
	cJSON_Delete(j_all_settings_obj);

}

/**
 * @brief  Запуск преобразования АЦП с DMA
 * @note   Запускает 16 последовательных измерений канала PA4 с DMA
 */
void ACS712_StartDMA_Conversion(void)
{
    // Сбрасываем флаг завершения
    adc_conversion_complete = 0;
    
    // Запускаем АЦП с DMA (16 измерений)
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_dma_buffer, ADC_SAMPLES);
}

/**
 * @brief  Фильтр скользящего среднего
 * @param  data - указатель на массив данных
 * @param  size - размер массива
 * @retval Среднее значение
 */
uint16_t ACS712_MovingAverage(uint16_t* data, uint16_t size)
{
    uint32_t sum = 0;
    
    // Суммируем все значения
    for (uint16_t i = 0; i < size; i++)
    {
        sum += data[i];
    }
    
    // Возвращаем среднее значение
    return (uint16_t)(sum / size);
}

/**
 * @brief  Обработка данных АЦП и преобразование в ток
 * @note   Применяет фильтр скользящего среднего и преобразует в ток
 *         ACS712ELCTR-20A-T характеристики:
 *         - Чувствительность: 100 мВ/А
 *         - Выходное напряжение при 0А: 2.5В (VCC/2 при VCC=5В)
 *         - На PA4 подключен через делитель на 2
 *         - АЦП 12-бит, VREF = 3.3В
 * @retval Ток в амперах
 */
float ACS712_ProcessData(void)
{
    // Применяем фильтр скользящего среднего
    uint16_t adc_avg = ACS712_MovingAverage(adc_dma_buffer, ADC_SAMPLES);
    
    // Преобразуем значение АЦП в напряжение (В)
    // VREF = 3.3В, 12-бит АЦП (4096 уровней)
    float voltage_on_adc = (float)adc_avg * 3.3f / 4096.0f;
    
    // Учитываем делитель напряжения (на 2)
    // Напряжение на выходе ACS712 в 2 раза больше
    float voltage_acs712 = voltage_on_adc * 2.0f;
    
    // ACS712 выдает 2.5В при токе 0А
    // Чувствительность 100 мВ/А = 0.1 В/А
    float zero_current_voltage = 2.5f;  // В
    float sensitivity = 0.1f;            // В/А
    
    // Вычисляем ток
    // I = (V_out - V_zero) / Sensitivity
    float current_raw = (voltage_acs712 - zero_current_voltage) / sensitivity;
    
    // Применяем калибровочное смещение нуля
    current_raw -= g_current_zero_offset;
    
    // Применяем экспоненциальный фильтр низких частот (EMA - Exponential Moving Average)
    // Формула: filtered = alpha * new_value + (1 - alpha) * filtered
    // Это эффективно подавляет высокочастотные шумы и выбросы
    if (g_current_filter_initialized == 0)
    {
        // Первое измерение - инициализируем фильтр текущим значением
        g_current_filtered = current_raw;
        g_current_filter_initialized = 1;
    }
    else
    {
        // Применяем экспоненциальное сглаживание
        g_current_filtered = CURRENT_FILTER_ALPHA * current_raw + 
                            (1.0f - CURRENT_FILTER_ALPHA) * g_current_filtered;
    }
    
    // Обновляем глобальную переменную фильтрованным значением
    g_current_amperes = g_current_filtered;
    
    return g_current_filtered;
}

/**
 * @brief  Вычисление CRC32 для калибровочных данных
 */
uint32_t Calibration_CalculateCRC(calibration_t* cal)
{
    uint32_t crc = 0;
    uint8_t* data = (uint8_t*)cal;
    uint32_t size = sizeof(calibration_t) - sizeof(uint32_t); // Без поля CRC
    
    // Простой CRC32
    for (uint32_t i = 0; i < size; i++)
    {
        crc = crc ^ data[i];
        for (uint8_t j = 0; j < 8; j++)
        {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc = crc >> 1;
        }
    }
    return crc;
}

/**
 * @brief  Сохранение калибровочных данных во flash
 */
void Calibration_Save(void)
{
    STM_LOG("Calibration_Save: start");
    
    calibration_t cal;
    
    // Заполняем структуру
    cal.magic_key = CALIBRATION_MAGIC;
    cal.current_zero_offset = g_current_zero_offset;
    cal.reserved1 = 0;
    cal.reserved2 = 0;
    cal.crc = Calibration_CalculateCRC(&cal);
    
    STM_LOG("Calibration_Save: erasing flash sector at 0x%04X...", CALIBRATION_FLASH_ADDR);
    
    // ВАЖНО: W25qxx_EraseSector ожидает НОМЕР СЕКТОРА, а не адрес!
    // Flash сектор = 4KB (0x1000), поэтому нужно делить адрес на размер сектора
    uint32_t sector_num = CALIBRATION_FLASH_ADDR / 4096;
    mem_spi.W25qxx_EraseSector(sector_num);
    
    // Даем время на завершение стирания
    osDelay(50);
    
    STM_LOG("Calibration_Save: writing to flash (addr=0x%04X, size=%d)...", 
            CALIBRATION_FLASH_ADDR, sizeof(calibration_t));
    
    // Сохраняем во flash (W25qxx_Write - правильная функция, W25qxx_WriteBytes - заглушка!)
    mem_spi.W25qxx_Write(CALIBRATION_FLASH_ADDR, (uint8_t*)&cal, sizeof(calibration_t));
    
    // Даем время на завершение записи
    osDelay(50);
    
    STM_LOG("Calibration_Save: verifying write...");
    
    // ВЕРИФИКАЦИЯ: читаем обратно и проверяем
    calibration_t verify_cal;
    mem_spi.W25qxx_ReadBytes(CALIBRATION_FLASH_ADDR, (uint8_t*)&verify_cal, sizeof(calibration_t));
    
    // Проверяем, что данные записались правильно
    if (verify_cal.magic_key != cal.magic_key || 
        verify_cal.crc != cal.crc ||
        verify_cal.current_zero_offset != cal.current_zero_offset)
    {
        STM_LOG("ERROR: Flash write verification FAILED!");
        STM_LOG("  Written: magic=0x%08X, crc=0x%08X, offset=%d", 
                (unsigned int)cal.magic_key, (unsigned int)cal.crc, 
                (int)(cal.current_zero_offset * 1000));
        STM_LOG("  Read back: magic=0x%08X, crc=0x%08X, offset=%d", 
                (unsigned int)verify_cal.magic_key, (unsigned int)verify_cal.crc,
                (int)(verify_cal.current_zero_offset * 1000));
    }
    else
    {
        STM_LOG("Calibration_Save: verification OK");
        int32_t offset_ma = (int32_t)(g_current_zero_offset * 1000.0f);
        STM_LOG("Calibration saved successfully: offset = %d mA", offset_ma);
    }
}

/**
 * @brief  Загрузка калибровочных данных из flash
 */
void Calibration_Load(void)
{
    STM_LOG("Calibration_Load: reading from address 0x%04X, size %d bytes", 
            CALIBRATION_FLASH_ADDR, sizeof(calibration_t));
    
    calibration_t cal;
    
    // Читаем из flash (порядок параметров: адрес, буфер, размер)
    mem_spi.W25qxx_ReadBytes(CALIBRATION_FLASH_ADDR, (uint8_t*)&cal, sizeof(calibration_t));
    
    // Выводим сырые данные для диагностики
    STM_LOG("Raw data read from flash:");
    STM_LOG("  magic_key = 0x%08X (expected 0x%08X)", 
            (unsigned int)cal.magic_key, (unsigned int)CALIBRATION_MAGIC);
    STM_LOG("  offset = %d mA", (int)(cal.current_zero_offset * 1000));
    STM_LOG("  crc = 0x%08X", (unsigned int)cal.crc);
    
    // Проверяем магическое число
    if (cal.magic_key != CALIBRATION_MAGIC)
    {
        STM_LOG("FAILED: Magic key mismatch - calibration data not found or corrupted");
        STM_LOG("Using default offset = 0.0 mA");
        g_current_zero_offset = 0.0f;
        return;
    }
    
    STM_LOG("Magic key OK, checking CRC...");
    
    // Проверяем CRC
    uint32_t calculated_crc = Calibration_CalculateCRC(&cal);
    STM_LOG("CRC check: stored = 0x%08X, calculated = 0x%08X", 
            (unsigned int)cal.crc, (unsigned int)calculated_crc);
    
    if (calculated_crc != cal.crc)
    {
        STM_LOG("FAILED: CRC mismatch - calibration data corrupted");
        STM_LOG("Using default offset = 0.0 mA");
        g_current_zero_offset = 0.0f;
        return;
    }
    
    // Загружаем значение
    g_current_zero_offset = cal.current_zero_offset;
    int32_t offset_ma = (int32_t)(g_current_zero_offset * 1000.0f);
    STM_LOG("SUCCESS: Calibration loaded, offset = %d mA", offset_ma);
}

/**
 * @brief  Калибровка нуля датчика тока
 * @note   Вызывайте эту функцию при отсутствии нагрузки (ток = 0А)
 *         Функция запоминает текущее показание как нулевую точку
 *         и в дальнейшем вычитает это смещение из всех измерений
 */
void ACS712_CalibrateZero(void)
{
    // Сбрасываем фильтр для точной калибровки
    g_current_filter_initialized = 0;
    
    // Временно обнуляем смещение для чистого измерения
    g_current_zero_offset = 0.0f;
    
    // Делаем несколько измерений для усреднения
    float sum = 0.0f;
    const int calibration_samples = 10;
    
    for (int i = 0; i < calibration_samples; i++)
    {
        // Запускаем преобразование
        ACS712_StartDMA_Conversion();
        
        // Ждём завершения
        uint32_t timeout = 100;
        uint32_t start_tick = osKernelSysTick();
        while (!adc_conversion_complete && (osKernelSysTick() - start_tick) < timeout)
        {
            osDelay(1);  // Используем osDelay вместо HAL_Delay для FreeRTOS
        }
        
        if (adc_conversion_complete)
        {
            // Обрабатываем данные вручную для получения сырого значения
            uint16_t adc_avg = ACS712_MovingAverage(adc_dma_buffer, ADC_SAMPLES);
            float voltage_on_adc = (float)adc_avg * 3.3f / 4096.0f;
            float voltage_acs712 = voltage_on_adc * 2.0f;
            float current = (voltage_acs712 - 2.5f) / 0.1f;
            sum += current;
        }
        
        osDelay(50);  // Используем osDelay вместо HAL_Delay для FreeRTOS
    }
    
    // Вычисляем среднее смещение
    g_current_zero_offset = sum / calibration_samples;
    
    // Даем другим задачам отработать перед записью во flash
    osDelay(10);
    
    // Сохраняем калибровку во flash
    Calibration_Save();
    
    // Даем время на завершение записи flash
    osDelay(100);
    
    // Сбрасываем фильтр для нового старта
    g_current_filter_initialized = 0;
    g_current_filtered = 0.0f;
    g_current_amperes = 0.0f;
}

/**
 * @brief  Callback вызываемый при завершении преобразования АЦП через DMA
 * @param  hadc - указатель на handle АЦП
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance == ADC1)
    {
        // Устанавливаем флаг завершения конверсии
        adc_conversion_complete = 1;
        
        // Обрабатываем данные (применяем фильтр и преобразуем в ток)
        ACS712_ProcessData();
    }
}

/* USER CODE END Application */
