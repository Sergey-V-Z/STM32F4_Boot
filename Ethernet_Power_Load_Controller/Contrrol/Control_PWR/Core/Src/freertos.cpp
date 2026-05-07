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
#include "api.h"
#include "device_API.h"
#include "tcp_server.h"
#include "firmware_update.h"
#include "protocol.h"
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
extern chName_t NameCH[MAX_CH_NAME];
uint16_t sensBuff[8] = {0};
uint8_t sensState = 255; // битовое поле
extern bool rx_end;

// Количество найденных устройств (обновляется при автопоиске)
uint32_t pcs_dev = 0;

uint16_t transaction_id;

uint32_t freqSens = HAL_RCC_GetHCLKFreq() / 30000u;
uint32_t pwmSens;

uint16_t adc_buffer[24] = {0};
uint16_t adc_buffer2[24] = {0};

uint8_t UART_tx[UART_TX_LENGTH] = {0}; // buff
uint8_t UART_rx[UART_RX_LENGTH] = {0}; // buff

extern led_t LED_IPadr;
extern led_t LED_error;
extern led_t LED_OSstart;

void action_ip(cJSON *obj, bool save);
void action_ch_set(cJSON *obj);
void action_cmd(cJSON *obj);
void action_settings_data(cJSON *obj);
void action_channels_data(cJSON *obj);
void action_bridge(cJSON *obj, bool save);
void action_bridge_data(cJSON *obj);

// Функция автоматического поиска и инициализации устройств
void devices_auto_discover_and_init();
// структуры для netcon
extern struct netif gnetif;

// TCP_IP
static char strIP[20];

// обмен данными с компом
extern uint8_t message_rx[message_RX_LENGTH];
extern uint8_t UART_debug_rx[UART6_RX_LENGTH];
extern uint16_t indx_message_rx;
extern uint16_t indx_UART6_rx;
extern uint16_t Size_message;
extern uint16_t Start_index;

// TCP for ModBUS
uint8_t rtu_data[256] = {0};
uint8_t response[260] = {0};
uint8_t Modbut_to_TCP[260] = {0};
uint16_t SizeInModBus = 0;
struct netconn connectionForModBUS, newconnectionForModBUS;
struct netconn *connMB = &connectionForModBUS, *newconnMB = &newconnectionForModBUS; // contains info about connection inc. type, port, buf pointers etc.

// переменные переферии
uint32_t Start = 0;
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern flash mem_spi;

SecondaryFirmwareConfig secondaryConfig;
// переменные для тестов

uint8_t txRedy = 1;
// мютекс для защиты доступа к устройствам
osMutexId deviceMutexHandle;
osMutexId varMutexDevicesHandle;

/* USER CODE END Variables */
osThreadId MainTaskHandle;
osThreadId LEDHandle;
osThreadId ethTasHandle;
osThreadId MBRTUTaskHandle;
osThreadId MBETHTaskHandle;
osThreadId uart_taskHandle;
osThreadId loggerTaskHandle;
osMessageQId rxDataUART2Handle;
osMessageQId rxDataUART1Handle;
osSemaphoreId ADC_endHandle;
osSemaphoreId ADC_end2Handle;
osSemaphoreId Resive_USARTHandle;
osSemaphoreId mulicom_uartHandle;

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
// extern "C"
/* USER CODE END FunctionPrototypes */

void mainTask(void const *argument);
void led(void const *argument);
void eth_Task(void const *argument);
void mbrtuTask(void const *argument);
void mbethTask(void const *argument);
void uart_Task(void const *argument);
void LoggerTask(void const *argument);

extern void MX_LWIP_Init(void);
void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* GetIdleTaskMemory prototype (linked to static allocation support) */
extern "C" void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize);

/* USER CODE BEGIN GET_IDLE_TASK_MEMORY */
static StaticTask_t xIdleTaskTCBBuffer;
static StackType_t xIdleStack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize)
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
void MX_FREERTOS_Init(void)
{
	/* USER CODE BEGIN Init */

	/* USER CODE END Init */

	/* USER CODE BEGIN RTOS_MUTEX */
	/* add mutexes, ... */
	osMutexDef(deviceMutex);
	deviceMutexHandle = osMutexCreate(osMutex(deviceMutex));

	osMutexDef(varMutexDevices);
	varMutexDevicesHandle = osMutexCreate(osMutex(varMutexDevices));
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
	/* USER CODE END RTOS_SEMAPHORES */

	/* USER CODE BEGIN RTOS_TIMERS */
	/* start timers, add new ones, ... */
	/* USER CODE END RTOS_TIMERS */

	/* Create the queue(s) */
	/* definition and creation of rxDataUART2 */
	osMessageQDef(rxDataUART2, 16, uint8_t);
	rxDataUART2Handle = osMessageCreate(osMessageQ(rxDataUART2), NULL);

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
	/* USER CODE END RTOS_THREADS */
}

//=============================================================================
// Функция автоматического поиска и инициализации устройств
//=============================================================================

// Автоматический поиск и инициализация устройств при старте
void devices_auto_discover_and_init() {
    STM_LOG("=== Device Auto-Discovery Start ===");
    
    // Выполняем поиск устройств через UART
    uint32_t found_count = auto_search_dev(devices, MAX_ADR_DEV);
    pcs_dev = found_count; // Обновляем глобальную переменную
    STM_LOG("Found %u devices via UART", found_count);
    
    // Выводим информацию о найденных устройствах
    for (uint32_t i = 0; i < found_count && i < MAX_ADR_DEV; i++) {
        const char* type_name = "Unknown";
        if      (devices[i].TypePCB == LED_DRV)    type_name = "LED_DRV";
        else if (devices[i].TypePCB == LED_DRV_v2) type_name = "LED_DRV_v2";
        else if (devices[i].TypePCB == PCB_PWR)    type_name = "PCB_PWR";
        else if (devices[i].TypePCB == MOSFET_3CH) type_name = "MOSFET_3CH";
        else if (devices[i].TypePCB == MOSFET_6CH) type_name = "MOSFET_6CH";
        
        STM_LOG("  Device %u: Addr=%u, Type=%s", i, devices[i].Addr, type_name);
    }
    
    // Создаем привязку каналов (NameCH[])
    osMutexWait(varMutexDevicesHandle, osWaitForever);
    
    for (uint32_t var = 0; var < found_count; ++var) {
        if ((devices[var].Addr >= START_ADR_DEV) && 
            (devices[var].Addr <= (START_ADR_DEV + MAX_ADR_DEV))) {
            
            for (int i = 0; i < 3; ++i) {
                NameCH[devices[var].ch[i].Name_ch].dev = &devices[var];
                NameCH[devices[var].ch[i].Name_ch].Channel_number = i;
            }
        }
    }
    
    osMutexRelease(varMutexDevicesHandle);
    
    STM_LOG("=== Device Auto-Discovery Complete ===");
}

//=============================================================================

/* USER CODE BEGIN Header_mainTask */
/**
 * @brief  Function implementing the MainTask thread.
 * @param  argument: Not used
 * @retval None
 */
/* USER CODE END Header_mainTask */
void mainTask(void const *argument)
{
	/* init code for LWIP */
	MX_LWIP_Init();
	/* USER CODE BEGIN mainTask */

	// нициализируем логгер
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

	HAL_StatusTypeDef status1;
	// uint8_t channelForName = 0;
	uint16_t Address = 0;
	uint8_t *rx_data = NULL;

	HAL_UARTEx_ReceiveToIdle_DMA(&huart1, UART_rx, UART_RX_LENGTH);
	__HAL_DMA_DISABLE_IT(&hdma_usart1_rx, DMA_IT_HT);

	// Автоматический поиск и инициализация устройств
	// Функция сама выполнит: поиск → загрузку из флеш → сравнение → сохранение при необходимости
	devices_auto_discover_and_init();

	/* Infinite loop */
	for (;;)
	{

		osMutexWait(varMutexDevicesHandle, osWaitForever);

		for (int var = 0; var < pcs_dev; ++var)
		{

			// мютекс зашитв переменной один для обоих devices и NameCH

			// если адрес устройства валидный
			if (devices[var].Addr == 0)
			{
				continue;
			}

			exchange_channel_data_with_device(&devices[var]);

			// osDelay(10);
		}
		osMutexRelease(varMutexDevicesHandle);

		osDelay(10);

		// обновление прошивки если нужно — не чаще одного раза в 30 секунд,
		// чтобы не создавать конкуренцию за rxDataUART1Handle с exchange_channel_data_with_device
		static uint32_t lastFwCheckTick = 0;
		if ((HAL_GetTick() - lastFwCheckTick) >= 30000U)
		{
			lastFwCheckTick = HAL_GetTick();
			FirmwareUpdate_Secondary(devices, pcs_dev);
		}
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
void led(void const *argument)
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

	// uint32_t tickcount = osKernelSysTick();// переменная для точной задержки
	/* Infinite loop */
	for (;;)
	{
		LED_IPadr.poll();
		LED_error.poll();
		LED_OSstart.poll();

		osDelay(1);
		// taskYIELD();
		// osDelayUntil(&tickcount, 1); // задача будет вызываься ровро через 1 милисекунду
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
void eth_Task(void const *argument)
{
	/* USER CODE BEGIN eth_Task */

	while (gnetif.ip_addr.addr == 0)
	{
		osDelay(1);
	} // ждем получение адреса
	LED_IPadr.LEDon();
	osDelay(1000);
	LED_IPadr.LEDoff();
	strncpy(strIP, ip4addr_ntoa(&gnetif.ip_addr), sizeof(strIP) - 1U);
	strIP[sizeof(strIP) - 1U] = '\0';

	// структуры для netcon
	struct netconn *conn;
	struct netconn *newconn;
	struct netbuf *netbuf;
	volatile err_t err, accept_err;
	// ip_addr_t local_ip;
	// ip_addr_t remote_ip;
	void *in_data = NULL;
	uint16_t data_size = 0;

	/* Infinite loop */
	for (;;)
	{

		conn = netconn_new(NETCONN_TCP);
		if (conn != NULL)
		{
			err = netconn_bind(conn, NULL, 81); // assign port number to connection
			if (err == ERR_OK)
			{
				netconn_listen(conn); // set port to listening mode
				while (1)
				{
					accept_err = netconn_accept(conn, &newconn); // suspend until new connection
					if (accept_err == ERR_OK)
					{
						LED_IPadr.LEDon();
						STM_LOG("Connect open");
						while ((accept_err = netconn_recv(newconn, &netbuf)) == ERR_OK) // работаем до тех пор пока клиент не разорвет соеденение
						{

							do
							{
								netbuf_data(netbuf, &in_data, &data_size); // get pointer and data size of the buffer
						/*-----------------------------------------------------------------------------------------------------------------------------*/
						static char in_buf[512];
						static char resp_buf[DEVICE_API_RESP_SIZE];
						size_t safe_sz = (data_size < 511U) ? data_size : 511U;
						memcpy(in_buf, in_data, safe_sz);
						in_buf[safe_sz] = '\0';
						STM_LOG("Get CMD %s", in_buf);
						if (safe_sz > 0U)
						{
							resp_buf[0] = '\0';
							Command_execution(in_buf, resp_buf, sizeof(resp_buf));
							netconn_write(newconn, resp_buf, strlen(resp_buf), NETCONN_COPY);
						}

						} while (netbuf_next(netbuf) >= 0);
						netbuf_delete(netbuf);
						}
						netconn_close(newconn);
						netconn_delete(newconn);
						STM_LOG("Connect close");
						LED_IPadr.LEDoff();
					}
					else
						netconn_delete(newconn);
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
void mbrtuTask(void const *argument)
{
	/* USER CODE BEGIN mbrtuTask */
	HAL_StatusTypeDef status1;

	status1 = HAL_UARTEx_ReceiveToIdle_IT(settings.bridge_sett.RS485, response, 256); // Read data
																					  // uint16_t usCRC16;
	/* Infinite loop */
	for (;;)
	{

		// ждем event от USART
		osSemaphoreWait(Resive_USARTHandle, osWaitForever);
		// если соеденение все еще установленно
		if (newconnMB->type == netconn_type::NETCONN_TCP)
		{

			// Засекаем время получения RTU ответа
			mb_timing.rtu_rx_start = HAL_GetTick();
			mb_timing.rtu_response_time = get_elapsed_ms(mb_timing.rtu_tx_start);

			switch (settings.bridge_sett.mode_rs485)
			{
			case RTU:
			{
				// Засекаем время начала отправки TCP
				mb_timing.tcp_tx_start = HAL_GetTick();
				mb_timing.rtu_to_tcp_time = get_elapsed_ms(mb_timing.rtu_rx_start);

				int tcp_length = rtu_to_tcp(response, SizeInModBus, Modbut_to_TCP, transaction_id);

				// Выводим статистику времени
				STM_LOG("Timing stats (ms): TCP->RTU: %lu, RTU response: %lu, RTU->TCP: %lu",
						mb_timing.tcp_to_rtu_time,
						mb_timing.rtu_response_time,
						mb_timing.rtu_to_tcp_time);

				if (tcp_length > 0)
				{
					// print_packet(Modbut_to_TCP, tcp_length, "Converted back to TCP");
				}
				else if (tcp_length == -2)
				{
					STM_LOG(LOG_ERR "RTU to TCP failed - CRC\n");
				}
				else
				{
					STM_LOG(LOG_ERR "RTU to TCP failed - packet length\n");
				}
				netconn_write(newconnMB, Modbut_to_TCP, tcp_length,
							  NETCONN_COPY);
				break;
			}
			case STREAMER:
			{
				netconn_write(newconnMB, response, SizeInModBus, NETCONN_COPY);
				break;
			}
			default:
			{
				STM_LOG(LOG_ERR "Unknown mode");
				break;
			}
			}

			SizeInModBus = 0;
		}
		else
		{
			SizeInModBus = 0;
		}

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
void mbethTask(void const *argument)
{
	/* USER CODE BEGIN mbethTask */
	while (gnetif.ip_addr.addr == 0)
	{
		osDelay(1);
	} // ждем получение адреса

	// TCP connection vars
	err_t err, accept_err;
	struct netbuf buffer;
	struct netbuf *buf = &buffer; // bufferized input data
	void *in_data = NULL;
	uint16_t data_size = 0;
	// uint8_t			mb_tcp_head[6];
	// uint16_t usCRC16;
	// osSemaphoreWait(ModBusEndHandle,1000);
	// int32_t SemRet = 0;
	// sizeH = xPortGetMinimumEverFreeHeapSize();
	/* Infinite loop */
	for (;;)
	{
		connMB = netconn_new(NETCONN_TCP);
		if (connMB != NULL)
		{
			err = netconn_bind(connMB, NULL, 502); // assign port number to connection
			if (err == ERR_OK)
			{
				netconn_listen(connMB); // set port to listening mode
				while (1)
				{
					accept_err = netconn_accept(connMB, &newconnMB); // suspend until new connection
					if (accept_err == ERR_OK)
					{
						while (netconn_recv(newconnMB, &buf) == ERR_OK) // suspend until data received
						{
							do
							{
								netbuf_data(buf, &in_data, &data_size); // get pointer and data size of the buffer

								// Засекаем время начала обработки TCP
								mb_timing.tcp_to_rtu_start = HAL_GetTick();

								// вырезать данные для модбаса
								// memcpy(mb_tcp_head, in_data, 6); // копируем заголовок
								// memcpy((void*)input_tcp_data, ((uint8_t*)in_data)+6, data_size-6);

								// uint16_t len = mb_tcp_head[4] << 8;
								// len |= mb_tcp_head[5];

								// проверить пакет на длинну
								// if( len >= 254 ){
								////netconn_write(newconnMB,response,4,NETCONN_COPY);
								// return;
								//}

								// расчитать crc
								// usCRC16 = usMBCRC16(input_tcp_data,data_size-6);
								// input_tcp_data[data_size-6] = ( uint8_t )( usCRC16 & 0xFF );
								// input_tcp_data[data_size-5] = ( uint8_t )( usCRC16 >> 8 );

								switch (settings.bridge_sett.mode_rs485)
								{
								case RTU:
								{
									// print_packet((uint8_t*)in_data, sizeof(in_data), "Original TCP packet");

									int rtu_length = tcp_to_rtu((uint8_t *)in_data, data_size, rtu_data, &transaction_id);
									if (rtu_length > 0)
									{
										// print_packet(rtu_data, rtu_length, "Converted RTU packet");

										// Проверяем CRC полученного RTU пакета
										if (verify_rtu_crc(rtu_data, rtu_length))
										{
											// STM_LOG("RTU CRC check: OK\n");
										}
										else
										{
											STM_LOG("RTU CRC check: FAILED\n");
										}

										// Засекаем время перед отправкой RTU
										mb_timing.rtu_tx_start = HAL_GetTick();
										mb_timing.tcp_to_rtu_time = get_elapsed_ms(mb_timing.tcp_to_rtu_start);

										// отправить
										HAL_GPIO_WritePin(DE_M_GPIO_Port, DE_M_Pin, GPIO_PIN_SET); // включить на передачу
										// HAL_UART_Transmit(bridge_sett.RS485, input_tcp_data, data_size, 100); //Отправляем данные в USART
										HAL_UART_Transmit(settings.bridge_sett.RS485, rtu_data, rtu_length, 100); // Отправляем данные в USART
										HAL_GPIO_WritePin(DE_M_GPIO_Port, DE_M_Pin, GPIO_PIN_RESET);			  // включить на прием
									}
									else
									{
										printf("Error: TCP to RTU failed\n");
									}

									break;
								}
								case STREAMER:
								{
									// отправить
									HAL_GPIO_WritePin(DE_M_GPIO_Port, DE_M_Pin, GPIO_PIN_SET); // включить на передачу
									// HAL_UART_Transmit(bridge_sett.RS485, input_tcp_data, data_size, 100); //Отправляем данные в USART
									HAL_UART_Transmit(settings.bridge_sett.RS485, (uint8_t *)in_data, data_size, 100); // Отправляем данные в USART
									HAL_GPIO_WritePin(DE_M_GPIO_Port, DE_M_Pin, GPIO_PIN_RESET);					   // включить на прием

									break;
								}
								default:
								{
									STM_LOG(LOG_ERR "Unknown mode");
									break;
								}
								}
							} while (netbuf_next(buf) >= 0);
							netbuf_delete(buf);
						}
						netconn_close(newconnMB);
						netconn_delete(newconnMB);
					}
					else
						netconn_delete(newconnMB);
					osDelay(1);
				}
			}
		}
		osDelay(10);
	}
	/* USER CODE END mbethTask */
}

/* USER CODE BEGIN Header_uart_Task */
/**
 * связь с копрютером обработка пришедших сообщений
 * @brief Function implementing the uart_task thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_uart_Task */
void uart_Task(void const *argument)
{
	/* USER CODE BEGIN uart_Task */
	// HAL_UART_Receive_DMA(bridge_sett.RS485, UART2_rx, UART2_RX_LENGTH);
	HAL_UARTEx_ReceiveToIdle_DMA(&huart6, UART_debug_rx, UART6_RX_LENGTH);
	__HAL_DMA_DISABLE_IT(&hdma_usart6_rx, DMA_IT_HT);
	//__HAL_DMA_DISABLE_IT(&hdma_usart2_rx, DMA_IT_TC);
	/* Infinite loop */
	for (;;)
	{
		// ожидать собщение
		osEvent evt = osMessageGet(rxDataUART2Handle, 5000);

		if (evt.status == osEventMessage)
		{
		// мигание LED при получении сообщения по UART
		LED_error.LEDon(1);

		// парсим  json
		cJSON *json = cJSON_Parse((char *)message_rx);
		if (json != NULL)
		{
			cJSON *id = cJSON_GetObjectItemCaseSensitive(json, "id");
			// cJSON *name_device = cJSON_GetObjectItemCaseSensitive(json, "name_device");
			cJSON *type_data = cJSON_GetObjectItemCaseSensitive(json, "type_data");
			cJSON *save_settings = cJSON_GetObjectItemCaseSensitive(json, "save_settings");
			cJSON *obj = cJSON_GetObjectItemCaseSensitive(json, "obj");

			if (cJSON_IsNumber(id) && cJSON_GetNumberValue(id) == ID_CTRL)
			{
				bool save_set = false;
				if (cJSON_IsTrue(save_settings))
				{
					save_set = true;
				}
				else
				{
					save_set = false;
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
					case 2: // chanels settings
					{
						action_ch_set(obj);
						break;
					}
					case 3:
					{
						action_cmd(obj);
						break;
					}
					case 4:
					{
						action_settings_data(obj);
						// STM_LOG("Empty type_data num");
						break;
					}
					case 7: // Запрос данных каналов с пагинацией
					{
						action_channels_data(obj);
						break;
					}
					case 5:
					{
						action_bridge(obj, save_set);
						// STM_LOG("Empty type_data num");
						break;
					}
					case 6:
					{
						action_bridge_data(obj);
						// STM_LOG("Empty type_data num");
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

			cJSON_Delete(json);
		}
		else
		{
			STM_LOG("Invalid JSON");
		}
		} else if (evt.status == osEventTimeout) {
			// таймаут - продолжаем ожидание
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
void LoggerTask(void const *argument)
{
	/* USER CODE BEGIN LoggerTask */
	/* Infinite loop */
	for (;;)
	{
		Logger_Process();
		osDelay(1);
	}
	/* USER CODE END LoggerTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

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
	if ((j_setIP != NULL) && cJSON_IsTrue(j_setIP) && (j_IP != NULL) && cJSON_IsString(j_IP))
	{
		const char *s = j_IP->valuestring;
		if (s && *s)
		{
			unsigned int a, b, c, d;
			if (sscanf(s, "%u.%u.%u.%u", &a, &b, &c, &d) == 4 &&
				a <= 255U && b <= 255U && c <= 255U && d <= 255U)
			{
				settings.saveIP.ip[0] = (uint8_t)a; settings.saveIP.ip[1] = (uint8_t)b;
				settings.saveIP.ip[2] = (uint8_t)c; settings.saveIP.ip[3] = (uint8_t)d;
				settingsChanged = true;
			}
			else
			{
				STM_LOG(LOG_ERR "Invalid IP address format");
			}
		}
	}

	// Обработка MAC
	if ((j_setMAC != NULL) && cJSON_IsTrue(j_setMAC) && (j_MAC != NULL) && cJSON_IsString(j_MAC))
	{
		const char *s = j_MAC->valuestring;
		if (s && *s)
		{
			unsigned int a,b,c,d,e,f;
			if (sscanf(s, "%x:%x:%x:%x:%x:%x", &a,&b,&c,&d,&e,&f) == 6 &&
				a<=255U && b<=255U && c<=255U && d<=255U && e<=255U && f<=255U)
			{
				settings.MAC[0]=(uint8_t)a; settings.MAC[1]=(uint8_t)b;
				settings.MAC[2]=(uint8_t)c; settings.MAC[3]=(uint8_t)d;
				settings.MAC[4]=(uint8_t)e; settings.MAC[5]=(uint8_t)f;
				settingsChanged = true;
			}
			else
			{
				STM_LOG(LOG_ERR "Invalid MAC address format");
			}
		}
	}

	// Обработка GATEWAY
	if ((j_setGATEWAY != NULL) && cJSON_IsTrue(j_setGATEWAY) && (j_GATEWAY != NULL) && cJSON_IsString(j_GATEWAY))
	{
		const char *s = j_GATEWAY->valuestring;
		if (s && *s)
		{
			unsigned int a, b, c, d;
			if (sscanf(s, "%u.%u.%u.%u", &a, &b, &c, &d) == 4 &&
				a <= 255U && b <= 255U && c <= 255U && d <= 255U)
			{
				settings.saveIP.gateway[0] = (uint8_t)a; settings.saveIP.gateway[1] = (uint8_t)b;
				settings.saveIP.gateway[2] = (uint8_t)c; settings.saveIP.gateway[3] = (uint8_t)d;
				settingsChanged = true;
			}
			else
			{
				STM_LOG(LOG_ERR "Invalid gateway address format");
			}
		}
	}

	// Обработка MASK
	if ((j_setMASK != NULL) && cJSON_IsTrue(j_setMASK) && (j_MASK != NULL) && cJSON_IsString(j_MASK))
	{
		const char *s = j_MASK->valuestring;
		if (s && *s)
		{
			unsigned int a, b, c, d;
			if (sscanf(s, "%u.%u.%u.%u", &a, &b, &c, &d) == 4 &&
				a <= 255U && b <= 255U && c <= 255U && d <= 255U)
			{
				settings.saveIP.mask[0] = (uint8_t)a; settings.saveIP.mask[1] = (uint8_t)b;
				settings.saveIP.mask[2] = (uint8_t)c; settings.saveIP.mask[3] = (uint8_t)d;
				settingsChanged = true;
			}
			else
			{
				STM_LOG(LOG_ERR "Invalid subnet mask format");
			}
		}
	}

	// Обработка DNS
	if ((j_setDNS != NULL) && cJSON_IsTrue(j_setDNS) && (j_DNS != NULL) && cJSON_IsString(j_DNS))
	{
		(void)j_DNS; /* DNS не реализован */
	}

	// Обработка DHCP
	if ((j_setDHCP != NULL) && cJSON_IsTrue(j_setDHCP) && (j_DHCP != NULL))
	{
		settings.DHCPset = cJSON_IsTrue(j_DHCP) ? 1 : 0;
		settingsChanged = true;
	}

	if (settingsChanged)
	{
		STM_LOG("Settings set successful");

		// Сохранение настроек если требуется
		if (save)
		{
			if (mem_spi.Write(settings))
			{
				STM_LOG("Save settings OK. size %d", sizeof(settings_t));
			}
			else
			{
				STM_LOG("Save settings FAIL");
			}
		}
	}
	else
	{
		STM_LOG("No settings were changed");
	}
}

// возвращает список каналов
void action_ch_set(cJSON *obj)
{
	// STM_LOG("Auto set channels start");
	if (settings.devices_depth != 0)
	{
		cJSON *j_out_obj = cJSON_CreateObject();
		cJSON *j_arr_obj = cJSON_CreateArray();

		// mutex
		osMutexWait(varMutexDevicesHandle, osWaitForever);

		// заполнение массива каналов
		for (int var = 0; var < settings.devices_depth; ++var)
		{
			cJSON *temp_obj = cJSON_CreateObject();
			uint8_t c = NameCH[var].Channel_number;
			cJSON_AddNumberToObject(temp_obj, "num", NameCH[var].dev->ch[c].Name_ch);
			cJSON_AddNumberToObject(temp_obj, "dev_addr", NameCH[var].dev->Addr);
			cJSON_AddNumberToObject(temp_obj, "ch_dev", c);

			cJSON_AddItemToArray(j_arr_obj, temp_obj);
		}
		// mutex
		osMutexRelease(varMutexDevicesHandle);

		cJSON_AddNumberToObject(j_out_obj, "id", ID_CTRL);
		cJSON_AddStringToObject(j_out_obj, "name_device", NAME);
		cJSON_AddNumberToObject(j_out_obj, "type_data", 2);
		cJSON_AddItemToObject(j_out_obj, "obj", j_arr_obj);

		char *out_str = cJSON_Print(j_out_obj);
		STM_LOG("%s", out_str);

		free(out_str);
		// cJSON_Delete(j_arr_obj);
		cJSON_Delete(j_out_obj);
		// STM_LOG("Auto set channels OK");
	}
	else
	{
		STM_LOG("empty channels");
	}
}

// Обработка команд управления
void action_cmd(cJSON *obj)
{
	cJSON *id_cmd = cJSON_GetObjectItemCaseSensitive(obj, "id_cmd");

	int key = cJSON_GetNumberValue(id_cmd);

	switch (key)
	{
	case 1:
	{ // auto set
		cJSON *Addr_start = cJSON_GetObjectItemCaseSensitive(obj, "Addr_start");
		cJSON *Count_dev = cJSON_GetObjectItemCaseSensitive(obj, "Count_dev");

		int addres = cJSON_GetNumberValue(Addr_start);
		int count = cJSON_GetNumberValue(Count_dev);

		STM_LOG("Manual device auto-fill triggered");
		
		// Выполняем автопоиск и инициализацию через новую систему
		devices_auto_discover_and_init();

		// Сохраняем основные настройки
		if (mem_spi.Write(settings))
		{
			STM_LOG("Save settings OK. size %d", sizeof(settings_t));
		}
		else
		{
			STM_LOG("Save settings FAIL");
		}

		STM_LOG("auto set end");
		break;
	}
	case 2:
	{ // add device
		cJSON *Num = cJSON_GetObjectItemCaseSensitive(obj, "Num");
		cJSON *Dev_addr = cJSON_GetObjectItemCaseSensitive(obj, "Dev_addr");
		cJSON *CH_dev = cJSON_GetObjectItemCaseSensitive(obj, "CH_dev");

		int num = cJSON_GetNumberValue(Num);
		int dev_addr = cJSON_GetNumberValue(Dev_addr);
		int ch_dev = cJSON_GetNumberValue(CH_dev);

		/*
		int ret = set_i2c_dev(dev_addr, ch_dev, num);
		switch (ret) {
		case 1:
			STM_LOG("Error not valid chanel data");
			break;
		case 2:
			STM_LOG("Error not empty cell");
			break;
		case 3:
			STM_LOG("Error not valid addres");
			break;
		case 4:
			STM_LOG("err not empty cell");
			break;

		}*/

		break;
	}
	case 3:
	{ // del device
		// cJSON *Num = cJSON_GetObjectItemCaseSensitive(obj, "Num");
		// int num = cJSON_GetNumberValue(Num);

		// del_Name_dev(num);
		STM_LOG("err empty cmd");
		break;
	}
	case 4:
	{ // on_off chanel
		cJSON *Num = cJSON_GetObjectItemCaseSensitive(obj, "Num");
		cJSON *PWM = cJSON_GetObjectItemCaseSensitive(obj, "PWM");
		cJSON *On = cJSON_GetObjectItemCaseSensitive(obj, "On");
		cJSON *All = cJSON_GetObjectItemCaseSensitive(obj, "All");

		int num = cJSON_GetNumberValue(Num);
		int pwm = cJSON_GetNumberValue(PWM);
		bool on = cJSON_IsTrue(On);
		bool all = cJSON_IsTrue(All);

		// mutex
		osMutexWait(varMutexDevicesHandle, osWaitForever);

		// mode set all channels
		if (all)
		{
			for (int name = 0; name < MAX_CH_NAME; ++name)
			{
				if (NameCH[name].dev != NULL)
				{
					uint8_t c = NameCH[name].Channel_number; // get channel number for this name
					NameCH[name].dev->ch[c].PWM_out = pwm;
					NameCH[name].dev->ch[c].On_off = on;
				}
			}
			STM_LOG("OK");
		}
		else
		{
			// mode set one channel
			if (NameCH[num].dev != NULL)
			{
				uint8_t c = NameCH[num].Channel_number; // get channel number for this name
				NameCH[num].dev->ch[c].PWM_out = pwm;
				NameCH[num].dev->ch[c].On_off = on;
				STM_LOG("OK");
			}
			else
			{
				STM_LOG("NULL ptr dev");
			}
		}
		// mutex
		osMutexRelease(varMutexDevicesHandle);
		break;
	}
	case 5:
	{ // reboot
		STM_LOG("Rebooting...");
		osDelay(3000);
		NVIC_SystemReset();
		break;
	}
	default:
	{
		STM_LOG("Error id_cmd");
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
	char srtIP_to_host[20];
	snprintf(srtIP_to_host, sizeof(srtIP_to_host), "%u.%u.%u.%u",
			 settings.saveIP.ip[0], settings.saveIP.ip[1],
			 settings.saveIP.ip[2], settings.saveIP.ip[3]);
	cJSON_AddStringToObject(obj_ip, "IP", srtIP_to_host);

	char srtMAC_to_host[20];
	snprintf(srtMAC_to_host, sizeof(srtMAC_to_host), "%x:%x:%x:%x:%x:%x",
			 settings.MAC[0], settings.MAC[1], settings.MAC[2],
			 settings.MAC[3], settings.MAC[4], settings.MAC[5]);
	cJSON_AddStringToObject(obj_ip, "MAC", srtMAC_to_host);

	char srtGATEWAY_to_host[20];
	snprintf(srtGATEWAY_to_host, sizeof(srtGATEWAY_to_host), "%u.%u.%u.%u",
			 settings.saveIP.gateway[0], settings.saveIP.gateway[1],
			 settings.saveIP.gateway[2], settings.saveIP.gateway[3]);
	cJSON_AddStringToObject(obj_ip, "GATEWAY", srtGATEWAY_to_host);

	char srtMASK_to_host[20];
	snprintf(srtMASK_to_host, sizeof(srtMASK_to_host), "%u.%u.%u.%u",
			 settings.saveIP.mask[0], settings.saveIP.mask[1],
			 settings.saveIP.mask[2], settings.saveIP.mask[3]);
	cJSON_AddStringToObject(obj_ip, "MASK", srtMASK_to_host);

	cJSON_AddStringToObject(obj_ip, "DNS", "0.0.0.0");

	if (settings.DHCPset)
	{
		cJSON_AddTrueToObject(obj_ip, "DHCP");
	}
	else
	{
		cJSON_AddFalseToObject(obj_ip, "DHCP");
	}

	// Подсчитываем количество активных каналов (без отправки данных)
	int active_channels = 0;
	for (int name = 0; name < MAX_CH_NAME; ++name) {
		if (NameCH[name].dev != NULL) {
			active_channels++;
		} else {
			break; // первый пустой канал означает конец списка
		}
	}

	// Создаем объект с метаданными о каналах (вместо отправки всех данных)
	cJSON *obj_ch_meta = cJSON_CreateObject();
	cJSON_AddNumberToObject(obj_ch_meta, "count", active_channels);
	cJSON_AddNumberToObject(obj_ch_meta, "total", MAX_CH_NAME);
	
	// Добавляем метаданные каналов в основной объект
	cJSON_AddItemToObject(j_all_settings_obj, "obj_ch_meta", obj_ch_meta);
	
	// Очищаем пустой массив obj_ch
	cJSON_Delete(obj_ch);
	cJSON_AddItemToObject(j_all_settings_obj, "obj_ip", obj_ip);

	char *str_to_host = cJSON_Print(j_all_settings_obj);

	// STM_LOG("%s", str_to_host);

	// Проверяем размер строки и отправляем частями если нужно
	size_t total_len = strlen(str_to_host);
	const size_t chunk_size = MAX_MESSAGE_SIZE - 3;

	if (total_len <= chunk_size)
	{
		// Отправляем целиком если размер не превышает MAX_MESSAGE_SIZE
		STM_LOG_xx("%s", str_to_host);
		STM_LOG_xx("\x03\x04"); // ETX + EOT
	}
	else
	{
		// Отправляем частями
		size_t offset = 0;
		while (offset < total_len)
		{
			size_t current_chunk_size = (total_len - offset > chunk_size) ? chunk_size : (total_len - offset);

			// Временно заменяем символ для создания null-terminated блока
			char saved_char = str_to_host[offset + current_chunk_size];
			str_to_host[offset + current_chunk_size] = '\0';

			// Отправляем блок данных
			STM_LOG_xx("%s", str_to_host + offset);

			// Восстанавливаем символ
			str_to_host[offset + current_chunk_size] = saved_char;

			offset += current_chunk_size;
			// osDelay(10);
		}
		STM_LOG_xx("\x03\x04"); // ETX + EOT - отправляем только по завершении всей передачи
	}

	// osDelay(10);
	cJSON_free(str_to_host);
	// cJSON_Delete(obj_ch);
	// cJSON_Delete(obj_ip);
	cJSON_Delete(j_all_settings_obj);
}

// Подготовка и отправка данных о каналах на хост (type_data=7)
void action_channels_data(cJSON *obj)
{
	// Получаем параметры пагинации из запроса
	cJSON *j_offset = cJSON_GetObjectItemCaseSensitive(obj, "offset");
	cJSON *j_limit = cJSON_GetObjectItemCaseSensitive(obj, "limit");
	
	int offset = (j_offset && cJSON_IsNumber(j_offset)) ? j_offset->valueint : 0;
	int limit = (j_limit && cJSON_IsNumber(j_limit)) ? j_limit->valueint : MAX_CH_NAME;
	
	// Ограничиваем размер порции для избежания переполнения буфера
	if (limit > 20) limit = 20; // Максимум 20 каналов за раз
	if (offset < 0) offset = 0;
	if (offset >= MAX_CH_NAME) offset = MAX_CH_NAME - 1;
	
	cJSON *j_response = cJSON_CreateObject();
	cJSON *obj_ch = cJSON_CreateArray();
	
	cJSON_AddNumberToObject(j_response, "id", ID_CTRL);
	cJSON_AddStringToObject(j_response, "name_device", NAME);
	cJSON_AddNumberToObject(j_response, "type_data", 7);
	
	int sent_count = 0;
	int end_index = offset + limit;
	if (end_index > MAX_CH_NAME) end_index = MAX_CH_NAME;
	
	// Формируем данные каналов для запрошенного диапазона
	for (int name = offset; name < end_index && sent_count < limit; ++name) {
		if (NameCH[name].dev != NULL) {
			cJSON *temp_obj = cJSON_CreateObject();
			uint8_t c = NameCH[name].Channel_number;
			
			cJSON_AddNumberToObject(temp_obj, "num", name);
			cJSON_AddNumberToObject(temp_obj, "addr", NameCH[name].dev->Addr);
			cJSON_AddNumberToObject(temp_obj, "ch", c);
			cJSON_AddNumberToObject(temp_obj, "pwm", NameCH[name].dev->ch[c].PWM_out);
			
			if (NameCH[name].dev->ch[c].On_off) {
				cJSON_AddTrueToObject(temp_obj, "enabled");
			} else {
				cJSON_AddFalseToObject(temp_obj, "enabled");
			}
			
			cJSON_AddItemToArray(obj_ch, temp_obj);
			sent_count++;
		} else {
			break; // Первый пустой канал - конец активных каналов
		}
	}
	
	// Добавляем информацию о пагинации
	cJSON_AddNumberToObject(j_response, "offset", offset);
	cJSON_AddNumberToObject(j_response, "limit", limit);
	cJSON_AddNumberToObject(j_response, "count", sent_count);
	cJSON_AddBoolToObject(j_response, "has_more", (offset + sent_count) < MAX_CH_NAME && NameCH[offset + sent_count].dev != NULL);
	
	cJSON_AddItemToObject(j_response, "obj_ch", obj_ch);
	
	char *str_to_host = cJSON_Print(j_response);
	
	// Проверяем размер и отправляем
	size_t total_len = strlen(str_to_host);
	const size_t chunk_size = MAX_MESSAGE_SIZE - 3;
	
	if (total_len <= chunk_size)
	{
		STM_LOG_xx("%s", str_to_host);
		STM_LOG_xx("\x03\x04"); // ETX + EOT
	}
	else
	{
		// Отправляем частями если все же превышает буфер
		size_t offset_send = 0;
		while (offset_send < total_len)
		{
			size_t current_chunk_size = (total_len - offset_send > chunk_size) ? chunk_size : (total_len - offset_send);
			char saved_char = str_to_host[offset_send + current_chunk_size];
			str_to_host[offset_send + current_chunk_size] = '\0';
			STM_LOG_xx("%s", str_to_host + offset_send);
			str_to_host[offset_send + current_chunk_size] = saved_char;
			offset_send += current_chunk_size;
		}
		STM_LOG_xx("\x03\x04");
	}
	
	cJSON_free(str_to_host);
	cJSON_Delete(j_response);
}

// Получение данных моста с хоста
void action_bridge(cJSON *obj, bool save)
{
	cJSON *j_mode = cJSON_GetObjectItemCaseSensitive(obj, "mode");
	cJSON *j_setMode = cJSON_GetObjectItemCaseSensitive(obj, "setMode");
	cJSON *j_port = cJSON_GetObjectItemCaseSensitive(obj, "port");
	cJSON *j_setPort = cJSON_GetObjectItemCaseSensitive(obj, "setPort");

	bool settingsChanged = false;

	// Установка режима работы моста
	if ((j_setMode != NULL) && cJSON_IsTrue(j_setMode))
	{
		if (j_mode != NULL && cJSON_IsNumber(j_mode))
		{
			mode_bridge_t newMode = static_cast<mode_bridge_t>(j_mode->valueint);
			if (settings.bridge_sett.mode_rs485 != newMode)
			{
				settings.bridge_sett.mode_rs485 = newMode;
				settingsChanged = true;
			}
		}
		else
		{
			STM_LOG(LOG_ERR "Invalid or missing mode value");
		}
	}

	// Установка порта
	if ((j_setPort != NULL) && cJSON_IsTrue(j_setPort))
	{
		if (j_port != NULL && cJSON_IsNumber(j_port))
		{
			uint16_t newPort = static_cast<uint16_t>(j_port->valueint);
			if (settings.bridge_sett.port != newPort)
			{
				settings.bridge_sett.port = newPort;
				settingsChanged = true;
			}
		}
		else
		{
			STM_LOG(LOG_ERR "Invalid or missing port value");
		}
	}

	// Если были изменения и требуется сохранение
	if (settingsChanged && save)
	{
		if (mem_spi.Write(settings))
		{
			STM_LOG("Save settings OK. size %d", sizeof(settings_t));
		}
		else
		{
			STM_LOG("Save settings FAIL");
		}
	}
	else if (!settingsChanged)
	{
		STM_LOG("No bridge settings were changed");
	}
}

// Подготовка и отправка данных моста на хост
void action_bridge_data(cJSON *obj)
{
	cJSON *j_all_settings_obj = cJSON_CreateObject();
	cJSON *obj_bridge = cJSON_CreateObject();

	// Основная информация
	cJSON_AddNumberToObject(j_all_settings_obj, "id", ID_CTRL);
	cJSON_AddStringToObject(j_all_settings_obj, "name_device", NAME);
	cJSON_AddNumberToObject(j_all_settings_obj, "type_data", 6); // �?спользуем 6 для получения настроек моста

	// Настройки моста
	cJSON_AddNumberToObject(obj_bridge, "mode", settings.bridge_sett.mode_rs485);
	cJSON_AddNumberToObject(obj_bridge, "port", settings.bridge_sett.port);
	cJSON_AddNumberToObject(obj_bridge, "bod", settings.bridge_sett.RS485->Init.BaudRate);

	// Если нужно добавить резервные поля
	// cJSON_AddNumberToObject(obj_bridge, "reserv1", settings.bridge_sett.reserv1);
	// cJSON_AddNumberToObject(obj_bridge, "reserv2", settings.bridge_sett.reserv2);
	// cJSON_AddNumberToObject(obj_bridge, "reserv3", settings.bridge_sett.reserv3);
	// cJSON_AddNumberToObject(obj_bridge, "reserv4", settings.bridge_sett.reserv4);
	// cJSON_AddNumberToObject(obj_bridge, "reserv5", settings.bridge_sett.reserv5);

	// Добавляем настройки моста в основной объект
	cJSON_AddItemToObject(j_all_settings_obj, "obj", obj_bridge);

	// Преобразуем в строку и отправляем
	char *str_to_host = cJSON_Print(j_all_settings_obj);

	if (str_to_host)
	{
		STM_LOG_xx("%s", str_to_host);
		STM_LOG_xx("\x03\x04"); // ETX + EOT
		cJSON_free(str_to_host);
	}

	cJSON_Delete(j_all_settings_obj);
}

/* USER CODE END Application */
