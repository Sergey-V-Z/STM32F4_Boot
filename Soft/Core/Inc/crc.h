/**
  ******************************************************************************
  * @file    crc.h
  * @brief   Header for crc.c file.
  *          This file contains the functions for CRC calculation.
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __CRC_H
#define __CRC_H

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include "flash_spi.h"

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Exported macros -----------------------------------------------------------*/
/* Exported functions --------------------------------------------------------*/

/**
  * @brief   Computes the 32-bit CRC of a given buffer of data.
  * @param   pBuffer: pointer to the input data buffer
  * @param   bufferSize: size of the input data buffer in bytes
  * @retval  Computed CRC value
  */
uint32_t CRC_Calculate(uint8_t* pBuffer, uint32_t bufferSize);

/**
  * @brief   Validates firmware using CRC.
  * @param   startAddress: starting address of the firmware in memory
  * @param   firmwareSize: size of the firmware in bytes
  * @param   expectedCRC: expected CRC value
  * @retval  1 if CRC is valid, 0 otherwise
  */
uint8_t CRC_ValidateFirmware(uint32_t startAddress, uint32_t firmwareSize, uint32_t expectedCRC);

/**
  * @brief   Calculate CRC32 and append it to firmware.
  * @param   pBuffer: pointer to the firmware buffer
  * @param   bufferSize: size of the firmware buffer
  * @retval  CRC32 value calculated
  */
uint32_t CRC_CalculateAndAppend(uint8_t* pBuffer, uint32_t bufferSize);

/**
  * @brief   Рассчитывает CRC32 для структуры метаданных
  * @param   pMetadata: указатель на структуру метаданных
  * @param   metadataSize: размер структуры метаданных (без учета поля CRC)
  * @retval  Вычисленное значение CRC
  */
uint32_t CRC_CalculateMetadata(void* pMetadata, uint32_t metadataSize);

/**
  * @brief   Проверяет CRC на указанном блоке данных в SPI Flash
  * @param   device: указатель на устройство SPI Flash
  * @param   address: адрес начала блока в SPI Flash
  * @param   size: размер блока данных
  * @param   expectedCRC: ожидаемое значение CRC
  * @retval  1 если CRC совпадает, 0 в противном случае
  */
uint8_t CRC_ValidateExternalData(W25QXX_Device_t* device, uint32_t address, uint32_t size, uint32_t expectedCRC);

#ifdef __cplusplus
}
#endif

#endif /* __CRC_H */
