/**
  ******************************************************************************
  * @file    crc.c
  * @brief   CRC calculation functions.
  *          This file provides functions to calculate CRC for firmware validation.
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "crc.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/**
  * @brief   Computes the 32-bit CRC of a given buffer of data.
  * @param   pBuffer: pointer to the input data buffer
  * @param   bufferSize: size of the input data buffer in bytes
  * @retval  Computed CRC value
  */
uint32_t CRC_Calculate(uint8_t* pBuffer, uint32_t bufferSize)
{
  uint32_t crc = 0xFFFFFFFF; // CRC32 initial value
  
  // Enable CRC clock
  __HAL_RCC_CRC_CLK_ENABLE();
  
  // Reset CRC peripheral
  CRC->CR = CRC_CR_RESET;
  
  // Process all data by blocks of 4 bytes (32 bits)
  for (uint32_t i = 0; i < (bufferSize / 4); i++)
  {
    CRC->DR = ((uint32_t*)pBuffer)[i];
  }
  
  // Process remaining bytes if buffer size is not a multiple of 4
  uint32_t remainingBytes = bufferSize % 4;
  if (remainingBytes > 0)
  {
    uint32_t data = 0;
    uint8_t* pByte = pBuffer + (bufferSize - remainingBytes);
    
    for (uint32_t i = 0; i < remainingBytes; i++)
    {
      data |= (uint32_t)(*pByte++) << (i * 8);
    }
    
    CRC->DR = data;
  }
  
  crc = CRC->DR;
  
  // Disable CRC clock to save power
  __HAL_RCC_CRC_CLK_DISABLE();
  
  return crc;
}

/**
  * @brief   Validates firmware using CRC.
  * @param   startAddress: starting address of the firmware in memory
  * @param   firmwareSize: size of the firmware in bytes
  * @param   expectedCRC: expected CRC value
  * @retval  1 if CRC is valid, 0 otherwise
  */
uint8_t CRC_ValidateFirmware(uint32_t startAddress, uint32_t firmwareSize, uint32_t expectedCRC)
{
  uint32_t calculatedCRC;
  
  // Calculate CRC for the firmware
  calculatedCRC = CRC_Calculate((uint8_t*)startAddress, firmwareSize);
  
  // Compare with expected CRC
  if (calculatedCRC == expectedCRC)
  {
    return 1; // CRC is valid
  }
  else
  {
    return 0; // CRC is invalid
  }
}

/**
  * @brief   Calculate CRC32 and append it to firmware.
  * @param   pBuffer: pointer to the firmware buffer
  * @param   bufferSize: size of the firmware buffer
  * @retval  CRC32 value calculated
  * @note    This function assumes that there are 4 bytes available after bufferSize in pBuffer
  */
uint32_t CRC_CalculateAndAppend(uint8_t* pBuffer, uint32_t bufferSize)
{
  uint32_t crc;
  
  // Calculate CRC
  crc = CRC_Calculate(pBuffer, bufferSize);
  
  // Append CRC to the buffer
  *((uint32_t*)(pBuffer + bufferSize)) = crc;
  
  return crc;
}
