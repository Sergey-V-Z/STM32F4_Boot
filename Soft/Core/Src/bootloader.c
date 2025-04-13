/**
  ******************************************************************************
  * @file    bootloader.c
  * @brief   Main bootloader functionality.
  *          This file contains the main bootloader logic and state machine.
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "bootloader.h"
#include "crc.h"

/* Private typedef -----------------------------------------------------------*/
typedef void (*pFunction)(void);

/* Private define ------------------------------------------------------------*/
#define BOOTLOADER_VERSION                 0x00010000  /* Bootloader version 1.0.0.0 */
#define BOOTLOADER_FIRMWARE_BUFFER_SIZE    1024        /* Size of buffer for firmware updates */

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
Bootloader_Status_t BootloaderStatus; // Сделаем глобальной для доступа из других модулей
static Recovery_Reason_t RecoveryReason = RECOVERY_REASON_NONE;
static uint8_t FirmwareBuffer[BOOTLOADER_FIRMWARE_BUFFER_SIZE];
static SPI_Flash_Firmware_Metadata_t CurrentFirmwareMetadata;

/* Private function prototypes -----------------------------------------------*/
static void Bootloader_InitStatus(void);
static void Bootloader_EnterRecoveryMode(Recovery_Reason_t reason);
static void Bootloader_ProcessState(void);
static void Bootloader_ResetApplication(void);
static uint8_t Bootloader_ValidateApplication(void);

/* Functions -----------------------------------------------------------------*/

/**
  * @brief  Initializes the bootloader.
  * @param  None
  * @retval None
  */
void Bootloader_Init(void)
{
  /* Initialize status to default values */
  Bootloader_InitStatus();
  
  /* Initialize Flash utilities */
  if (FLASH_Utils_Unlock() != HAL_OK)
  {
    BootloaderStatus.State = BOOTLOADER_STATE_ERROR;
    BootloaderStatus.ErrorCode = BOOTLOADER_ERROR_FLASH_INIT_FAILED;
    return;
  }
  FLASH_Utils_Lock();
  
  /* Initialize SPI Flash */
  if (!SPI_Flash_Init())
  {
    BootloaderStatus.State = BOOTLOADER_STATE_ERROR;
    BootloaderStatus.ErrorCode = BOOTLOADER_ERROR_FLASH_INIT_FAILED;
    return;
  }
  
  /* Increment and check reset counter */
  BootloaderStatus.ResetCount = FLASH_Utils_IncrementResetCounter();
  
  /* Check if we need to enter recovery mode due to excessive resets */
  if (BootloaderStatus.ResetCount >= MAX_RESET_COUNT)
  {
    Bootloader_EnterRecoveryMode(RECOVERY_REASON_RESET_COUNT);
    return;
  }
  
  /* Move to application checking state */
  BootloaderStatus.State = BOOTLOADER_STATE_CHECK_APP;
}

/**
  * @brief  Runs the bootloader main process.
  * @param  None
  * @retval None
  */
void Bootloader_Run(void)
{
  /* Process the current state */
  Bootloader_ProcessState();
}

/**
  * @brief  Gets the current bootloader status.
  * @param  status: Pointer to store the bootloader status
  * @retval None
  */
void Bootloader_GetStatus(Bootloader_Status_t *status)
{
  if (status != NULL)
  {
    *status = BootloaderStatus;
  }
}

/**
  * @brief  Forces entry into recovery mode.
  * @param  None
  * @retval None
  */
void Bootloader_ForceRecovery(void)
{
  Bootloader_EnterRecoveryMode(RECOVERY_REASON_FORCED);
}

/**
  * @brief  Jumps to the application.
  * @param  None
  * @retval None (does not return if successful)
  */
void Bootloader_JumpToApplication(void)
{
  /* Check if application is valid */
  if (!Bootloader_IsApplicationValid())
  {
    Bootloader_EnterRecoveryMode(RECOVERY_REASON_INVALID_APP);
    return;
  }
  
  /* Reset recovery reason */
  RecoveryReason = RECOVERY_REASON_NONE;
  
  /* Reset the reset counter */
  FLASH_Utils_ResetResetCounter();
  
  /* Set bootloader key to indicate a valid jump */
  FLASH_Utils_SetBootloaderKey();
  
  /* Деинициализация всей периферии */

  /* Деинициализация GPIO */
  HAL_GPIO_DeInit(GPIOA, GPIO_PIN_All);
  HAL_GPIO_DeInit(GPIOB, GPIO_PIN_All);
  HAL_GPIO_DeInit(GPIOC, GPIO_PIN_All);
  HAL_GPIO_DeInit(GPIOD, GPIO_PIN_All);
  HAL_GPIO_DeInit(GPIOE, GPIO_PIN_All);

  /* Деинициализация SPI */
  if(hspi3.Instance != NULL)
    HAL_SPI_DeInit(&hspi3);

  /* Деинициализация UART */
  if(huart2.Instance != NULL)
    HAL_UART_DeInit(&huart2);

  /* Деинициализация таймеров */
  /* Добавьте деинициализацию других таймеров, если они используются */
  if(htim14.Instance != NULL)
    HAL_TIM_Base_DeInit(&htim14);

  /* Деинициализация DMA */
  HAL_DMA_DeInit(&hdma_usart2_rx);
  HAL_DMA_DeInit(&hdma_usart2_tx);

  /* Сброс тактирования периферии */
  RCC->AHB1ENR = 0x00000000;
  RCC->AHB2ENR = 0x00000000;
  RCC->AHB3ENR = 0x00000000;
  RCC->APB1ENR = 0x00000000;
  RCC->APB2ENR = 0x00000000;

  /* Сброс всех настроек тактирования */
  HAL_RCC_DeInit();

  /* Disable all interrupts */
  __disable_irq();
  
  /* Set the vector table offset to the application's vector table */
  SCB->VTOR = MAIN_PROGRAM_START_ADDRESS;
  
  /* Get the application's reset handler address */
  uint32_t jumpAddress = *(__IO uint32_t*)(MAIN_PROGRAM_START_ADDRESS + 4);
  pFunction jumpToApplication = (pFunction)jumpAddress;
  
  /* Set the application's stack pointer */
  __set_MSP(*(__IO uint32_t*)MAIN_PROGRAM_START_ADDRESS);
  
  /* Jump to the application */
  jumpToApplication();
  
  /* Should never reach here */
  BootloaderStatus.State = BOOTLOADER_STATE_ERROR;
  BootloaderStatus.ErrorCode = BOOTLOADER_ERROR_UNKNOWN;
}

/**
  * @brief  Checks if the application is valid.
  * @param  None
  * @retval 1 if valid, 0 if invalid
  */
uint8_t Bootloader_IsApplicationValid(void)
{
  return Bootloader_ValidateApplication();
}

/**
  * @brief  Handles incoming data for firmware update.
  * @param  data: Pointer to the firmware data
  * @param  size: Size of the data
  * @param  offset: Offset into the firmware where this data should be written
  * @retval 1 if successful, 0 if failed
  */
uint8_t Bootloader_HandleFirmwareData(uint8_t *data, uint32_t size, uint32_t offset)
{
  // Функция больше не будет вызываться, но оставим заглушку
  BootloaderStatus.ErrorCode = BOOTLOADER_ERROR_INVALID_STATE;
  return 0;
}

/**
  * @brief  Finalizes firmware update.
  * @param  None
  * @retval 1 if successful, 0 if failed
  */
uint8_t Bootloader_FinalizeFirmwareUpdate(void)
{
  // Функция больше не будет вызываться, но оставим заглушку
  BootloaderStatus.ErrorCode = BOOTLOADER_ERROR_INVALID_STATE;
  return 0;
}

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Initializes bootloader status to default values.
  * @param  None
  * @retval None
  */
static void Bootloader_InitStatus(void)
{
  BootloaderStatus.State = BOOTLOADER_STATE_INIT;
  BootloaderStatus.ErrorCode = BOOTLOADER_ERROR_NONE;
  BootloaderStatus.ResetCount = 0;
  BootloaderStatus.IsAppValid = 0;
  BootloaderStatus.IsRecovery = 0;
  BootloaderStatus.LastFlashAddress = 0;
  BootloaderStatus.BytesFlashed = 0;
}

/**
  * @brief  Enters recovery mode with the specified reason.
  * @param  reason: Reason for entering recovery mode
  * @retval None
  */
static void Bootloader_EnterRecoveryMode(Recovery_Reason_t reason)
{
  RecoveryReason = reason;
  BootloaderStatus.State = BOOTLOADER_STATE_RECOVERY;
  BootloaderStatus.IsRecovery = 1;
  
  // Сразу переходим к восстановлению
  BootloaderStatus.State = BOOTLOADER_STATE_RECOVERY;
}

/**
  * @brief  Processes the current bootloader state.
  * @param  None
  * @retval None
  */
/**
  * @brief  Processes the current bootloader state.
  * @param  None
  * @retval None
  */
/**
  * @brief  Processes the current bootloader state.
  * @param  None
  * @retval None
  */
static void Bootloader_ProcessState(void)
{
  switch (BootloaderStatus.State)
  {
    case BOOTLOADER_STATE_INIT:
      /* Should not be in this state, move to check app */
      BootloaderStatus.State = BOOTLOADER_STATE_CHECK_APP;
      break;
      
    case BOOTLOADER_STATE_CHECK_APP:
      /* Check if bootloader key is valid */
      if (FLASH_Utils_ReadBootloaderKey() == BOOTLOADER_KEY_VALUE)
      {
        /* Key is valid, reset it */
        FLASH_Utils_ResetBootloaderKey();
        
        /* Check if application is valid */
        if (Bootloader_ValidateApplication())
        {
          BootloaderStatus.IsAppValid = 1;
          BootloaderStatus.State = BOOTLOADER_STATE_JUMP_TO_APP;
        }
        else
        {
          BootloaderStatus.IsAppValid = 0;
          Bootloader_EnterRecoveryMode(RECOVERY_REASON_INVALID_APP);
        }
      }
      else
      {
        /* Check if application is present */
        if (Bootloader_ValidateApplication())
        {
          BootloaderStatus.IsAppValid = 1;
          BootloaderStatus.State = BOOTLOADER_STATE_JUMP_TO_APP;
        }
        else
        {
          BootloaderStatus.IsAppValid = 0;
          Bootloader_EnterRecoveryMode(RECOVERY_REASON_NO_APP);
        }
      }
      break;
      
    case BOOTLOADER_STATE_JUMP_TO_APP:
      /* Jump to the application */
      Bootloader_JumpToApplication();
      /* If we're here, jump failed */
      Bootloader_EnterRecoveryMode(RECOVERY_REASON_INVALID_APP);
      break;
      

    case BOOTLOADER_STATE_RECOVERY:
      /* Проверяем наличие резервной прошивки в SPI Flash */
      if (Bootloader_CheckBackupFirmware()) {
        /* Attempt to restore from backup copy in SPI Flash */
        if (SPI_Flash_CopyFirmwareToInternal(1)) {
          /* If restore is successful, prepare to jump to application */
          BootloaderStatus.State = BOOTLOADER_STATE_JUMP_TO_APP;
        }
        else {
          /* Восстановление не удалось, проверяем наличие прошивки во внутренней флеш-памяти */
          if (Bootloader_ValidateApplication()) {
            /* Существующая прошивка во внутренней флеш-памяти валидна, пытаемся запустить */
            BootloaderStatus.IsAppValid = 1;
            BootloaderStatus.State = BOOTLOADER_STATE_JUMP_TO_APP;
          } else {
            /* Нет валидной прошивки ни во внешней, ни во внутренней памяти */
            BootloaderStatus.State = BOOTLOADER_STATE_ERROR;
            BootloaderStatus.ErrorCode = BOOTLOADER_ERROR_RECOVERY_FAILED;
          }
        }
      } else {
        /* Резервная прошивка не найдена, проверяем наличие прошивки во внутренней флеш-памяти */
        if (Bootloader_ValidateApplication()) {
          /* Существующая прошивка во внутренней флеш-памяти валидна, пытаемся запустить */
          BootloaderStatus.IsAppValid = 1;
          BootloaderStatus.State = BOOTLOADER_STATE_JUMP_TO_APP;
        } else {
          /* Нет валидной прошивки ни во внешней, ни во внутренней памяти */
          BootloaderStatus.State = BOOTLOADER_STATE_ERROR;
          BootloaderStatus.ErrorCode = BOOTLOADER_ERROR_RECOVERY_FAILED;

          /* Индикация ошибки - отсутствие прошивки */
          HAL_GPIO_WritePin(R_GPIO_Port, R_Pin, GPIO_PIN_RESET);
          HAL_GPIO_WritePin(G_GPIO_Port, G_Pin, GPIO_PIN_SET);
          HAL_GPIO_WritePin(B_GPIO_Port, B_Pin, GPIO_PIN_SET);
        }
      }
      break;

    case BOOTLOADER_STATE_WAIT_COMMAND:
      /* We don't wait for commands anymore, just attempt recovery */
      BootloaderStatus.State = BOOTLOADER_STATE_RECOVERY;
      break;
      
    case BOOTLOADER_STATE_FLASHING:
      /* This state is not used without external update commands */
      BootloaderStatus.State = BOOTLOADER_STATE_ERROR;
      BootloaderStatus.ErrorCode = BOOTLOADER_ERROR_INVALID_STATE;
      break;
      
    case BOOTLOADER_STATE_ERROR:
    	/* In error state, we just wait for reset */
		/* мигания SOS */
		for (int i = 0; i < 3; i++) {
		  HAL_GPIO_WritePin(R_GPIO_Port, R_Pin, GPIO_PIN_RESET);
		  HAL_Delay(200);
		  HAL_GPIO_WritePin(R_GPIO_Port, R_Pin, GPIO_PIN_SET);
		  HAL_Delay(200);
		}
		HAL_Delay(400);
		for (int i = 0; i < 3; i++) {
		  HAL_GPIO_WritePin(R_GPIO_Port, R_Pin, GPIO_PIN_RESET);
		  HAL_Delay(500);
		  HAL_GPIO_WritePin(R_GPIO_Port, R_Pin, GPIO_PIN_SET);
		  HAL_Delay(200);
		}
		HAL_Delay(400);
		for (int i = 0; i < 3; i++) {
		  HAL_GPIO_WritePin(R_GPIO_Port, R_Pin, GPIO_PIN_RESET);
		  HAL_Delay(200);
		  HAL_GPIO_WritePin(R_GPIO_Port, R_Pin, GPIO_PIN_SET);
		  HAL_Delay(200);
		}
		HAL_Delay(1000);
      break;
      
    default:
      /* Unknown state, move to error */
      BootloaderStatus.State = BOOTLOADER_STATE_ERROR;
      BootloaderStatus.ErrorCode = BOOTLOADER_ERROR_UNKNOWN;
      break;
  }
}


/**
  * @brief  Validates the application in internal flash.
  * @param  None
  * @retval 1 if valid, 0 if invalid
  */
static uint8_t Bootloader_ValidateApplication(void)
{
  /* Проверяем, что область приложения не стёрта (первые байты не равны 0xFF) */
  uint32_t* appStart = (uint32_t*)MAIN_PROGRAM_START_ADDRESS;
  if (*appStart == 0xFFFFFFFF) {
    return 0; /* Область стёрта или пуста */
  }

  /* Check if the stack pointer points to valid RAM area */
  uint32_t sp = *(__IO uint32_t*)MAIN_PROGRAM_START_ADDRESS;

  /* Проверка корректности указателя стека */
  if (sp < 0x20000000 || sp > 0x20030000) {
    return 0; /* Недопустимый указатель стека */
  }

  /* Check if reset vector points to valid flash area */
  uint32_t resetVector = *(__IO uint32_t*)(MAIN_PROGRAM_START_ADDRESS + 4);
  
  if (resetVector < MAIN_PROGRAM_START_ADDRESS || resetVector > 0x08100000) /* 1MB flash max */
  {
    return 0;
  }

  /* Дополнительная проверка на наличие валидных векторов прерываний */
  for (int i = 0; i < 4; i++) {
    uint32_t vector = *(__IO uint32_t*)(MAIN_PROGRAM_START_ADDRESS + (i * 4));
    if (vector == 0xFFFFFFFF) {
      return 0; /* Вектор не задан - неполная прошивка */
    }
  }

  return 1; /* Все проверки пройдены */
}

/**
  * @brief  Resets the application area in internal flash.
  * @param  None
  * @retval None
  */
static void Bootloader_ResetApplication(void)
{
  FLASH_Utils_Unlock();
  
  /* Determine sectors to erase */
  uint32_t startSector = FLASH_Utils_GetSector(MAIN_PROGRAM_START_ADDRESS);
  uint32_t endSector = FLASH_Utils_GetSector(0x08100000 - 1); /* End of flash */
  
  /* Erase each sector */
  for (uint32_t sector = startSector; sector <= endSector; sector++)
  {
    FLASH_Utils_EraseSector(sector);
  }
  
  FLASH_Utils_Lock();
}

/**
  * @brief  Checks if backup firmware exists in SPI Flash.
  * @param  None
  * @retval 1 if valid backup firmware exists, 0 if not
  */
uint8_t Bootloader_CheckBackupFirmware(void)
{
  SPI_Flash_Firmware_Metadata_t metadata;

  /* Чтение метаданных резервной прошивки */
  if (!SPI_Flash_ReadFirmwareMetadata(1, &metadata))
    return 0;

  /* Проверка ограничений на размер прошивки */
  if (metadata.FirmwareSize == 0 || metadata.FirmwareSize > 0x80000) // не более 512KB
    return 0;

  /* Проверка версии и контрольной суммы */
  if (metadata.FirmwareVersion == 0)
    return 0;

  /* Проверка адреса назначения */
  if (metadata.TargetAddress != MAIN_PROGRAM_START_ADDRESS)
    return 0;

  /* Проверка валидности данных */
  uint8_t tempBuffer[256]; // буфер для чтения части прошивки
  uint32_t address = SPI_FLASH_BACKUP_FW_ADDRESS + sizeof(SPI_Flash_Firmware_Metadata_t);

  /* Чтение первых байтов прошивки для проверки */
  if (!SPI_Flash_Read(address, tempBuffer, sizeof(tempBuffer)))
    return 0;

  /* Проверка наличия валидного стека и вектора сброса (первые два слова) */
  uint32_t stackPointer = *((uint32_t*)tempBuffer);
  uint32_t resetVector = *((uint32_t*)(tempBuffer + 4));

  /* Проверка, что стек указывает на область RAM */
  if (stackPointer < 0x20000000 || stackPointer > 0x20020000) /* 128KB RAM */
    return 0;

  /* Проверка, что вектор сброса указывает на область Flash */
  if (resetVector < MAIN_PROGRAM_START_ADDRESS || resetVector > 0x08100000) /* 1MB Flash */
    return 0;

  return 1; /* Прошивка существует и, предположительно, валидна */
}
