/**
  ******************************************************************************
  * @file    flash_utils.c
  * @brief   Internal Flash utilities.
  *          This file provides functions for working with internal Flash memory.
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "flash_utils.h"
#include "crc.h"
#include "string.h"
/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/**
  * @brief   Unlocks the Flash for writing.
  * @param   None
  * @retval  HAL status
  */
HAL_StatusTypeDef FLASH_Utils_Unlock(void)
{
  return HAL_FLASH_Unlock();
}

/**
  * @brief   Locks the Flash after writing.
  * @param   None
  * @retval  HAL status
  */
HAL_StatusTypeDef FLASH_Utils_Lock(void)
{
  return HAL_FLASH_Lock();
}

/**
  * @brief   Gets the sector number for a given address.
  * @param   address: Flash address
  * @retval  Sector number
  */
uint32_t FLASH_Utils_GetSector(uint32_t address)
{
  uint32_t sector = 0;
  
  /* STM32F4xx has different sector sizes */
  if (address < 0x08003FFF) /* 16KB - Sector 0 */
    sector = FLASH_SECTOR_0;
  else if (address < 0x08007FFF) /* 16KB - Sector 1 */
    sector = FLASH_SECTOR_1;
  else if (address < 0x0800BFFF) /* 16KB - Sector 2 */
    sector = FLASH_SECTOR_2;
  else if (address < 0x0800FFFF) /* 16KB - Sector 3 */
    sector = FLASH_SECTOR_3;
  else if (address < 0x0801FFFF) /* 64KB - Sector 4 */
    sector = FLASH_SECTOR_4;
  else if (address < 0x0803FFFF) /* 128KB - Sector 5 */
    sector = FLASH_SECTOR_5;
  else if (address < 0x0805FFFF) /* 128KB - Sector 6 */
    sector = FLASH_SECTOR_6;
  else if (address < 0x0807FFFF) /* 128KB - Sector 7 */
    sector = FLASH_SECTOR_7;
  else if (address < 0x0809FFFF) /* 128KB - Sector 8 */
    sector = FLASH_SECTOR_8;
  else if (address < 0x080BFFFF) /* 128KB - Sector 9 */
    sector = FLASH_SECTOR_9;
  else if (address < 0x080DFFFF) /* 128KB - Sector 10 */
    sector = FLASH_SECTOR_10;
  else if (address < 0x080FFFFF) /* 128KB - Sector 11 */
    sector = FLASH_SECTOR_11;
  
  return sector;
}

/**
  * @brief   Erases the specified Flash sector.
  * @param   sectorNumber: Sector number to erase
  * @retval  HAL status
  */
HAL_StatusTypeDef FLASH_Utils_EraseSector(uint32_t sectorNumber)
{
  FLASH_EraseInitTypeDef eraseInit;
  uint32_t sectorError = 0;
  HAL_StatusTypeDef status;
  
  /* Fill EraseInit structure */
  eraseInit.TypeErase = FLASH_TYPEERASE_SECTORS;
  eraseInit.VoltageRange = FLASH_VOLTAGE_RANGE_3;
  eraseInit.Sector = sectorNumber;
  eraseInit.NbSectors = 1;
  
  status = HAL_FLASHEx_Erase(&eraseInit, &sectorError);
  
  return status;
}

/**
  * @brief   Writes data to Flash.
  * @param   address: address to write to
  * @param   data: data to write
  * @param   dataSize: size of data in bytes (must be a multiple of 4)
  * @retval  HAL status
  */
HAL_StatusTypeDef FLASH_Utils_Write(uint32_t address, const uint8_t* data, uint32_t dataSize)
{
  HAL_StatusTypeDef status = HAL_OK;
  
  /* Check if the data size is a multiple of 4 (word size) */
  if (dataSize % 4 != 0)
    return HAL_ERROR;
  
  /* Write data word by word */
  for (uint32_t i = 0; i < dataSize; i += 4)
  {
    /* Cast to ensure proper alignment */
    uint32_t word = *((uint32_t*)(data + i));
    
    status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address + i, word);
    
    if (status != HAL_OK)
      break;
  }
  
  return status;
}

/**
  * @brief   Reads data from Flash.
  * @param   address: address to read from
  * @param   data: buffer to store read data
  * @param   dataSize: size of data to read in bytes
  * @retval  HAL status
  */
HAL_StatusTypeDef FLASH_Utils_Read(uint32_t address, uint8_t* data, uint32_t dataSize)
{
  /* No specific flash read function is needed, just memory copy */
  memcpy(data, (void*)address, dataSize);
  
  return HAL_OK;
}

/**
  * @brief   Sets the bootloader key to jump to main application.
  * @param   None
  * @retval  HAL status
  */
HAL_StatusTypeDef FLASH_Utils_SetBootloaderKey(void)
{
  HAL_StatusTypeDef status;
  
  status = FLASH_Utils_Unlock();
  if (status != HAL_OK)
    return status;
  
  status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, BOOTLOADER_KEY_ADDRESS, BOOTLOADER_KEY_VALUE);
  
  FLASH_Utils_Lock();
  
  return status;
}

/**
  * @brief   Resets the bootloader key.
  * @param   None
  * @retval  HAL status
  */
HAL_StatusTypeDef FLASH_Utils_ResetBootloaderKey(void)
{
  HAL_StatusTypeDef status;
  uint32_t sector = FLASH_Utils_GetSector(BOOTLOADER_KEY_ADDRESS);
  
  status = FLASH_Utils_Unlock();
  if (status != HAL_OK)
    return status;
  
  /* Erase the sector containing the bootloader key */
  status = FLASH_Utils_EraseSector(sector);
  
  FLASH_Utils_Lock();
  
  return status;
}

/**
  * @brief   Reads the bootloader key.
  * @param   None
  * @retval  Key value read from Flash
  */
uint32_t FLASH_Utils_ReadBootloaderKey(void)
{
  return *((uint32_t*)BOOTLOADER_KEY_ADDRESS);
}

/**
  * @brief   Increments the reset counter.
  * @param   None
  * @retval  New value of reset counter
  */
uint32_t FLASH_Utils_IncrementResetCounter(void)
{
  uint32_t currentCount = FLASH_Utils_ReadResetCounter();
  uint32_t newCount = currentCount + 1;
  HAL_StatusTypeDef status;
  
  status = FLASH_Utils_Unlock();
  if (status != HAL_OK)
    return currentCount; /* Return old value on error */
  
  /* Program the new counter value */
  status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, RESET_COUNTER_ADDRESS, newCount);
  
  FLASH_Utils_Lock();
  
  return (status == HAL_OK) ? newCount : currentCount;
}

/**
  * @brief   Reads the reset counter.
  * @param   None
  * @retval  Current value of reset counter
  */
uint32_t FLASH_Utils_ReadResetCounter(void)
{
  uint32_t counterValue = *((uint32_t*)RESET_COUNTER_ADDRESS);
  
  /* If counter location is erased (0xFFFFFFFF), return 0 */
  if (counterValue == 0xFFFFFFFF)
    return 0;
  
  return counterValue;
}

/**
  * @brief   Resets the reset counter to zero.
  * @param   None
  * @retval  HAL status
  */
HAL_StatusTypeDef FLASH_Utils_ResetResetCounter(void)
{
  HAL_StatusTypeDef status;
  
  status = FLASH_Utils_Unlock();
  if (status != HAL_OK)
    return status;
  
  /* Program the counter with value 0 */
  status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, RESET_COUNTER_ADDRESS, 0);
  
  FLASH_Utils_Lock();
  
  return status;
}

/**
  * @brief   Validates firmware at given address
  * @param   startAddress: starting address of firmware
  * @retval  1 if valid, 0 if invalid
  */
uint8_t FLASH_Utils_ValidateFirmware(uint32_t startAddress)
{
  /* Read metadata structure located at the beginning of the firmware */
  AppMetadata_t metadata;
  FLASH_Utils_Read(startAddress, (uint8_t*)&metadata, sizeof(AppMetadata_t));
  
  /* Check firmware size is reasonable */
  if (metadata.firmwareSize == 0xFFFFFFFF || metadata.firmwareSize > (1024 * 1024)) /* Max 1MB */
    return 0;
  
  /* Calculate CRC of the firmware excluding the metadata */
  uint32_t calculatedCRC = CRC_Calculate((uint8_t*)(startAddress + sizeof(AppMetadata_t)), 
                                         metadata.firmwareSize);
  
  /* Compare calculated CRC with stored CRC */
  if (calculatedCRC != metadata.firmwareCRC)
    return 0;
  
  /* Validate stack pointer and reset handler */
  uint32_t* vectorTable = (uint32_t*)startAddress;
  uint32_t stackPointer = vectorTable[0];
  uint32_t resetHandler = vectorTable[1];
  
  /* Check if stack pointer is in RAM range */
  if (stackPointer < 0x20000000 || stackPointer > 0x20020000) /* 128KB RAM */
    return 0;
  
  /* Check if reset handler is in Flash range */
  if (resetHandler < MAIN_PROGRAM_START_ADDRESS || resetHandler > 0x08100000) /* 1MB Flash */
    return 0;
  
  return 1; /* Firmware is valid */
}
