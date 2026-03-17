/**
 * @file crc.cpp
 * @brief Реализация функций для вычисления CRC32
 */

#include "crc.h"

// Полином CRC32: 0x04C11DB7
#define CRC32_POLYNOMIAL 0xEDB88320 // Отраженный полином для CRC32

/**
 * @brief Вычисляет CRC32 для блока данных
 *
 * @param data Указатель на данные
 * @param length Длина данных в байтах
 * @param initial_crc Начальное значение CRC (для продолжения вычисления)
 * @return uint32_t Результат вычисления CRC32
 */
uint32_t crc32_calculate(const void* data, size_t length, uint32_t initial_crc) {
    const uint8_t* p = (const uint8_t*)data;
    uint32_t crc = ~initial_crc; // Инвертируем начальное значение

    while (length--) {
        crc ^= *p++;
        for (int i = 0; i < 8; i++) {
            crc = (crc >> 1) ^ (CRC32_POLYNOMIAL & -(crc & 1));
        }
    }

    return ~crc; // Инвертируем результат
}

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
