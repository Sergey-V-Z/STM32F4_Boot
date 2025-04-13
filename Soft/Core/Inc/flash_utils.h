/**
  ******************************************************************************
  * @file    flash_utils.h
  * @brief   Header for flash_utils.c file.
  *          This file contains functions for internal Flash operations.
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __FLASH_UTILS_H
#define __FLASH_UTILS_H

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/

/* Bootloader-specific memory locations */
#define BOOTLOADER_START_ADDRESS            0x08000000                  /* Start of bootloader */
#define BOOTLOADER_SIZE                     0x20000                     /* 128 KB for bootloader */
#define MAIN_PROGRAM_START_ADDRESS          (BOOTLOADER_START_ADDRESS + BOOTLOADER_SIZE)
#define BOOTLOADER_KEY_ADDRESS              (BOOTLOADER_START_ADDRESS + BOOTLOADER_SIZE - 0x1000) /* Last 4KB page before main program */
#define BOOTLOADER_KEY_VALUE                0xAAAA5555
#define RESET_COUNTER_ADDRESS               (BOOTLOADER_KEY_ADDRESS + 4)
#define MAX_RESET_COUNT                     3

/* Flash constants */
#define FLASH_PAGE_SIZE                     0x4000      /* 16 KB pages for STM32F407 */
#define FLASH_SECTOR_SIZE                   FLASH_PAGE_SIZE

/* Application metadata structure */
typedef struct {
    uint32_t firmwareSize;     /* Size of firmware in bytes */
    uint32_t firmwareCRC;      /* CRC of firmware */
    uint32_t firmwareVersion;  /* Version number of firmware */
    uint32_t reserved;         /* Reserved for future use */
} AppMetadata_t;

/* Exported macros -----------------------------------------------------------*/
/* Exported functions --------------------------------------------------------*/

/**
  * @brief   Unlocks the Flash for writing.
  * @param   None
  * @retval  HAL status
  */
HAL_StatusTypeDef FLASH_Utils_Unlock(void);

/**
  * @brief   Locks the Flash after writing.
  * @param   None
  * @retval  HAL status
  */
HAL_StatusTypeDef FLASH_Utils_Lock(void);

/**
  * @brief   Erases the specified Flash sector.
  * @param   sectorNumber: Sector number to erase
  * @retval  HAL status
  */
HAL_StatusTypeDef FLASH_Utils_EraseSector(uint32_t sectorNumber);

/**
  * @brief   Writes data to Flash.
  * @param   address: address to write to
  * @param   data: data to write
  * @param   dataSize: size of data in bytes (must be a multiple of 4)
  * @retval  HAL status
  */
HAL_StatusTypeDef FLASH_Utils_Write(uint32_t address, const uint8_t* data, uint32_t dataSize);

/**
  * @brief   Reads data from Flash.
  * @param   address: address to read from
  * @param   data: buffer to store read data
  * @param   dataSize: size of data to read in bytes
  * @retval  HAL status
  */
HAL_StatusTypeDef FLASH_Utils_Read(uint32_t address, uint8_t* data, uint32_t dataSize);

/**
  * @brief   Sets the bootloader key to jump to main application.
  * @param   None
  * @retval  HAL status
  */
HAL_StatusTypeDef FLASH_Utils_SetBootloaderKey(void);

/**
  * @brief   Resets the bootloader key.
  * @param   None
  * @retval  HAL status
  */
HAL_StatusTypeDef FLASH_Utils_ResetBootloaderKey(void);

/**
  * @brief   Reads the bootloader key.
  * @param   None
  * @retval  Key value read from Flash
  */
uint32_t FLASH_Utils_ReadBootloaderKey(void);

/**
  * @brief   Gets the sector number for a given address.
  * @param   address: Flash address
  * @retval  Sector number
  */
uint32_t FLASH_Utils_GetSector(uint32_t address);

/**
  * @brief   Increments the reset counter.
  * @param   None
  * @retval  New value of reset counter
  */
uint32_t FLASH_Utils_IncrementResetCounter(void);

/**
  * @brief   Reads the reset counter.
  * @param   None
  * @retval  Current value of reset counter
  */
uint32_t FLASH_Utils_ReadResetCounter(void);

/**
  * @brief   Resets the reset counter to zero.
  * @param   None
  * @retval  HAL status
  */
HAL_StatusTypeDef FLASH_Utils_ResetResetCounter(void);

/**
  * @brief   Validates firmware at given address
  * @param   startAddress: starting address of firmware 
  * @retval  1 if valid, 0 if invalid
  */
uint8_t FLASH_Utils_ValidateFirmware(uint32_t startAddress);

#ifdef __cplusplus
}
#endif

#endif /* __FLASH_UTILS_H */
