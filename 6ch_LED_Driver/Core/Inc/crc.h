#ifndef CRC_H
#define CRC_H

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stddef.h>

/**
 * @brief Вычисляет CRC32 для блока данных
 *
 * @param data Указатель на данные
 * @param length Длина данных в байтах
 * @param initial_crc Начальное значение CRC (для продолжения вычисления)
 * @return uint32_t Результат вычисления CRC32
 */
uint32_t crc32_calculate(const uint8_t* data, uint32_t size, uint32_t initial_crc);

/**
  * @brief   Computes the 32-bit CRC of a given buffer of data.
  * @param   pBuffer: pointer to the input data buffer
  * @param   bufferSize: size of the input data buffer in bytes
  * @retval  Computed CRC value
  */
uint32_t CRC_Calculate(uint8_t* pBuffer, uint32_t bufferSize);

#endif // CRC_H
