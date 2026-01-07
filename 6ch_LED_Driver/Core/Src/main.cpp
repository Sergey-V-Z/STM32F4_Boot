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
#include "adc.h"
#include "dma.h"
#include "lwip.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
// #include "flash_spi.h"
#include "Delay_us_DWT.h"
#include "LED.h"
#include "flash_spi.h"
#include "cmsis_os.h"
#include "firmware_update.h"
#include "pwm_controller.h"
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

uint32_t count_tic = 0; // для замеров времени выполнения кода

led_t LED_IPadr;
led_t LED_error;
led_t LED_OSstart;

flash mem_spi;

bool resetSettings = false;
bool resetFlash = false;

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

extern osMessageQId rxDataUART6Handle;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void MX_FREERTOS_Init(void);
/* USER CODE BEGIN PFP */
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
    //SCB->VTOR = FLASH_BASE | 0x10000; /* 0x08010000 */
    extern uint32_t _app_start;
    SCB->VTOR = (uint32_t)&_app_start;
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
  MX_USART6_UART_Init();
  MX_ADC1_Init();
  MX_TIM1_Init();
  MX_TIM4_Init();
  /* USER CODE BEGIN 2 */

    // Инициализация PWM контроллера
    PWM_Init(&htim1, &htim4);

    mem_spi.Init(&hspi3, SPI_FLASH_CONFIG_ADDRESS, ChipSelect, WriteProtect, Hold, false);
    // HAL_Delay(100);
    if(resetFlash)
    {
      STM_LOG("Flash reset");
      mem_spi.W25qxx_EraseChip();
      resetFlash = false;
      STM_LOG("Flash reset done");
    }
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

        settings.version = CURENT_VERSION;

        mem_spi.Write(settings);
        mem_spi.Read(&settings);
        finishedBlink();
    }

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
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
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

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    // Обработка UART1/UART2 удалена - вторичные контроллеры не используются

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
                osStatus status = osMessagePut(rxDataUART6Handle, (uint32_t)indx_message_rx, 0);
                if (status != osOK)
                {
                    // Если очередь заполнена, очищаем ее
                    osEvent evt;
                    do
                    {
                        evt = osMessageGet(rxDataUART6Handle, 0);
                    } while (evt.status == osEventMessage);

                    // Пытаемся отправить снова
                    status = osMessagePut(rxDataUART6Handle, (uint32_t)indx_message_rx, 0);
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
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    // Callback для UART1/UART2 удалён - больше не используются
}

// Обработчик прерывания DMA UART
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == logger.huart)
    {
        Logger_TxCpltCallback();
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
  if (htim->Instance == TIM7) {
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

#ifdef  USE_FULL_ASSERT
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
