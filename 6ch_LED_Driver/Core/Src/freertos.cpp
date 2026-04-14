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
void action_settings_data(cJSON *obj);
void action_commands(cJSON *obj);
//структуры для netcon
extern struct netif gnetif;

//TCP_IP
string strIP;
string in_str;

// обмен данными с компом
extern uint8_t message_rx[message_RX_LENGTH];
extern uint8_t UART_debug_rx[UART6_RX_LENGTH];
extern uint16_t indx_message_rx;
extern uint16_t indx_UART6_rx;
extern uint16_t Size_message;
extern uint16_t Start_index;

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
osThreadId fadeTasHandle;
osMessageQId rxDataUART6Handle;
osMessageQId rxDataUART1Handle;
osSemaphoreId ADC_endHandle;
osSemaphoreId ADC_end2Handle;
osSemaphoreId Resive_USARTHandle;
osSemaphoreId mulicom_uartHandle;

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
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

/* USER CODE END FunctionPrototypes */

void mainTask(void const * argument);
void led(void const * argument);
void eth_Task(void const * argument);
void mbrtuTask(void const * argument);
void mbethTask(void const * argument);
void uart_Task(void const * argument);
void LoggerTask(void const * argument);
void fadeTask(void const * argument);

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
  	
  	// Загружаем калибровочные данные из flash
  	Calibration_Load();

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
								STM_LOG("Get CMD %s", in_str.c_str());

								if (!in_str.empty()) {
									string resp = Сommand_execution(in_str);
									netconn_write(newconn, resp.c_str(), resp.size(), NETCONN_COPY);
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
  // Запускаем прием по UART6 (связь с компьютером)
  HAL_UARTEx_ReceiveToIdle_DMA(&huart6, UART_debug_rx, UART6_RX_LENGTH);
  __HAL_DMA_DISABLE_IT(&hdma_usart6_rx, DMA_IT_HT);

  /* Infinite loop */
  for(;;)
  {
    // Ожидаем сообщение из очереди
    osMessageGet(rxDataUART6Handle, osWaitForever);

    // Парсим JSON
    cJSON *json = cJSON_Parse((char *)message_rx);
    if (json != NULL)
    {
      cJSON *id = cJSON_GetObjectItemCaseSensitive(json, "id");
      cJSON *type_data = cJSON_GetObjectItemCaseSensitive(json, "type_data");
      cJSON *save_settings = cJSON_GetObjectItemCaseSensitive(json, "save_settings");
      cJSON *obj = cJSON_GetObjectItemCaseSensitive(json, "obj");

      if (cJSON_IsNumber(id))
      {
        uint32_t received_id = (uint32_t)cJSON_GetNumberValue(id);
        
        // Принимаем сообщения для своего ID или широковещательные (id=0)
        if (received_id == ID_CTRL || received_id == 0)
        {
        bool save_set = false;
        if (cJSON_IsTrue(save_settings))
        {
          save_set = true;
        }

        if (cJSON_IsNumber(type_data))
        {
          switch (type_data->valueint)
          {
          case 1: // ip settings
          {
            action_ip(obj, save_set);
            break;
          }
          case 3: // commands
          {
            // Обработка команд управления
            action_commands(obj);
            break;
          }
          case 4: // get settings
          {
            action_settings_data(obj);
            break;
          }
          default:
          {
            STM_LOG("data type not registered");
            break;
          }
          }
        }
        else
        {
          STM_LOG("Invalid type data");
        }
        }
        else
        {
          STM_LOG("id not valid");
        }
      }
      else
      {
        STM_LOG("id not valid");
      }

      cJSON_Delete(json);
    }
    else
    {
      STM_LOG("Invalid JSON");
    }
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
        PWM_FadeTick();
        osDelay(FADE_TICK_MS);
    }
}

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

// Обработка команд управления от хоста (type_data=3)
void action_commands(cJSON *obj)
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
				case 0: PWM_DisableAll();     break;
				case 1: PWM_EnableAll();      break;
				case 3: PWM_EnableFadeAll();  break;
				case 4: PWM_DisableFadeAll(); break;
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
								case 0: PWM_Disable((PWM_Channel_t)ch_num);     break;
								case 1: PWM_Enable((PWM_Channel_t)ch_num);      break;
								case 3: PWM_EnableFade((PWM_Channel_t)ch_num);  break;
								case 4: PWM_DisableFade((PWM_Channel_t)ch_num); break;
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
	case 7: // Управление временем плавного перехода (fade duration)
	{
		cJSON *duration_ms = cJSON_GetObjectItemCaseSensitive(obj, "duration_ms");
		if (cJSON_IsNumber(duration_ms)) {
			int ms = duration_ms->valueint;
			if (ms >= 50 && ms <= 10000) {
				PWM_SetFadeDuration((uint16_t)ms);
				STM_LOG("Fade duration set to %d ms", ms);
			}
		}
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
			STM_LOG_xx("%s", resp_str);
			STM_LOG_xx("\x03\x04");
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
			STM_LOG_xx("%s", resp_str);
			STM_LOG_xx("\x03\x04");
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
			STM_LOG_xx("%s", resp_str);
			STM_LOG_xx("\x03\x04");
			cJSON_free(resp_str);
			cJSON_Delete(response);
			
			STM_LOG("Calibration reset");
		}
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
void action_settings_data(cJSON *obj)
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
		cJSON_AddBoolToObject(temp_obj, "enabled", pwm_controller.channels[ch].enabled);

		cJSON_AddItemToArray(obj_ch, temp_obj);
	}

	cJSON_AddItemToObject(j_all_settings_obj, "obj_ch", obj_ch);
	
	// Добавляем текущее значение тока с датчика ACS712 в obj_ip
	int32_t current_ma = (int32_t)(g_current_amperes * 1000.0f);
	cJSON_AddNumberToObject(obj_ip, "current_ma", current_ma);
	
	cJSON_AddItemToObject(j_all_settings_obj, "obj_ip", obj_ip);

	char *str_to_host = cJSON_Print(j_all_settings_obj);

	STM_LOG("Отправка настроек: %s", str_to_host);

    // Проверяем размер строки и отправляем частями если нужно
    size_t total_len = strlen(str_to_host);
    const size_t chunk_size = MAX_MESSAGE_SIZE - 3;

    if (total_len <= chunk_size) {
        // Отправляем целиком если размер не превышает MAX_MESSAGE_SIZE
        STM_LOG_xx("%s", str_to_host);
        STM_LOG_xx("\x03\x04"); // ETX + EOT
    } else {
        // Отправляем частями
        size_t offset = 0;
        while (offset < total_len) {
            size_t current_chunk_size = (total_len - offset > chunk_size) ? chunk_size : (total_len - offset);

            // Временно заменяем символ для создания null-terminated блока
            char saved_char = str_to_host[offset + current_chunk_size];
            str_to_host[offset + current_chunk_size] = '\0';

            // Отправляем блок данных
            STM_LOG_xx("%s", str_to_host + offset);

            // Восстанавливаем символ
            str_to_host[offset + current_chunk_size] = saved_char;

            offset += current_chunk_size;
            //osDelay(10);
        }
        STM_LOG_xx("\x03\x04"); // ETX + EOT - отправляем только по завершении всей передачи
    }

    //osDelay(10);
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
