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

uint16_t transaction_id;

uint32_t freqSens = HAL_RCC_GetHCLKFreq()/30000u;
uint32_t pwmSens;

uint16_t adc_buffer[24] = {0};
uint16_t adc_buffer2[24] = {0};

uint8_t UART_tx[UART_TX_LENGTH]={0}; // buff
uint8_t UART_rx[UART_RX_LENGTH]={0}; // buff

extern led LED_IPadr;
extern led LED_error;
extern led LED_OSstart;

void action_ip(cJSON *obj, bool save);
void action_ch_set(cJSON *obj);
void action_cmd(cJSON *obj);
void action_settings_data(cJSON *obj);
void action_bridge(cJSON *obj, bool save);
void action_bridge_data(cJSON *obj);
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

//TCP for ModBUS
uint8_t         rtu_data[256] = {0};
uint8_t 		response[260] = {0};
uint8_t 		Modbut_to_TCP[260] = {0};
uint16_t		SizeInModBus = 0;
struct netconn 	connectionForModBUS , newconnectionForModBUS;
struct netconn 	*connMB = &connectionForModBUS, *newconnMB = &newconnectionForModBUS; //contains info about connection inc. type, port, buf pointers etc.

//переменные переферии
uint32_t Start = 0;
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern flash mem_spi;


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

  	/* Инициализируем модуль обновления прошивки */
  	FirmwareUpdate_Init(&mem_spi);

  	/* Инициализируем TCP-сервер обновления прошивки */
  	FirmwareUpdateServer_Init();

  	/* Запускаем TCP-сервер (он будет работать всегда) */
  	FirmwareUpdateServer_Start();

  	STM_LOG("Firmware updater ready");

	HAL_StatusTypeDef status1;
	//uint8_t channelForName = 0;
	uint16_t Address = 0;
    uint8_t *rx_data = NULL;
    uint16_t rx_data_size = 0;

    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, UART_rx, UART_RX_LENGTH);
    __HAL_DMA_DISABLE_IT(&hdma_usart1_rx, DMA_IT_HT);

    uint32_t pcs_dev = auto_search_dev(devices, MAX_ADR_DEV);

    // выполнить связывание
	for (int var = 0; var <= pcs_dev; ++var)
	{
		// check device address
		if ((devices[var].Addr >= START_ADR_I2C) && (devices[var].Addr <= (START_ADR_I2C + MAX_ADR_DEV)))
		{
			for (int i = 0; i < 3; ++i)
			{
				NameCH[devices[var].ch[i].Name_ch].dev = &devices[var];
				NameCH[devices[var].ch[i].Name_ch].Channel_number = i;
			}
		}
	}

	/* Infinite loop */
	for(;;)
	{

        for (int var = 0; var < pcs_dev; ++var) {

			if(devices[var].Addr == 0)
			{
				continue;
			}

			// добавить мютекс для зашиты devices

			//отправить данные devices[var]
			if(send_pwm_ch_to_dev(&devices[var]) != HAL_OK)
			{
				continue;
			}

			// ожидаем ответ
			osEvent evt = osMessageGet(rxDataUART1Handle, 100); // ждем ответ

			if (evt.status == osEventMessage){
				uint32_t size = evt.value.v;

				uint8_t *rx_data = nullptr;
				rx_data_size = 0;
				Header_t header;

				uart_parse_packet(message_rx, size, &header, &rx_data, &rx_data_size); // парсим пакет

				if (header.cmd != cmd_t::data)
				{
					continue;
				}

				if(rx_data != nullptr){
					// разборка данных
					deserialize_buff_to_dev(rx_data, &devices[var]);
				}else{
					continue;
				}



			}else if (evt.status == osEventTimeout) {
				// В случае тайм-аута, просто выводим информационное сообщение и продолжаем
			}

            osDelay(10);
        }
        osDelay(10);
		
		//osDelay(10);
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
	HAL_StatusTypeDef status1;

	status1 = HAL_UARTEx_ReceiveToIdle_IT(settings.bridge_sett.RS485, response, 256);// Read data
    //uint16_t usCRC16;
	/* Infinite loop */
	for(;;)
	{

		// ждем event от USART
		osSemaphoreWait(Resive_USARTHandle,osWaitForever);
		// если соеденение все еще установленно
		if(newconnMB->type == netconn_type::NETCONN_TCP){

            // Засекаем время получения RTU ответа
            mb_timing.rtu_rx_start = HAL_GetTick();
            mb_timing.rtu_response_time = get_elapsed_ms(mb_timing.rtu_tx_start);

			switch (settings.bridge_sett.mode_rs485) {
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

					if (tcp_length > 0) {
						//print_packet(Modbut_to_TCP, tcp_length, "Converted back to TCP");
					} else if (tcp_length == -2) {
						STM_LOG(LOG_ERR "RTU to TCP failed - CRC\n");
					} else {
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
		}else{
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
void mbethTask(void const * argument)
{
  /* USER CODE BEGIN mbethTask */
	while(gnetif.ip_addr.addr == 0){osDelay(1);}	//ждем получение адреса

	//TCP connection vars
	err_t                err, accept_err;
	struct netbuf        buffer;
	struct netbuf 	*buf = &buffer; //bufferized input data
	void 		*in_data = NULL;
	uint16_t 		data_size = 0;
	//uint8_t			mb_tcp_head[6];
	//uint16_t usCRC16;
	//osSemaphoreWait(ModBusEndHandle,1000);
	//int32_t SemRet = 0;
	//sizeH = xPortGetMinimumEverFreeHeapSize();
	/* Infinite loop */
	for(;;)
	{
		connMB = netconn_new(NETCONN_TCP);
		if (connMB!=NULL)
		{
			err = netconn_bind(connMB,NULL,502);//assign port number to connection
			if (err==ERR_OK)
			{
				netconn_listen(connMB);//set port to listening mode
				while(1)
				{
					accept_err=netconn_accept(connMB,&newconnMB);//suspend until new connection
					if (accept_err==ERR_OK)
					{
						while (netconn_recv(newconnMB,&buf)==ERR_OK)//suspend until data received
						{
							do
							{
								netbuf_data(buf,&in_data,&data_size);//get pointer and data size of the buffer

	                            // Засекаем время начала обработки TCP
	                            mb_timing.tcp_to_rtu_start = HAL_GetTick();

								// вырезать данные для модбаса
								//memcpy(mb_tcp_head, in_data, 6); // копируем заголовок
								//memcpy((void*)input_tcp_data, ((uint8_t*)in_data)+6, data_size-6);

								//uint16_t len = mb_tcp_head[4] << 8;
								//len |= mb_tcp_head[5];

								// проверить пакет на длинну
								//if( len >= 254 ){
									////netconn_write(newconnMB,response,4,NETCONN_COPY);
									//return;
								//}

								// расчитать crc
								//usCRC16 = usMBCRC16(input_tcp_data,data_size-6);
								//input_tcp_data[data_size-6] = ( uint8_t )( usCRC16 & 0xFF );
								//input_tcp_data[data_size-5] = ( uint8_t )( usCRC16 >> 8 );

							    switch (settings.bridge_sett.mode_rs485) {
									case RTU:
										{
											//print_packet((uint8_t*)in_data, sizeof(in_data), "Original TCP packet");

											int rtu_length = tcp_to_rtu((uint8_t*)in_data, data_size, rtu_data, &transaction_id);
											if (rtu_length > 0) {
												//print_packet(rtu_data, rtu_length, "Converted RTU packet");


												// Проверяем CRC полученного RTU пакета
												if (verify_rtu_crc(rtu_data, rtu_length)) {
													//STM_LOG("RTU CRC check: OK\n");
												} else {
													STM_LOG("RTU CRC check: FAILED\n");
												}

		                                        // Засекаем время перед отправкой RTU
		                                        mb_timing.rtu_tx_start = HAL_GetTick();
		                                        mb_timing.tcp_to_rtu_time = get_elapsed_ms(mb_timing.tcp_to_rtu_start);

												// отправить
												HAL_GPIO_WritePin(DE_M_GPIO_Port, DE_M_Pin, GPIO_PIN_SET); //включить на передачу
												//HAL_UART_Transmit(bridge_sett.RS485, input_tcp_data, data_size, 100); //Отправляем данные в USART
												HAL_UART_Transmit(settings.bridge_sett.RS485, rtu_data, rtu_length, 100); //Отправляем данные в USART
												HAL_GPIO_WritePin(DE_M_GPIO_Port, DE_M_Pin, GPIO_PIN_RESET); //включить на прием

											} else {
												printf("Error: TCP to RTU failed\n");
											}

											break;
										}
									case STREAMER:
										{
											// отправить
											HAL_GPIO_WritePin(DE_M_GPIO_Port, DE_M_Pin, GPIO_PIN_SET); //включить на передачу
											//HAL_UART_Transmit(bridge_sett.RS485, input_tcp_data, data_size, 100); //Отправляем данные в USART
											HAL_UART_Transmit(settings.bridge_sett.RS485, (uint8_t*)in_data, data_size, 100); //Отправляем данные в USART
											HAL_GPIO_WritePin(DE_M_GPIO_Port, DE_M_Pin, GPIO_PIN_RESET); //включить на прием

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
					} else netconn_delete(newconnMB);
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
void uart_Task(void const * argument)
{
  /* USER CODE BEGIN uart_Task */
	 /* USER CODE BEGIN uart_Task */
		//HAL_UART_Receive_DMA(bridge_sett.RS485, UART2_rx, UART2_RX_LENGTH);
		HAL_UARTEx_ReceiveToIdle_DMA(&huart6, UART_debug_rx, UART6_RX_LENGTH);
		__HAL_DMA_DISABLE_IT(&hdma_usart6_rx, DMA_IT_HT);
		//__HAL_DMA_DISABLE_IT(&hdma_usart2_rx, DMA_IT_TC);
		/* Infinite loop */
		for (;;) {
			// ожидать собщение
			osMessageGet(rxDataUART2Handle, osWaitForever);
			//uint32_t message_len = strlen((char*) message_rx);
			//HAL_UART_Transmit(bridge_sett.RS485, message_rx, message_len, HAL_MAX_DELAY);

			// парсим  json
			cJSON *json = cJSON_Parse((char*) message_rx);
			if (json != NULL) {
				cJSON *id = cJSON_GetObjectItemCaseSensitive(json, "id");
				//cJSON *name_device = cJSON_GetObjectItemCaseSensitive(json, "name_device");
				cJSON *type_data = cJSON_GetObjectItemCaseSensitive(json, "type_data");
				cJSON *save_settings = cJSON_GetObjectItemCaseSensitive(json, "save_settings");
				cJSON *obj = cJSON_GetObjectItemCaseSensitive(json, "obj");

			if (cJSON_IsNumber(id) && cJSON_GetNumberValue(id) == ID_CTRL) {
				bool save_set = false;
				if (cJSON_IsTrue(save_settings)) {
					save_set = true;
				} else {
					save_set = false;
				}

				if (cJSON_IsNumber(type_data)) {
					switch (type_data->valueint) {
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
						//STM_LOG("Empty type_data num");
						break;
					}
					case 5:
					{
						action_bridge(obj, save_set);
						//STM_LOG("Empty type_data num");
						break;
					}
					case 6:
					{
						action_bridge_data(obj);
						//STM_LOG("Empty type_data num");
						break;
					}
					default:
					{
						STM_LOG("data type not registered");
						break;
					}
					}
				} else {
					STM_LOG("Invalid type data");
				}
			} else {
				STM_LOG("id not valid");
			}

			cJSON_Delete(json);
		} else {
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

// работа с каналами
void action_ch_set(cJSON *obj)
{
	//STM_LOG("Auto set channels start");
	if(settings.devices_depth != 0)
	{
		cJSON *j_out_obj = cJSON_CreateObject();
		cJSON *j_arr_obj = cJSON_CreateArray();

		for (int var = 0; var < settings.devices_depth; ++var) {
			cJSON *temp_obj = cJSON_CreateObject();
			uint8_t c = NameCH[var].Channel_number;
			cJSON_AddNumberToObject(temp_obj, "num", NameCH[var].dev->ch[c].Name_ch);
			cJSON_AddNumberToObject(temp_obj, "dev_addr", NameCH[var].dev->Addr);
			cJSON_AddNumberToObject(temp_obj, "ch_dev", c);

			cJSON_AddItemToArray(j_arr_obj, temp_obj);
		}

		cJSON_AddNumberToObject(j_out_obj, "id", ID_CTRL);
		cJSON_AddStringToObject(j_out_obj, "name_device", NAME);
		cJSON_AddNumberToObject(j_out_obj, "type_data", 2);
		cJSON_AddItemToObject(j_out_obj, "obj", j_arr_obj);

		char *out_str = cJSON_Print(j_out_obj);
		STM_LOG("%s", out_str);

		free(out_str);
		//cJSON_Delete(j_arr_obj);
		cJSON_Delete(j_out_obj);
		//STM_LOG("Auto set channels OK");
	}
	else
	{
		STM_LOG("empty channels");
	}

}

// Обработка команд
void action_cmd(cJSON *obj)
{
	cJSON *id_cmd = cJSON_GetObjectItemCaseSensitive(obj, "id_cmd");

	int key = cJSON_GetNumberValue(id_cmd);

	switch (key) {
	case 1:{ //auto set
		cJSON *Addr_start = cJSON_GetObjectItemCaseSensitive(obj, "Addr_start");
		cJSON *Count_dev = cJSON_GetObjectItemCaseSensitive(obj, "Count_dev");

		int addres = cJSON_GetNumberValue(Addr_start);
		int count = cJSON_GetNumberValue(Count_dev);

		// автопоиск
		uint32_t pcs_dev = auto_search_dev(devices, MAX_ADR_DEV);
		//setRange_i2c_dev(addres, count);

		if(mem_spi.Write(settings))
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
	case 2:{ // add device
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
	case 3:{ // del device
		//cJSON *Num = cJSON_GetObjectItemCaseSensitive(obj, "Num");
		//int num = cJSON_GetNumberValue(Num);

		//del_Name_dev(num);
		STM_LOG("err empty cmd");
		break;
	}
	case 4:{ // on_off chanel
		cJSON *Num = cJSON_GetObjectItemCaseSensitive(obj, "Num");
		cJSON *PWM = cJSON_GetObjectItemCaseSensitive(obj, "PWM");
		cJSON *On = cJSON_GetObjectItemCaseSensitive(obj, "On");
		cJSON *All = cJSON_GetObjectItemCaseSensitive(obj, "All");

		int num = cJSON_GetNumberValue(Num);
		int pwm = cJSON_GetNumberValue(PWM);
		bool on = cJSON_IsTrue(On);
		bool all = cJSON_IsTrue(All);

		if (all) {
			for (int name = 0; name < MAX_CH_NAME; ++name) {
				if(NameCH[name].dev != NULL){
					uint8_t c = NameCH[name].Channel_number; // get channel number for this name
					NameCH[name].dev->ch[c].PWM_out = pwm;
					NameCH[name].dev->ch[c].On_off = on;
				}
			}
			STM_LOG("OK");
		} else {
			// mode set one channel
			if(NameCH[num].dev != NULL){
				uint8_t c = NameCH[num].Channel_number; // get channel number for this name
				NameCH[num].dev->ch[c].PWM_out = pwm;
				NameCH[num].dev->ch[c].On_off = on;
				STM_LOG("OK");
			}else{
				STM_LOG("NULL ptr dev");
			}
		}
		break;
	}
	case 5:{ // reboot
		STM_LOG("Rebooting...");
		osDelay(3000);
		NVIC_SystemReset();
		break;
	}
	default:{
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

	/* убрал отправку данных о каналах так как занимает много мести. восможно стоит вернуть в бинарном формате
	// настройки каналов
	for (int name = 0; name < MAX_CH_NAME; ++name) {

		if (NameCH[name].dev != NULL) {
			cJSON *temp_obj = cJSON_CreateObject();

			uint8_t c = NameCH[name].Channel_number; // get channel number for this name

			cJSON_AddNumberToObject(temp_obj, "num", NameCH[name].dev->ch[c].Name_ch);
			cJSON_AddNumberToObject(temp_obj, "dev_addr", NameCH[name].dev->Addr);
			cJSON_AddNumberToObject(temp_obj, "ch_dev", NameCH[name].Channel_number);
			cJSON_AddNumberToObject(temp_obj, "PWM", NameCH[name].dev->ch[c].PWM_out);

			cJSON_AddItemToArray(obj_ch, temp_obj);

			//cJSON_Delete(temp_obj);
		} else {
			//STM_LOG("Error id_cmd");
			break;
		}
	}

	// отрпавка на хост
	cJSON_AddItemToObject(j_all_settings_obj, "obj_ch", obj_ch);
	*/

	cJSON_AddItemToObject(j_all_settings_obj, "obj_ip", obj_ip);

	char *str_to_host = cJSON_Print(j_all_settings_obj);

	//STM_LOG("%s", str_to_host);

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
	//cJSON_Delete(obj_ch);
	//cJSON_Delete(obj_ip);
	cJSON_Delete(j_all_settings_obj);

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
    if ((j_setMode != NULL) && cJSON_IsTrue(j_setMode)) {
        if (j_mode != NULL && cJSON_IsNumber(j_mode)) {
            mode_bridge_t newMode = static_cast<mode_bridge_t>(j_mode->valueint);
            if (settings.bridge_sett.mode_rs485 != newMode) {
                settings.bridge_sett.mode_rs485 = newMode;
                settingsChanged = true;
            }
        } else {
            STM_LOG(LOG_ERR "Invalid or missing mode value");
        }
    }

    // Установка порта
    if ((j_setPort != NULL) && cJSON_IsTrue(j_setPort)) {
        if (j_port != NULL && cJSON_IsNumber(j_port)) {
            uint16_t newPort = static_cast<uint16_t>(j_port->valueint);
            if (settings.bridge_sett.port != newPort) {
                settings.bridge_sett.port = newPort;
                settingsChanged = true;
            }
        } else {
            STM_LOG(LOG_ERR "Invalid or missing port value");
        }
    }

    // Если были изменения и требуется сохранение
    if (settingsChanged && save) {
		if(mem_spi.Write(settings))
		{
			STM_LOG("Save settings OK. size %d", sizeof(settings_t));
		}
		else
		{
			STM_LOG("Save settings FAIL");
		}
    } else if (!settingsChanged) {
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
    cJSON_AddNumberToObject(j_all_settings_obj, "type_data", 6);  // �?спользуем 6 для получения настроек моста

    // Настройки моста
    cJSON_AddNumberToObject(obj_bridge, "mode", settings.bridge_sett.mode_rs485);
    cJSON_AddNumberToObject(obj_bridge, "port", settings.bridge_sett.port);
    cJSON_AddNumberToObject(obj_bridge, "bod", settings.bridge_sett.RS485->Init.BaudRate);

    // Если нужно добавить резервные поля
    //cJSON_AddNumberToObject(obj_bridge, "reserv1", settings.bridge_sett.reserv1);
    //cJSON_AddNumberToObject(obj_bridge, "reserv2", settings.bridge_sett.reserv2);
    //cJSON_AddNumberToObject(obj_bridge, "reserv3", settings.bridge_sett.reserv3);
    //cJSON_AddNumberToObject(obj_bridge, "reserv4", settings.bridge_sett.reserv4);
    //cJSON_AddNumberToObject(obj_bridge, "reserv5", settings.bridge_sett.reserv5);

    // Добавляем настройки моста в основной объект
    cJSON_AddItemToObject(j_all_settings_obj, "obj", obj_bridge);

    // Преобразуем в строку и отправляем
    char *str_to_host = cJSON_Print(j_all_settings_obj);

    if (str_to_host) {
    	STM_LOG_xx("%s", str_to_host);
        STM_LOG_xx("\x03\x04"); // ETX + EOT
        cJSON_free(str_to_host);
    }

    cJSON_Delete(j_all_settings_obj);
}


/* USER CODE END Application */
