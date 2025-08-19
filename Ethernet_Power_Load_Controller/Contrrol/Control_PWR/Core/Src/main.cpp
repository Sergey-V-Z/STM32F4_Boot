/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
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
#include "main.h"
#include "cmsis_os.h"
#include "dma.h"
#include "lwip.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
// #include "flash_spi.h"
#include "Delay_us_DWT.h"
#include "LED.h"
#include "flash_spi.h"

#include "cmsis_os.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
// Размещение метаданных по фиксированному адресу с помощью атрибута section
__attribute__((section(".metadata")))
const meta_t firmware_metadata = {
    .key_start = METADATA_KEY,
    .version = FIRMWARE_VERSION,
    .name_proj = FIRMWARE_NAME,
    .reserved = 0};
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
uint8_t ucHeap[configTOTAL_HEAP_SIZE] __attribute__((section(".ccmram"))) = {0};

chName_t NameCH[MAX_CH_NAME];
DEV_t devices[MAX_ADR_DEV];

uint32_t count_tic = 0; // для замеров времени выполнения кода

led LED_IPadr;
led LED_error;
led LED_OSstart;

flash mem_spi;
// for i2c
// g_stat_t I2C_net[45];

bool resetSettings = false;
timing_info_t mb_timing;

// for SPI Flash
pins_spi_t ChipSelect = {SPI3_CS_GPIO_Port, SPI3_CS_Pin};
pins_spi_t WriteProtect = {WP_GPIO_Port, WP_Pin};
pins_spi_t Hold = {HOLD_GPIO_Port, HOLD_Pin};

settings_t settings = {0, 0x0E};

// Глобальный экземпляр логгера
UartLogger_t logger;

// Локальный буфер для DMA передачи
char txBuffer[MAX_MESSAGE_SIZE];
// Пул сообщений и буфер для него
LogMessage_t messagePool[QUEUE_SIZE];
uint8_t messagePoolUsed[QUEUE_SIZE];
osMutexId poolMutexHandle;

uint8_t message_rx[message_RX_LENGTH];
uint8_t UART_debug_rx[UART6_RX_LENGTH];
uint16_t indx_message_rx = 0;
uint16_t indx_UART2_rx = 0;
uint16_t Size_message = 0;
uint16_t Start_index = 0;
extern osMessageQId rxDataUART2Handle;

// обмен данными с другими платами
bool rx_end = 1;

// TCP for ModBUS
extern uint8_t rtu_data[256];
extern uint8_t response[260];
extern uint8_t Modbut_to_TCP[260];
extern uint16_t SizeInModBus;
extern struct netconn connectionForModBUS, newconnectionForModBUS;
extern struct netconn *connMB, *newconnMB; // contains info about connection inc. type, port, buf pointers etc.

extern osSemaphoreId Resive_USARTHandle;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void MX_FREERTOS_Init(void);
/* USER CODE BEGIN PFP */
uint8_t ReadStraps();
void finishedBlink();
void timoutBlink();

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// uint8_t ucHeap[ configTOTAL_HEAP_SIZE] __attribute__((section(".ccmram")));

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void)
{

    /* USER CODE BEGIN 1 */
    /* Настройка вектора прерываний на адрес приложения */
    SCB->VTOR = FLASH_BASE | 0x10000; /* 0x08010000 */
    __enable_irq();
    /* USER CODE END 1 */

    /* MCU Configuration--------------------------------------------------------*/

    /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
    HAL_Init();

    /* USER CODE BEGIN Init */

    /* USER CODE END Init */

    /* Configure the system clock */
    SystemClock_Config();

    /* USER CODE BEGIN SysInit */

    /* USER CODE END SysInit */

    /* Initialize all configured peripherals */
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_SPI3_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();
    MX_USART6_UART_Init();
    /* USER CODE BEGIN 2 */

    mem_spi.Init(&hspi3, 0, ChipSelect, WriteProtect, Hold, false);
    // HAL_Delay(100);
    mem_spi.Read(&settings);

    HAL_GPIO_WritePin(R_GPIO_Port, R_Pin, GPIO_PIN_SET); // PC15 VD4
    HAL_GPIO_WritePin(B_GPIO_Port, B_Pin, GPIO_PIN_SET); // PC13 VD2
    HAL_GPIO_WritePin(G_GPIO_Port, G_Pin, GPIO_PIN_SET); // PC14 VD3

    uint8_t endMAC = 0, IP = 100;

    HAL_GPIO_WritePin(eth_NRST_GPIO_Port, eth_NRST_Pin, GPIO_PIN_SET);

    // работаем с настройками из флешки
    if ((settings.version == 0) | (settings.version == 0xFF) | resetSettings)
    {
        STM_LOG("Reset settings");

        settings.isON_from_settings = false;
        settings.IP_end_from_settings = 1;

        settings.DHCPset = true;
        settings.devices_depth = 0;

        settings.saveIP.ip[0] = 192;
        settings.saveIP.ip[1] = 168;
        settings.saveIP.ip[2] = 1;
        settings.saveIP.ip[3] = IP;

        settings.saveIP.mask[0] = 255;
        settings.saveIP.mask[1] = 255;
        settings.saveIP.mask[2] = 255;
        settings.saveIP.mask[3] = 0;

        settings.saveIP.gateway[0] = 192;
        settings.saveIP.gateway[1] = 168;
        settings.saveIP.gateway[2] = 1;
        settings.saveIP.gateway[3] = 1;

        settings.MAC[0] = 0x44;
        settings.MAC[1] = 0x84;
        settings.MAC[2] = 0x23;
        settings.MAC[3] = 0x84;
        settings.MAC[4] = 0x44;
        settings.MAC[5] = endMAC;

        settings.bridge_sett.mode_rs485 = mode_bridge_t::RTU;
        settings.bridge_sett.port = 0;

        settings.version = CURENT_VERSION;

        mem_spi.Write(settings);
        mem_spi.Read(&settings);
        finishedBlink();
    }

    settings.bridge_sett.RS485 = &huart2; // RS485 UART

    // reset link
    for (int var = 0; var <= MAX_CH_NAME; ++var)
    {
        NameCH[var].dev = NULL;
        NameCH[var].Channel_number = 0xff;
    }

    // linking the channel name with the device and channel number
    /* после поиска
    for (int var = 0; var <= MAX_ADR_DEV; ++var) {
        // check device address
        if ((devices[var].Addr >= START_ADR_I2C) && (devices[var].Addr <= (START_ADR_I2C + MAX_ADR_DEV))) {
            for (int i = 0; i < 3; ++i) {
                NameCH[devices[var].ch[i].Name_ch].dev = &devices[var];
                NameCH[devices[var].ch[i].Name_ch].Channel_number = i;
            }
        }
    }
    */

    mem_spi.SetUsedInOS(true); // switch to use in OS
    /* USER CODE END 2 */

    /* Call init function for freertos objects (in cmsis_os2.c) */
    MX_FREERTOS_Init();

    /* Start scheduler */
    osKernelStart();

    /* We should never get here as control is now taken by the scheduler */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    while (1)
    {
        /* USER CODE END WHILE */

        /* USER CODE BEGIN 3 */
    }
    /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /** Configure the main internal regulator output voltage
     */
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /** Initializes the RCC Oscillators according to the specified parameters
     * in the RCC_OscInitTypeDef structure.
     */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 6;
    RCC_OscInitStruct.PLL.PLLN = 160;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 4;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    /** Initializes the CPU, AHB and APB buses clocks
     */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
    {
        Error_Handler();
    }

    /** Enables the Clock Security System
     */
    HAL_RCC_EnableCSS();
}

/* USER CODE BEGIN 4 */

/*
// Инициализация логгера
void Logger_Init(UART_HandleTypeDef* huart) {
    // Сохраняем указатель на UART
    logger.huart = huart;
    logger.isTransmitting = 0;
    logger.txBuffer = txBuffer;
    logger.started = 1;

    // Очищаем пул сообщений
    memset(messagePoolUsed, 0, sizeof(messagePoolUsed));

    // Создаем мьютекс для пула
    osMutexDef(poolMutex);
    poolMutexHandle = osMutexCreate(osMutex(poolMutex));

    // Создаем очередь сообщений (теперь храним только индексы)
    osMessageQDef(logQueue, QUEUE_SIZE, uint32_t);
    logger.messageQueue = osMessageCreate(osMessageQ(logQueue), NULL);
}

// Обработка сообщений и отправка через UART
void Logger_Process(void) {
    if (!logger.isTransmitting) {
        osEvent event = osMessageGet(logger.messageQueue, 0); // Неблокирующее получение

        if (event.status == osEventMessage) {
            uint32_t msgIndex = event.value.v;
            if(msgIndex < QUEUE_SIZE) {
                LogMessage_t* msg = &messagePool[msgIndex];

                // Проверка валидности сообщения
                if(msg->length > 0 && msg->length < MAX_MESSAGE_SIZE) {
                    // Копируем сообщение в буфер отправки
                    memcpy(logger.txBuffer, msg->data, msg->length);

                    // Освобождаем слот в пуле
                    uint32_t primask_bit;
                    primask_bit = __get_PRIMASK();
                    __disable_irq();
                    messagePoolUsed[msgIndex] = 0;
                    if (!primask_bit) {
                        __enable_irq();
                    }

                    // Начинаем передачу
                    logger.isTransmitting = 1;

                    // Проверяем состояние DMA перед отправкой
                    if(huart2.hdmatx->State == HAL_DMA_STATE_READY) {
                        HAL_StatusTypeDef status = HAL_UART_Transmit_DMA(logger.huart, (uint8_t*)logger.txBuffer, msg->length);
                        if(status != HAL_OK) {
                            // Если DMA не готов, используем обычную передачу
                            HAL_UART_Transmit(logger.huart, (uint8_t*)logger.txBuffer, msg->length, 100);
                            logger.isTransmitting = 0;
                        }
                    } else {
                        // DMA занят, используем обычную передачу
                        HAL_UART_Transmit(logger.huart, (uint8_t*)logger.txBuffer, msg->length, 100);
                        logger.isTransmitting = 0;
                    }
                } else {
                    // Некорректный размер сообщения - просто освобождаем слот
                    uint32_t primask_bit;
                    primask_bit = __get_PRIMASK();
                    __disable_irq();
                    messagePoolUsed[msgIndex] = 0;
                    if (!primask_bit) {
                        __enable_irq();
                    }
                }
            }
        }
    }
}

// Callback завершения передачи
void Logger_TxCpltCallback(void) {
    logger.isTransmitting = 0;
}

// Функция логирования (может вызываться из прерывания или потока)
void Logger_Log(const char* format, ...) {
    if(!logger.started) return;
    if (!format) return;

    int slot = -1;

    // Безопасное получение слота с учетом контекста (прерывание или нет)
    if(isInInterrupt()) {
        // В прерывании - атомарно захватываем слот
        // Используем атомарный подход без мьютекса (мьютексы недопустимы в прерываниях)
        for(int i = 0; i < QUEUE_SIZE; i++) {
            if(messagePoolUsed[i] == 0) {
                // Попытка атомарно захватить слот
                uint32_t primask_bit;
                primask_bit = __get_PRIMASK();
                __disable_irq();
                if(messagePoolUsed[i] == 0) {
                    messagePoolUsed[i] = 1;
                    slot = i;
                }
                if (!primask_bit) {
                    __enable_irq();
                }
                if(slot >= 0) break;
            }
        }
    } else {
        // В обычном коде используем мьютекс
        osMutexWait(poolMutexHandle, osWaitForever);
        slot = getFreeMessageSlot();
        osMutexRelease(poolMutexHandle);
    }

    if(slot < 0) return; // Нет свободных слотов

    LogMessage_t* msg = &messagePool[slot];
    va_list args;

    va_start(args, format);
    int length = vsnprintf(msg->data, MAX_MESSAGE_SIZE-3, format, args);
    va_end(args);

    if (length <= 0) {
        // Атомарное освобождение слота
        uint32_t primask_bit;
        primask_bit = __get_PRIMASK();
        __disable_irq();
        messagePoolUsed[slot] = 0;
        if (!primask_bit) {
            __enable_irq();
        }
        return;
    }

    // Добавляем '\r' и нулевой символ для завершения отправки

    msg->data[length] = '\r';
    msg->data[length + 1] = '\x03';
    msg->data[length + 2] = '\x04';
    msg->length = length + 3;

    // Помещаем индекс сообщения в очередь
    osStatus status = osMessagePut(logger.messageQueue, slot, 10);
    if(status != osOK) {
        // Атомарное освобождение слота при ошибке
        uint32_t primask_bit;
        primask_bit = __get_PRIMASK();
        __disable_irq();
        messagePoolUsed[slot] = 0;
        if (!primask_bit) {
            __enable_irq();
        }
    }
}

// Функция логирования (может вызываться из прерывания или потока)
void Logger_Log_xx(const char* format, ...) {
    if(!logger.started) return;
    if (!format) return;

    int slot = -1;

    // Безопасное получение слота с учетом контекста (прерывание или нет)
    if(isInInterrupt()) {
        // В прерывании - атомарно захватываем слот
        // Используем атомарный подход без мьютекса (мьютексы недопустимы в прерываниях)
        for(int i = 0; i < QUEUE_SIZE; i++) {
            if(messagePoolUsed[i] == 0) {
                // Попытка атомарно захватить слот
                uint32_t primask_bit;
                primask_bit = __get_PRIMASK();
                __disable_irq();
                if(messagePoolUsed[i] == 0) {
                    messagePoolUsed[i] = 1;
                    slot = i;
                }
                if (!primask_bit) {
                    __enable_irq();
                }
                if(slot >= 0) break;
            }
        }
    } else {
        // В обычном коде используем мьютекс
        osMutexWait(poolMutexHandle, osWaitForever);
        slot = getFreeMessageSlot();
        osMutexRelease(poolMutexHandle);
    }

    if(slot < 0) return; // Нет свободных слотов

    LogMessage_t* msg = &messagePool[slot];
    va_list args;

    va_start(args, format);
    int length = vsnprintf(msg->data, MAX_MESSAGE_SIZE, format, args);
    va_end(args);

    if (length <= 0) {
        // Атомарное освобождение слота
        uint32_t primask_bit;
        primask_bit = __get_PRIMASK();
        __disable_irq();
        messagePoolUsed[slot] = 0;
        if (!primask_bit) {
            __enable_irq();
        }
        return;
    }

    msg->length = length;
    // Помещаем индекс сообщения в очередь
    osStatus status = osMessagePut(logger.messageQueue, slot, 10);
    if(status != osOK) {
        // Атомарное освобождение слота при ошибке
        uint32_t primask_bit;
        primask_bit = __get_PRIMASK();
        __disable_irq();
        messagePoolUsed[slot] = 0;
        if (!primask_bit) {
            __enable_irq();
        }
    }
}
*/

uint8_t ReadStraps()
{
    uint8_t tempStraps;

    // Bit0
    if (HAL_GPIO_ReadPin(MAC_b0_GPIO_Port, MAC_b0_Pin))
        SET_BIT(tempStraps, 1 << 0);
    else
        CLEAR_BIT(tempStraps, 1 << 0);
    // Bit1
    if (HAL_GPIO_ReadPin(MAC_b1_GPIO_Port, MAC_b1_Pin))
        SET_BIT(tempStraps, 1 << 1);
    else
        CLEAR_BIT(tempStraps, 1 << 1);
    // Bit2
    if (HAL_GPIO_ReadPin(MAC_b2_GPIO_Port, MAC_b2_Pin))
        SET_BIT(tempStraps, 1 << 2);
    else
        CLEAR_BIT(tempStraps, 1 << 2);
    // Bit3
    if (HAL_GPIO_ReadPin(MAC_b3_GPIO_Port, MAC_b3_Pin))
        SET_BIT(tempStraps, 1 << 3);
    else
        CLEAR_BIT(tempStraps, 1 << 3);
    // Bit4
    if (HAL_GPIO_ReadPin(MAC_b4_GPIO_Port, MAC_b4_Pin))
        SET_BIT(tempStraps, 1 << 4);
    else
        CLEAR_BIT(tempStraps, 1 << 4);
    // Bit5
    if (HAL_GPIO_ReadPin(MAC_b5_GPIO_Port, MAC_b5_Pin))
        SET_BIT(tempStraps, 1 << 5);
    else
        CLEAR_BIT(tempStraps, 1 << 5);
    // Bit6
    if (HAL_GPIO_ReadPin(MAC_b6_GPIO_Port, MAC_b6_Pin))
        SET_BIT(tempStraps, 1 << 6);
    else
        CLEAR_BIT(tempStraps, 1 << 6);
    // Bit7
    if (HAL_GPIO_ReadPin(MAC_b7_GPIO_Port, MAC_b7_Pin))
        SET_BIT(tempStraps, 1 << 7);
    else
        CLEAR_BIT(tempStraps, 1 << 7);

    return tempStraps;
}

void finishedBlink()
{
#define timeBetween 300

    // finished blink
    HAL_GPIO_WritePin(B_GPIO_Port, B_Pin, GPIO_PIN_SET); // PC15 VD4
    HAL_GPIO_WritePin(R_GPIO_Port, R_Pin, GPIO_PIN_SET); // PC13 VD2
    HAL_GPIO_WritePin(G_GPIO_Port, G_Pin, GPIO_PIN_SET); // PC14 VD3

    for (int var = 0; var < 5; ++var)
    {
        HAL_GPIO_WritePin(G_GPIO_Port, G_Pin, GPIO_PIN_RESET); // PC14 VD3
        HAL_Delay(timeBetween);
        HAL_GPIO_WritePin(G_GPIO_Port, G_Pin, GPIO_PIN_SET);   // PC14 VD3
        HAL_GPIO_WritePin(R_GPIO_Port, R_Pin, GPIO_PIN_RESET); // PC13 VD2
        HAL_Delay(timeBetween);
        HAL_GPIO_WritePin(R_GPIO_Port, R_Pin, GPIO_PIN_SET);   // PC13 VD2
        HAL_GPIO_WritePin(B_GPIO_Port, B_Pin, GPIO_PIN_RESET); // PC15 VD4
        HAL_Delay(timeBetween);
        HAL_GPIO_WritePin(B_GPIO_Port, B_Pin, GPIO_PIN_SET); // PC15 VD4
    }

    HAL_GPIO_WritePin(B_GPIO_Port, B_Pin, GPIO_PIN_SET); // PC15 VD4
    HAL_GPIO_WritePin(R_GPIO_Port, R_Pin, GPIO_PIN_SET); // PC13 VD2
    HAL_GPIO_WritePin(G_GPIO_Port, G_Pin, GPIO_PIN_SET); // PC14 VD3
}

void timoutBlink()
{
    // timOut plink  all
    HAL_GPIO_WritePin(B_GPIO_Port, B_Pin, GPIO_PIN_RESET); // PC15 VD4
    HAL_GPIO_WritePin(R_GPIO_Port, R_Pin, GPIO_PIN_RESET); // PC13 VD2
    HAL_GPIO_WritePin(G_GPIO_Port, G_Pin, GPIO_PIN_RESET); // PC14 VD3
    for (int var = 0; var < 5; ++var)
    {
        HAL_GPIO_TogglePin(B_GPIO_Port, B_Pin); // PC15 VD4
        HAL_GPIO_TogglePin(R_GPIO_Port, R_Pin); // PC13 VD2
        HAL_GPIO_TogglePin(G_GPIO_Port, G_Pin); // PC14 VD3
        HAL_Delay(800);
    }
}

/*
 * функция установки нового устройства
 * Addr - I2C адрес
 * CH	- один из трех канвлов
 * Name	- глобальное имя от 1 до 45
 */
/*
int set_i2c_dev(uint8_t Addr, uint8_t CH, uint8_t Name) {
    uint8_t ret = 0, dev = (Addr - START_ADR_I2C);

    // проверка входных данных
    if (CH > 2) {
        return 1;
    }
    if ((Name > MAX_CH_NAME)) {
        return 2;
    }
    //если вышли за диапазон
    if ((Addr < START_ADR_I2C) || (Addr > (START_ADR_I2C + MAX_ADR_DEV))) {
        return 3;
    }

    //mem_spi.W25qxx_EraseSector(0);
    NameCH[Name].dev = &settings.devices[dev];
    NameCH[Name].Channel_number = CH;

    // записываем данные в память и сохраняем на флешку
    settings.devices[dev].Addr = Addr;
    settings.devices[dev].AddrFromDev = 0;
    settings.devices[dev].ch[CH].Name_ch = Name;
    settings.devices[dev].ERR_counter = 0;
    settings.devices[dev].last_ERR = 0;
    settings.devices[dev].TypePCB = PCBType::NoInit;

    settings.devices[dev].ch[CH].Current = 0;
    settings.devices[dev].ch[CH].IsOn = 0;
    settings.devices[dev].ch[CH].On_off = 0;
    settings.devices[dev].ch[CH].PWM = 0;
    settings.devices[dev].ch[CH].PWM_out = 0;
    //mem_spi.Write(settings);

    return ret;
}*/

/*
 * функция удаления устройства
 * Addr - I2C адрес
 * CH	- один из трех канвлов
 * Name	- глобальное имя от 1 до 45
 */
/*
int del_Name_dev(uint8_t Name) {
    uint8_t ret = 0;

    if ((Name > 44)) {
        return -2;
    }

    //mem_spi.W25qxx_EraseSector(0);
    // записываем данные в память и сохраняем на флешку
    //NameCH[Name].dev = &settings.devices[dev];
    uint8_t CH = NameCH[Name].Channel_number;

    // записываем данные в память и сохраняем на флешку
    NameCH[Name].dev->Addr = 0xff;
    NameCH[Name].dev->AddrFromDev = 0xff;
    NameCH[Name].dev->ch[CH].Name_ch = 0xff;
    NameCH[Name].dev->ERR_counter = 0xffffffff;
    NameCH[Name].dev->last_ERR = 0xffffffff;
    NameCH[Name].dev->TypePCB = PCBType::NoInit;

    NameCH[Name].dev->ch[CH].Current = 0xffff;
    NameCH[Name].dev->ch[CH].IsOn = 0xff;
    NameCH[Name].dev->ch[CH].On_off = 0xff;
    NameCH[Name].dev->ch[CH].PWM = 0xffffffff;
    NameCH[Name].dev->ch[CH].PWM_out = 0xffffffff;

    NameCH[Name].dev = NULL;
    NameCH[Name].Channel_number = 0xff;
    //mem_spi.Write(settings);

    return ret;
}*/
/*
void setRange_i2c_dev(uint8_t startAddres, uint8_t quantity) {
    // Clear all
    cleanAll_i2c_dev();

    uint8_t name_num = 0;
    for (int var = 0; var < quantity; ++var) {
        for (int ch = 0; ch < 3; ++ch) {
            set_i2c_dev(startAddres + var, ch, name_num);
            ++name_num;
            settings.devices_depth++;
        }
    }
}

void cleanAll_i2c_dev() {
    // Clear all
    for (int var = 0; var <= MAX_CH_NAME; ++var) {
        del_Name_dev(var);
    }
    del_all_dev();
}

void del_all_dev() {
    for (int var = 0; var < MAX_ADR_DEV; ++var) {

        settings.devices[var].Addr = 0xff;
        settings.devices[var].AddrFromDev = 0xff;
        settings.devices[var].ERR_counter = 0xffffffff;
        settings.devices[var].last_ERR = 0xffffffff;
        settings.devices[var].TypePCB = PCBType::NoInit;

        for (int CH = 0; CH < 3; ++CH) {
            settings.devices[var].ch[CH].Current = 0xffff;
            settings.devices[var].ch[CH].IsOn = 0xff;
            settings.devices[var].ch[CH].On_off = 0xff;
            settings.devices[var].ch[CH].PWM = 0xffffffff;
            settings.devices[var].ch[CH].PWM_out = 0xffffffff;
            settings.devices[var].ch[CH].Name_ch = 0xff;
        }

    }
    settings.devices_depth = 0;
}*/

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{

    if (huart->Instance == settings.bridge_sett.RS485->Instance)
    {
        SizeInModBus = Size;
        HAL_UARTEx_ReceiveToIdle_IT(settings.bridge_sett.RS485, response, 256); // Read data
        osSemaphoreRelease(Resive_USARTHandle);
    }

    if (huart->Instance == DBG_PORT_NAME)
    {
        // Проверяем, что DMA_TC флаг установлен
        while (__HAL_UART_GET_FLAG(huart, UART_FLAG_TC) != SET)
        {
            // Ожидаем завершения передачи
        };

        uint16_t Size_Data = Size - Start_index;

        HAL_UART_RxEventTypeTypeDef rxEventType;
        rxEventType = HAL_UARTEx_GetRxEventType(huart);
        switch (rxEventType)
        {
        case HAL_UART_RXEVENT_IDLE:
            // Копируем данные
            memcpy(&message_rx[indx_message_rx], &UART_debug_rx[Start_index], Size_Data);

            if ((message_rx[indx_message_rx + Size_Data - 1] == '\r') ||
                (message_rx[indx_message_rx + Size_Data - 1] == 0))
            {
                message_rx[indx_message_rx + Size_Data] = 0;

                // Отправляем сообщение в очередь с таймаутом 0
                osStatus status = osMessagePut(rxDataUART2Handle, (uint32_t)indx_message_rx, 0);
                if (status != osOK)
                {
                    // Если очередь заполнена, очищаем ее
                    osEvent evt;
                    do
                    {
                        evt = osMessageGet(rxDataUART2Handle, 0);
                    } while (evt.status == osEventMessage);

                    // Пытаемся отправить снова
                    status = osMessagePut(rxDataUART2Handle, (uint32_t)indx_message_rx, 0);
                }

                Size_message = 0;
                indx_message_rx = 0;
            }
            else
            {
                indx_message_rx += Size_Data;
            }

            Start_index = Size;
            break;

        case HAL_UART_RXEVENT_TC:
            // Копируем в начало буфера
            memcpy(&message_rx[indx_message_rx], &UART_debug_rx[Start_index], Size_Data);
            indx_message_rx += Size_Data;
            Start_index = 0;
            break;

        default:
            STM_LOG("Неизвестный тип события UART: %d", rxEventType);
            break;
        }

        // Перезапускаем DMA для приема
        HAL_UARTEx_ReceiveToIdle_DMA(huart, UART_debug_rx, UART6_RX_LENGTH);
        __HAL_DMA_DISABLE_IT(&hdma_usart6_rx, DMA_IT_HT);
    }

    if (huart->Instance == USART1)
    {
        // Проверяем, что DMA_TC флаг установлен
        while (__HAL_UART_GET_FLAG(huart, UART_FLAG_TC) != SET)
        {
            // Ожидаем завершения передачи
        };
        memcpy(message_rx, UART_rx, Size);
        // Отправляем сообщение в очередь с таймаутом 0
        osStatus status = osMessagePut(rxDataUART1Handle, (uint32_t)Size, 0);
        if (status != osOK)
        {
            // Если очередь заполнена, очищаем ее
            osEvent evt;
            do
            {
                evt = osMessageGet(rxDataUART1Handle, 0);
            } while (evt.status == osEventMessage);

            // Пытаемся отправить снова
            status = osMessagePut(rxDataUART1Handle, (uint32_t)Size, 0);
        }

        HAL_UARTEx_ReceiveToIdle_DMA(huart, UART_rx, UART_RX_LENGTH);
        __HAL_DMA_DISABLE_IT(&hdma_usart1_rx, DMA_IT_HT);
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{

    if (huart->Instance == USART1)
    {
        HAL_UART_DMAStop(huart);
        rx_end = 1;
    }
    if (huart->Instance == settings.bridge_sett.RS485->Instance)
    {
    }
}

// Обработчик прерывания DMA UART
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == logger.huart)
    {
        Logger_TxCpltCallback();
    }

    if (huart->Instance == USART1)
    {
    }

    if (huart->Instance == settings.bridge_sett.RS485->Instance)
    {
    }
}
/* USER CODE END 4 */

/**
 * @brief  Period elapsed callback in non blocking mode
 * @note   This function is called  when TIM7 interrupt took place, inside
 * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
 * a global variable "uwTick" used as application time base.
 * @param  htim : TIM handle
 * @retval None
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    /* USER CODE BEGIN Callback 0 */

    /* USER CODE END Callback 0 */
    if (htim->Instance == TIM7)
    {
        HAL_IncTick();
    }
    /* USER CODE BEGIN Callback 1 */

    /* USER CODE END Callback 1 */
}

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
    /* USER CODE BEGIN Error_Handler_Debug */
    /* User can add his own implementation to report the HAL error return state */
    STM_LOG("Error handler");
    __disable_irq();
    while (1)
    {
    }
    /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line)
{
    /* USER CODE BEGIN 6 */
    /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
    /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
