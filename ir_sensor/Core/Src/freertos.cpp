/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * File Name          : freertos.c
 * Description        : Code for freertos applications
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
#include "sensor.h"
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
extern ADC_HandleTypeDef hadc1;
extern DMA_HandleTypeDef hdma_adc1;

extern settings_t settings;
extern sensor Sensor;
extern flash mem_spi;

uint32_t freqSens = HAL_RCC_GetHCLKFreq() / 30000u;
uint32_t pwmSens;
extern led_t LED_IPadr;
extern led_t LED_error;
extern led_t LED_OSstart;

// переменные для отладки
struct debugSensor
{
    uint32_t time;
    uint16_t dada[16];
    bool detect = false;
};

vector<debugSensor> debugBuf;
uint32_t debug_I = 0;
bool debug_send = false;
uint32_t g_Result_S1, g_Result_S2;
uint32_t g_Detect_S1, g_Detect_S2;
uint16_t call1 = 0;

// структуры для netcon
extern struct netif gnetif;

// Буферы для АЦП
uint16_t raw_adc_buffer[ADC_BUF_SIZE] = {0};  // буфер для сырых данных АЦП
uint16_t temp_adc_buffer[ADC_BUF_SIZE] = {0}; // буфер для временного хранения сырых данных АЦП

void action_ip(cJSON *obj, bool save);
// void action_ch_set(cJSON *obj);
void action_cmd(cJSON *obj);
void action_settings_data(cJSON *obj);
// void action_bridge(cJSON *obj, bool save);
// void action_bridge_data(cJSON *obj);

// TCP_IP
string strIP;
string in_str;

// обмен данными с компом
extern uint8_t message_rx[message_RX_LENGTH];
extern uint8_t UART_debug_rx[UART6_RX_LENGTH];
extern uint16_t indx_message_rx;
extern uint16_t indx_UART6_rx;
extern uint16_t Size_message;
extern uint16_t Start_index;

// семафор окончания обработки данных с АЦП
SemaphoreHandle_t adcDataReadySemaphore;

/* USER CODE END Variables */
osThreadId mainTaskHandle;
osThreadId ethTaskHandle;
osThreadId ledTaskHandle;
osThreadId debug_udpHandle;
osThreadId uart_taskHandle;
osThreadId loggerTaskHandle;
osMessageQId rxDataUART2Handle;
osMutexId s2DistanceMutexHandle;
osMutexId mutexADCHandle;
osMutexId s1DistanceMutexHandle;
osMutexId setMutexHandle;
osSemaphoreId ADC_endHandle;

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
// extern "C"
/* USER CODE END FunctionPrototypes */

void MainTask(void const *argument);
void eth_Task(void const *argument);
void LedTask(void const *argument);
void Debug_udp(void const *argument);
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
    /* Create the mutex(es) */
    /* definition and creation of s2DistanceMutex */
    osMutexDef(s2DistanceMutex);
    s2DistanceMutexHandle = osMutexCreate(osMutex(s2DistanceMutex));

    /* definition and creation of mutexADC */
    osMutexDef(mutexADC);
    mutexADCHandle = osMutexCreate(osMutex(mutexADC));

    /* definition and creation of s1DistanceMutex */
    osMutexDef(s1DistanceMutex);
    s1DistanceMutexHandle = osMutexCreate(osMutex(s1DistanceMutex));

    /* definition and creation of setMutex */
    osMutexDef(setMutex);
    setMutexHandle = osMutexCreate(osMutex(setMutex));

    /* USER CODE BEGIN RTOS_MUTEX */
    /* add mutexes, ... */
    /* USER CODE END RTOS_MUTEX */

    /* Create the semaphores(s) */
    /* definition and creation of ADC_end */
    osSemaphoreDef(ADC_end);
    ADC_endHandle = osSemaphoreCreate(osSemaphore(ADC_end), 1);

    /* USER CODE BEGIN RTOS_SEMAPHORES */

    adcDataReadySemaphore = xSemaphoreCreateBinary();
    /* add semaphores, ... */
    /* USER CODE END RTOS_SEMAPHORES */

    /* USER CODE BEGIN RTOS_TIMERS */
    /* start timers, add new ones, ... */
    /* USER CODE END RTOS_TIMERS */

    /* Create the queue(s) */
    /* definition and creation of rxDataUART2 */
    osMessageQDef(rxDataUART2, 16, uint8_t);
    rxDataUART2Handle = osMessageCreate(osMessageQ(rxDataUART2), NULL);

    /* USER CODE BEGIN RTOS_QUEUES */
    /* add queues, ... */
    /* USER CODE END RTOS_QUEUES */

    /* Create the thread(s) */
    /* definition and creation of mainTask */
    osThreadDef(mainTask, MainTask, osPriorityNormal, 0, 512);
    mainTaskHandle = osThreadCreate(osThread(mainTask), NULL);

    /* definition and creation of ethTask */
    osThreadDef(ethTask, eth_Task, osPriorityNormal, 0, 512);
    ethTaskHandle = osThreadCreate(osThread(ethTask), NULL);

    /* definition and creation of ledTask */
    osThreadDef(ledTask, LedTask, osPriorityNormal, 0, 512);
    ledTaskHandle = osThreadCreate(osThread(ledTask), NULL);

    /* definition and creation of debug_udp */
    osThreadDef(debug_udp, Debug_udp, osPriorityNormal, 0, 512);
    debug_udpHandle = osThreadCreate(osThread(debug_udp), NULL);

    /* definition and creation of uart_task */
    osThreadDef(uart_task, uart_Task, osPriorityNormal, 0, 512);
    uart_taskHandle = osThreadCreate(osThread(uart_task), NULL);

    /* definition and creation of loggerTask */
    osThreadDef(loggerTask, LoggerTask, osPriorityNormal, 0, 128);
    loggerTaskHandle = osThreadCreate(osThread(loggerTask), NULL);

    /* USER CODE BEGIN RTOS_THREADS */
    /* add threads, ... */
    /* USER CODE END RTOS_THREADS */
}

/* USER CODE BEGIN Header_MainTask */
// тут обрабатываем и фильтруем данные с АЦП
/**
 * @brief  Function implementing the mainTask thread.
 * @param  argument: Not used
 * @retval None
 */
/* USER CODE END Header_MainTask */
void MainTask(void const *argument)
{
    /* init code for LWIP */
    MX_LWIP_Init();
    /* USER CODE BEGIN MainTask */
    // нициализируем логгер
    Logger_Init(&huart6);

    STM_LOG("Start %s app. Ver %d", FIRMWARE_NAME, FIRMWARE_VERSION);

    /* Инициализируем модуль обновления прошивки */
    FirmwareUpdate_Init(&mem_spi);

    /* Инициализируем TCP-сервер обновления прошивки */
    FirmwareUpdateServer_Init();

    /* Запускаем TCP-сервер (он будет работать всегда) */
    FirmwareUpdateServer_Start();

    STM_LOG("Firmware updater ready");

    float temp_distance_ul = 0.0;
    bool temp_det = 0;

    Sensor.Init(&settings, &ADC_endHandle, &hadc1, raw_adc_buffer, ADC_SAMPLES_PER_CYCLE);

    // переменные для сбора данных
    static uint32_t prevTime = HAL_GetTick();
    static uint32_t GTime = 0;
    debugSensor tempUnit;

    /* Infinite loop */
    for (;;)
    {

        if (Sensor.GetCalibrationStatus())
        {
            LED_error.LEDon();
            Sensor.Calibration();
            LED_error.LEDoff();
        }
        else
        {
            // взять мютекс
            osMutexWait(mutexADCHandle, osWaitForever);
            // запустить ацп
            HAL_ADC_Start_DMA(&hadc1, (uint32_t *)&raw_adc_buffer, Sensor.Depth);
            // подождать симафор от АЦП
            osSemaphoreWait(ADC_endHandle, osWaitForever);
            // вернуть мютекс
            osMutexRelease(mutexADCHandle);
            // обработать данные
            g_Result_S1 = Sensor.DataProcessing(raw_adc_buffer);
            // start debug
            if (debug_I <= 100)
            {
                debug_I++;
                tempUnit.time = GTime += ((HAL_GetTick()) - prevTime);
                prevTime = HAL_GetTick();
                tempUnit.dada[0] = Sensor.GetResult();
                tempUnit.detect = Sensor.Getdetect();
                debugBuf.push_back(tempUnit);
            }
            else
            {
                if (debug_send)
                {
                    debug_send = false;
                    debugBuf.clear();
                    debug_I = 0;
                }
            }
            // end debug
            temp_det = Sensor.DetectPoll();
            if (temp_det)
            {
                g_Detect_S1 = 1000;
            }
            else
            {
                g_Detect_S1 = 0;
            }
            if (temp_det)
            {
                LED_error.LEDon();
            }
            else
            {
                LED_error.LEDoff();
            }
        }

        // taskYIELD();
    }
    /* USER CODE END MainTask */
}

/* USER CODE BEGIN Header_eth_Task */
/**
 * @brief Function implementing the ethTask thread.
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
    strIP = ip4addr_ntoa(&gnetif.ip_addr);

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
                                in_str.assign((char *)in_data, data_size); // copy in string
                                /*-----------------------------------------------------------------------------------------------------------------------------*/
                                STM_LOG("Get CMD %s", in_str.c_str());

                                if (!in_str.empty())
                                {
                                    string resp = Command_execution(in_str);
                                    netconn_write(newconn, resp.c_str(), resp.size(), NETCONN_COPY);
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

/* USER CODE BEGIN Header_LedTask */
/**
 * @brief Function implementing the ledTask thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_LedTask */
void LedTask(void const *argument)
{
    /* USER CODE BEGIN LedTask */
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
    /* USER CODE END LedTask */
}

/* USER CODE BEGIN Header_Debug_udp */
/**
 * @brief Function implementing the debug_udp thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_Debug_udp */
void Debug_udp(void const *argument)
{
    /* USER CODE BEGIN Debug_udp */
    while (gnetif.ip_addr.addr == 0)
    {
        osDelay(1);
    } // ждем получение адреса

    strIP = ip4addr_ntoa(&gnetif.ip_addr);

    // структуры для netcon
    struct netconn *conn;
    struct netconn *newconn;
    struct netbuf *netbuf;
    volatile err_t err, accept_err;
    void *in_data = NULL;
    uint16_t data_size = 0;

    /* Infinite loop */
    for (;;)
    {
        conn = netconn_new(NETCONN_TCP);
        if (conn != NULL)
        {
            // ИСПРАВЛЕНИЕ: добавлена привязка к порту (например, 82 для отладки)
            err = netconn_bind(conn, NULL, 82);
            if (err == ERR_OK)
            {
                netconn_listen(conn); // set port to listening mode
                while (1)
                {
                    accept_err = netconn_accept(conn, &newconn); // suspend until new connection
                    if (accept_err == ERR_OK)
                    {
                        // LED_IPadr.LEDon();
                        while ((accept_err = netconn_recv(newconn, &netbuf)) == ERR_OK) // работаем до тех пор пока клиент не разорвет соеденение
                        {
                            do
                            {
                                netbuf_data(netbuf, &in_data, &data_size); // get pointer and data size of the buffer
                                in_str.assign((char *)in_data, data_size); // copy in string

                                // Формируем ответ
                                string resp;

                                // ИСПРАВЛЕНИЕ: проверка размера вектора перед доступом
                                size_t bufSize = debugBuf.size();
                                size_t maxItems = (bufSize < 100) ? bufSize : 100;

                                for (size_t i = 0; i < maxItems; ++i)
                                {
                                    resp.append(to_string(debugBuf[i].time) + ";");
                                    resp.append(to_string(debugBuf[i].detect) + ";");
                                    resp.append(to_string(debugBuf[i].dada[0]) + "\n");
                                }

                                // ИСПРАВЛЕНИЕ: использование NETCONN_COPY для безопасности
                                if (!resp.empty())
                                {
                                    netconn_write(newconn, resp.c_str(), resp.size(), NETCONN_COPY);
                                }
                                debug_send = true;

                            } while (netbuf_next(netbuf) >= 0);
                            netbuf_delete(netbuf);
                        }
                        netconn_close(newconn);
                        netconn_delete(newconn);
                        // LED_IPadr.LEDoff();
                    }
                    else
                    {
                        // ИСПРАВЛЕНИЕ: правильная обработка ошибки accept
                        if (newconn != NULL)
                        {
                            netconn_delete(newconn);
                        }
                    }
                    osDelay(20);
                }
            }
            else
            {
                // ИСПРАВЛЕНИЕ: освобождение ресурсов при ошибке bind
                netconn_delete(conn);
            }
        }

        osDelay(1);
    }
    /* USER CODE END Debug_udp */
}

/* USER CODE BEGIN Header_uart_Task */
// Задача для обработки UART данныйх от компа
/**
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
        osMessageGet(rxDataUART2Handle, osWaitForever);
        // uint32_t message_len = strlen((char*) message_rx);
        // HAL_UART_Transmit(bridge_sett.RS485, message_rx, message_len, HAL_MAX_DELAY);

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
                        // action_ch_set(obj);
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
                    case 5:
                    {
                        // action_bridge(obj, save_set);
                        // STM_LOG("Empty type_data num");
                        break;
                    }
                    case 6:
                    {
                        // action_bridge_data(obj);
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
        char sep = '.';
        std::string s = j_IP->valuestring;
        if (!s.empty())
        {
            std::string sepIP[4];
            int i = 0;
            bool parseSuccess = true;

            for (size_t p = 0, q = 0; (p != s.npos) && (i < 4); p = q, i++)
            {
                sepIP[i] = s.substr(p + (p != 0),
                                    (q = s.find(sep, p + 1)) - p - (p != 0));
            }

            // Проверка, что удалось разделить строку на 4 части
            if (i == 4)
            {
                uint8_t ipParts[4] = {0};
                for (i = 0; i < 4; i++)
                {
                    char *endptr = nullptr;
                    long value = strtol(sepIP[i].c_str(), &endptr, 10);

                    // Проверка успешности преобразования и диапазона значений
                    if (*endptr != '\0' || value < 0 || value > 255)
                    {
                        parseSuccess = false;
                        break;
                    }
                    ipParts[i] = (uint8_t)value;
                }

                if (parseSuccess)
                {
                    // Сохраняем новые значения только если парсинг успешен
                    for (i = 0; i < 4; i++)
                    {
                        settings.saveIP.ip[i] = ipParts[i];
                    }
                    settingsChanged = true;
                }
                else
                {
                    STM_LOG(LOG_ERR "Invalid IP address format");
                }
            }
            else
            {
                STM_LOG(LOG_ERR "IP address must have 4 octets");
            }
        }
    }

    // Обработка MAC
    if ((j_setMAC != NULL) && cJSON_IsTrue(j_setMAC) && (j_MAC != NULL) && cJSON_IsString(j_MAC))
    {
        char sep = ':';
        std::string s = j_MAC->valuestring;
        if (!s.empty())
        {
            std::string sepMAC[6];
            int i = 0;
            bool parseSuccess = true;

            for (size_t p = 0, q = 0; (p != s.npos) && (i < 6); p = q, i++)
            {
                sepMAC[i] = s.substr(p + (p != 0),
                                     (q = s.find(sep, p + 1)) - p - (p != 0));
            }

            // Проверка, что удалось разделить строку на 6 частей
            if (i == 6)
            {
                uint8_t macParts[6] = {0};
                for (i = 0; i < 6; i++)
                {
                    char *endptr = nullptr;
                    long value = strtol(sepMAC[i].c_str(), &endptr, 16);

                    // Проверка успешности преобразования и диапазона значений
                    if (*endptr != '\0' || value < 0 || value > 255)
                    {
                        parseSuccess = false;
                        break;
                    }
                    macParts[i] = (uint8_t)value;
                }

                if (parseSuccess)
                {
                    // Сохраняем новые значения только если парсинг успешен
                    for (i = 0; i < 6; i++)
                    {
                        settings.MAC[i] = macParts[i];
                    }
                    settingsChanged = true;
                }
                else
                {
                    STM_LOG(LOG_ERR "Invalid MAC address format");
                }
            }
            else
            {
                STM_LOG(LOG_ERR "MAC address must have 6 octets");
            }
        }
    }

    // Обработка GATEWAY
    if ((j_setGATEWAY != NULL) && cJSON_IsTrue(j_setGATEWAY) && (j_GATEWAY != NULL) && cJSON_IsString(j_GATEWAY))
    {
        char sep = '.';
        std::string s = j_GATEWAY->valuestring;
        if (!s.empty())
        {
            std::string sepGATEWAY[4];
            int i = 0;
            bool parseSuccess = true;

            for (size_t p = 0, q = 0; (p != s.npos) && (i < 4); p = q, i++)
            {
                sepGATEWAY[i] = s.substr(p + (p != 0),
                                         (q = s.find(sep, p + 1)) - p - (p != 0));
            }

            // Проверка, что удалось разделить строку на 4 части
            if (i == 4)
            {
                uint8_t gatewayParts[4] = {0};
                for (i = 0; i < 4; i++)
                {
                    char *endptr = nullptr;
                    long value = strtol(sepGATEWAY[i].c_str(), &endptr, 10);

                    // Проверка успешности преобразования и диапазона значений
                    if (*endptr != '\0' || value < 0 || value > 255)
                    {
                        parseSuccess = false;
                        break;
                    }
                    gatewayParts[i] = (uint8_t)value;
                }

                if (parseSuccess)
                {
                    // Сохраняем новые значения только если парсинг успешен
                    for (i = 0; i < 4; i++)
                    {
                        settings.saveIP.gateway[i] = gatewayParts[i];
                    }
                    settingsChanged = true;
                }
                else
                {
                    STM_LOG(LOG_ERR "Invalid gateway address format");
                }
            }
            else
            {
                STM_LOG(LOG_ERR "Gateway address must have 4 octets");
            }
        }
    }

    // Обработка MASK
    if ((j_setMASK != NULL) && cJSON_IsTrue(j_setMASK) && (j_MASK != NULL) && cJSON_IsString(j_MASK))
    {
        char sep = '.';
        std::string s = j_MASK->valuestring;
        if (!s.empty())
        {
            std::string sepMASK[4];
            int i = 0;
            bool parseSuccess = true;

            for (size_t p = 0, q = 0; (p != s.npos) && (i < 4); p = q, i++)
            {
                sepMASK[i] = s.substr(p + (p != 0),
                                      (q = s.find(sep, p + 1)) - p - (p != 0));
            }

            // Проверка, что удалось разделить строку на 4 части
            if (i == 4)
            {
                uint8_t maskParts[4] = {0};
                for (i = 0; i < 4; i++)
                {
                    char *endptr = nullptr;
                    long value = strtol(sepMASK[i].c_str(), &endptr, 10);

                    // Проверка успешности преобразования и диапазона значений
                    if (*endptr != '\0' || value < 0 || value > 255)
                    {
                        parseSuccess = false;
                        break;
                    }
                    maskParts[i] = (uint8_t)value;
                }

                if (parseSuccess)
                {
                    // Сохраняем новые значения только если парсинг успешен
                    for (i = 0; i < 4; i++)
                    {
                        settings.saveIP.mask[i] = maskParts[i];
                    }
                    settingsChanged = true;
                }
                else
                {
                    STM_LOG(LOG_ERR "Invalid subnet mask format");
                }
            }
            else
            {
                STM_LOG(LOG_ERR "Subnet mask must have 4 octets");
            }
        }
    }

    // Обработка DNS
    if ((j_setDNS != NULL) && cJSON_IsTrue(j_setDNS) && (j_DNS != NULL) && cJSON_IsString(j_DNS))
    {
        char sep = '.';
        std::string s = j_DNS->valuestring;
        if (!s.empty())
        {
            std::string sepDNS[4];
            int i = 0;
            bool parseSuccess = true;

            for (size_t p = 0, q = 0; (p != s.npos) && (i < 4); p = q, i++)
            {
                sepDNS[i] = s.substr(p + (p != 0),
                                     (q = s.find(sep, p + 1)) - p - (p != 0));
            }

            // Проверка, что удалось разделить строку на 4 части
            if (i == 4)
            {
                // Закомментировано, так как в оригинальном коде это не реализовано
                // Оставляем для совместимости с исходным кодом
                // uint8_t dnsParts[4] = {0};
                // for (i = 0; i < 4; i++) {
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
                // if (parseSuccess) {
                //    // Сохраняем новые значения только если парсинг успешен
                //    //settings.[0] = dnsParts[0];
                //    //settings.[1] = dnsParts[1];
                //    //settings.[2] = dnsParts[2];
                //    //settings.[3] = dnsParts[3];
                //    settingsChanged = true;
                //} else {
                //    STM_LOG(LOG_ERR "Invalid DNS address format");
                //}
            }
            else
            {
                STM_LOG(LOG_ERR "DNS address must have 4 octets");
            }
        }
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

// Обработка команд управления
void action_cmd(cJSON *obj)
{
    cJSON *id_cmd = cJSON_GetObjectItemCaseSensitive(obj, "id_cmd");

    int key = cJSON_GetNumberValue(id_cmd);

    switch (key)
    {
    case 1:
    { // auto set

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
    string srtIP_to_host = std::to_string(settings.saveIP.ip[0]) + "." +
                           std::to_string(settings.saveIP.ip[1]) + "." +
                           std::to_string(settings.saveIP.ip[2]) + "." +
                           std::to_string(settings.saveIP.ip[3]);
    cJSON_AddStringToObject(obj_ip, "IP", srtIP_to_host.c_str());

    char srtMAC_to_host[100];
    sprintf(srtMAC_to_host, "%x:%x:%x:%x:%x:%x", settings.MAC[0], settings.MAC[1], settings.MAC[2],
            settings.MAC[3], settings.MAC[4], settings.MAC[5]);
    cJSON_AddStringToObject(obj_ip, "MAC", srtMAC_to_host);

    string srtGATEWAY_to_host = std::to_string(settings.saveIP.gateway[0]) + "." +
                                std::to_string(settings.saveIP.gateway[1]) + "." +
                                std::to_string(settings.saveIP.gateway[2]) + "." +
                                std::to_string(settings.saveIP.gateway[3]);
    cJSON_AddStringToObject(obj_ip, "GATEWAY", srtGATEWAY_to_host.c_str());

    string srtMASK_to_host = std::to_string(settings.saveIP.mask[0]) + "." +
                             std::to_string(settings.saveIP.mask[1]) + "." +
                             std::to_string(settings.saveIP.mask[2]) + "." +
                             std::to_string(settings.saveIP.mask[3]);
    cJSON_AddStringToObject(obj_ip, "MASK", srtMASK_to_host.c_str());

    cJSON_AddStringToObject(obj_ip, "DNS", "0.0.0.0");

    if (settings.DHCPset)
    {
        cJSON_AddTrueToObject(obj_ip, "DHCP");
    }
    else
    {
        cJSON_AddFalseToObject(obj_ip, "DHCP");
    }

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

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1)
    {
        HAL_ADC_Stop_DMA(&hadc1);
        osSemaphoreRelease(ADC_endHandle);
    }
}
/* USER CODE END Application */
