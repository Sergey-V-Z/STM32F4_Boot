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
#include "bootloader.h"
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
extern boot_data_t BootloaderData;
/* USER CODE END Variables */
osThreadId mainTaskHandle;
osThreadId uartTaskHandle;
osThreadId ethTaskHandle;
osThreadId bootloaderTaskHandle;
osMessageQId rxDataUART2Handle;

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartMainTask(void const * argument);
void UartTask(void const * argument);
void EthTask(void const * argument);
void BootloaderTask(void const * argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* GetIdleTaskMemory prototype (linked to static allocation support) */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize );

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

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of mainTask */
  osThreadDef(mainTask, StartMainTask, osPriorityNormal, 0, 512);
  mainTaskHandle = osThreadCreate(osThread(mainTask), NULL);

  /* definition and creation of uartTask */
  osThreadDef(uartTask, UartTask, osPriorityNormal, 0, 512);
  uartTaskHandle = osThreadCreate(osThread(uartTask), NULL);

  /* definition and creation of ethTask */
  osThreadDef(ethTask, EthTask, osPriorityNormal, 0, 512);
  ethTaskHandle = osThreadCreate(osThread(ethTask), NULL);

  /* definition and creation of bootloaderTask */
  osThreadDef(bootloaderTask, BootloaderTask, osPriorityIdle, 0, 512);
  bootloaderTaskHandle = osThreadCreate(osThread(bootloaderTask), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

}

/* USER CODE BEGIN Header_StartMainTask */
/**
  * @brief  Function implementing the mainTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartMainTask */
void StartMainTask(void const * argument)
{
  /* USER CODE BEGIN StartMainTask */
  /* Infinite loop */
  for(;;)
  {
	    // Если загрузчик находится в состоянии ошибки, попытаемся сбросить систему
	    if (BootloaderData.status.State == BOOTLOADER_STATE_ERROR) {
	      // Задержка перед сбросом
	      osDelay(10000);
	      // Сброс системы
	      //NVIC_SystemReset();
	    }

	    // Проверка состояния системы
	    osDelay(1000);
  }
  /* USER CODE END StartMainTask */
}

/* USER CODE BEGIN Header_UartTask */
/**
* @brief Function implementing the uartTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_UartTask */
void UartTask(void const * argument)
{
  /* USER CODE BEGIN UartTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(100);
  }
  /* USER CODE END UartTask */
}

/* USER CODE BEGIN Header_EthTask */
/**
* @brief Function implementing the ethTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_EthTask */
void EthTask(void const * argument)
{
  /* USER CODE BEGIN EthTask */
  /* Infinite loop */
  for(;;)
  {
	    // Обработка Ethernet для загрузчика
	    osDelay(10);
  }
  /* USER CODE END EthTask */
}

/* USER CODE BEGIN Header_BootloaderTask */
/**
* @brief Function implementing the bootloaderTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_BootloaderTask */
void BootloaderTask(void const * argument)
{
  /* USER CODE BEGIN BootloaderTask */
  /* Infinite loop */
  for(;;)
  {
	    // Вызываем функцию обработки состояния загрузчика
	    Bootloader_Run();

	    // Короткая задержка для передачи управления другим задачам
	    osDelay(10);
  }
  /* USER CODE END BootloaderTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */
