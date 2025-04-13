/**
  ******************************************************************************
  * @file    bootloader.h
  * @brief   Header for bootloader.c file.
  *          This file contains the main bootloader functionality.
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __BOOTLOADER_H
#define __BOOTLOADER_H

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include "flash_utils.h"
#include "spi_flash.h"

/* Exported types ------------------------------------------------------------*/

/* Bootloader state enum */
typedef enum {
  BOOTLOADER_STATE_INIT,           /* Bootloader initializing */
  BOOTLOADER_STATE_CHECK_APP,      /* Checking if application is valid */
  BOOTLOADER_STATE_JUMP_TO_APP,    /* Jumping to application */
  BOOTLOADER_STATE_RECOVERY,       /* Entering recovery mode */
  BOOTLOADER_STATE_WAIT_COMMAND,   /* Waiting for commands in recovery mode */
  BOOTLOADER_STATE_FLASHING,       /* Flashing new firmware */
  BOOTLOADER_STATE_ERROR           /* Error state */
} Bootloader_State_t;

/* Bootloader status struct */
typedef struct {
  Bootloader_State_t State;        /* Current state of bootloader */
  uint32_t ErrorCode;              /* Error code if any */
  uint32_t ResetCount;             /* Number of consecutive resets */
  uint8_t  IsAppValid;             /* Is the application valid */
  uint8_t  IsRecovery;             /* Is in recovery mode */
  uint32_t LastFlashAddress;       /* Last address flashed */
  uint32_t BytesFlashed;           /* Number of bytes flashed */
} Bootloader_Status_t;

/* Bootloader error codes */
typedef enum {
  BOOTLOADER_ERROR_NONE = 0,
  BOOTLOADER_ERROR_INIT_FAILED,
  BOOTLOADER_ERROR_FLASH_INIT_FAILED,
  BOOTLOADER_ERROR_APP_INVALID,
  BOOTLOADER_ERROR_FLASH_ERASE_FAILED,
  BOOTLOADER_ERROR_FLASH_WRITE_FAILED,
  BOOTLOADER_ERROR_CRC_FAILED,
  BOOTLOADER_ERROR_RECOVERY_FAILED,    // ошибки для неудачного восстановления
  BOOTLOADER_ERROR_INVALID_STATE,      //ошибки для неверного состояния
  BOOTLOADER_ERROR_UNKNOWN
} Bootloader_Error_t;

/* Recovery mode reason enum */
typedef enum {
  RECOVERY_REASON_NONE = 0,
  RECOVERY_REASON_NO_APP,
  RECOVERY_REASON_INVALID_APP,
  RECOVERY_REASON_RESET_COUNT,
  RECOVERY_REASON_FORCED
} Recovery_Reason_t;

/* Exported constants --------------------------------------------------------*/

/* Exported macros -----------------------------------------------------------*/

/* Exported functions --------------------------------------------------------*/

/**
  * @brief  Initializes the bootloader.
  * @param  None
  * @retval None
  */
void Bootloader_Init(void);

/**
  * @brief  Runs the bootloader main process.
  * @param  None
  * @retval None
  */
void Bootloader_Run(void);

/**
  * @brief  Gets the current bootloader status.
  * @param  status: Pointer to store the bootloader status
  * @retval None
  */
void Bootloader_GetStatus(Bootloader_Status_t *status);

/**
  * @brief  Forces entry into recovery mode.
  * @param  None
  * @retval None
  */
void Bootloader_ForceRecovery(void);

/**
  * @brief  Jumps to the application.
  * @param  None
  * @retval None (does not return if successful)
  */
void Bootloader_JumpToApplication(void);

/**
  * @brief  Checks if the application is valid.
  * @param  None
  * @retval 1 if valid, 0 if invalid
  */
uint8_t Bootloader_IsApplicationValid(void);

/**
  * @brief  Handles incoming data for firmware update.
  * @param  data: Pointer to the firmware data
  * @param  size: Size of the data
  * @param  offset: Offset into the firmware where this data should be written
  * @retval 1 if successful, 0 if failed
  */
uint8_t Bootloader_HandleFirmwareData(uint8_t *data, uint32_t size, uint32_t offset);

/**
  * @brief  Finalizes firmware update.
  * @param  None
  * @retval 1 if successful, 0 if failed
  */
uint8_t Bootloader_FinalizeFirmwareUpdate(void);

/**
  * @brief  Checks if backup firmware exists in SPI Flash.
  * @param  None
  * @retval 1 if valid backup firmware exists, 0 if not
  */
uint8_t Bootloader_CheckBackupFirmware(void);
#ifdef __cplusplus
}
#endif

#endif /* __BOOTLOADER_H */
