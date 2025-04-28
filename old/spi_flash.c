/**
  ******************************************************************************
  * @file    spi_flash.c
  * @brief   SPI Flash operations.
  *          This file provides functions for working with external SPI Flash.
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "spi_flash.h"
#include "crc.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
//static SPI_Flash_Layout_t flashLayout;
//static uint8_t flashInitialized = 0;

/* Private function prototypes -----------------------------------------------*/
static uint32_t SPI_Flash_GetFirmwareAddress(uint8_t firmwareArea);
static uint32_t SPI_Flash_GetFirmwareSize(uint8_t firmwareArea);

/* Functions -----------------------------------------------------------------*/

/**
  * @brief  Initializes the SPI Flash interface.
  * @param  None
  * @retval Success (1) or failure (0)
  */
uint8_t SPI_Flash_Init(void) {
	// Инициализируем структуру макета памяти
	flashLayout.ConfigAreaAddress = SPI_FLASH_CONFIG_ADDRESS;
	flashLayout.ConfigAreaSize = SPI_FLASH_CONFIG_SIZE;
	flashLayout.MainFirmwareAddress = SPI_FLASH_MAIN_FW_ADDRESS;
	flashLayout.MainFirmwareSize = SPI_FLASH_MAIN_FW_SIZE;
	flashLayout.BackupFirmwareAddress = SPI_FLASH_BACKUP_FW_ADDRESS;
	flashLayout.BackupFirmwareSize = SPI_FLASH_BACKUP_FW_SIZE;
	flashLayout.AppDataAddress = SPI_FLASH_APP_DATA_ADDRESS;
	flashLayout.AppDataSize = SPI_FLASH_APP_DATA_SIZE;

	// Определяем параметры пинов SPI (см. main.h для определений)
	pins_spi_t ChipSelect = { SPI3_CS_GPIO_Port, SPI3_CS_Pin };
	pins_spi_t WriteProtect = { WP_GPIO_Port, WP_Pin };
	pins_spi_t Hold = { HOLD_GPIO_Port, HOLD_Pin };

	// Инициализация устройства
	W25QXX_InitDevice(&w25qxx_dev);
	if (!W25QXX_Init(&w25qxx_dev, &hspi3, 0, ChipSelect, WriteProtect, Hold,
			0)) {
		// Ошибка инициализации
		Error_Handler();
	}
	flashInitialized = 1;
	return 1;
}

/**
  * @brief  Gets the SPI Flash memory layout.
  * @param  pLayout: pointer to store the layout information
  * @retval None
  */
void SPI_Flash_GetLayout(SPI_Flash_Layout_t* pLayout)
{
  if (pLayout != NULL)
  {
    *pLayout = flashLayout;
  }
}

/**
  * @brief  Reads data from SPI Flash.
  * @param  address: address to read from
  * @param  pData: buffer to store read data
  * @param  dataSize: size of data to read in bytes
  * @retval Success (1) or failure (0)
  */
uint8_t SPI_Flash_Read(uint32_t address, uint8_t* pData, uint32_t dataSize)
{
  if (!flashInitialized)
    return 0;
  
  // Используем функцию чтения из существующего драйвера
  //mem_spi.w25qxx_dev_ReadBytes(pData, address, dataSize);
  W25QXX_ReadBytes(&w25qxx_dev, pData, address, dataSize);
  
  return 1;
}

/**
  * @brief  Writes data to SPI Flash.
  * @param  address: address to write to
  * @param  pData: data to write
  * @param  dataSize: size of data to write in bytes
  * @retval Success (1) or failure (0)
  */
uint8_t SPI_Flash_Write(uint32_t address, const uint8_t* pData, uint32_t dataSize)
{
  if (!flashInitialized)
    return 0;
  
  // Определяем, в какой именно раздел памяти мы пишем
  uint32_t page = address / w25qxx_dev.Info.PageSize;
  uint32_t offset = address % w25qxx_dev.Info.PageSize;
  
  // Если размер данных меньше размера страницы, используем запись страницы
  if (dataSize <= w25qxx_dev.Info.PageSize)
  {
    W25QXX_WritePage(&w25qxx_dev, (uint8_t*)pData, page, offset, dataSize);
  }
  // Если размер данных меньше размера сектора, используем запись сектора
  else if (dataSize <= w25qxx_dev.Info.SectorSize)
  {
    uint32_t sector = address / w25qxx_dev.Info.SectorSize;
    uint32_t sectorOffset = address % w25qxx_dev.Info.SectorSize;
    W25QXX_WriteSector(&w25qxx_dev, (uint8_t*)pData, sector, sectorOffset, dataSize);
  }
  // Если данные больше сектора, используем запись блока
  else
  {
    uint32_t block = address / w25qxx_dev.Info.BlockSize;
    uint32_t blockOffset = address % w25qxx_dev.Info.BlockSize;
    W25QXX_WriteBlock(&w25qxx_dev, (uint8_t*)pData, block, blockOffset, dataSize);
  }
  
  return 1;
}

/**
  * @brief  Erases a sector of SPI Flash.
  * @param  sectorAddress: address of the sector to erase
  * @retval Success (1) or failure (0)
  */
uint8_t SPI_Flash_EraseSector(uint32_t sectorAddress)
{
  if (!flashInitialized)
    return 0;
  
  // Вычисляем номер сектора
  uint32_t sector = sectorAddress / w25qxx_dev.Info.SectorSize;
  
  // Стираем сектор
  W25QXX_EraseSector(&w25qxx_dev, sector);
  
  return 1;
}
/**
  * @brief  Erases a block of SPI Flash.
  * @param  blockAddress: address of the block to erase
  * @retval Success (1) or failure (0)
  */
uint8_t SPI_Flash_EraseBlock(uint32_t blockAddress)
{
  if (!flashInitialized)
    return 0;
  
  // Вычисляем номер блока
  uint32_t block = blockAddress / w25qxx_dev.Info.BlockSize;
  
  // Стираем блок
  W25QXX_EraseBlock(&w25qxx_dev, block);
  
  return 1;
}

/**
  * @brief  Gets the address of firmware area.
  * @param  firmwareArea: 0 for main firmware, 1 for backup firmware
  * @retval Address of firmware area
  */
static uint32_t SPI_Flash_GetFirmwareAddress(uint8_t firmwareArea)
{
  if (firmwareArea == 0)
    return flashLayout.MainFirmwareAddress;
  else
    return flashLayout.BackupFirmwareAddress;
}

/**
  * @brief  Gets the size of firmware area.
  * @param  firmwareArea: 0 for main firmware, 1 for backup firmware
  * @retval Size of firmware area
  */
static uint32_t SPI_Flash_GetFirmwareSize(uint8_t firmwareArea)
{
  if (firmwareArea == 0)
    return flashLayout.MainFirmwareSize;
  else
    return flashLayout.BackupFirmwareSize;
}

/**
  * @brief  Reads firmware metadata from SPI Flash.
  * @param  firmwareArea: 0 for main firmware, 1 for backup firmware
  * @param  pMetadata: pointer to store the metadata
  * @retval Success (1) or failure (0)
  */
uint8_t SPI_Flash_ReadFirmwareMetadata(uint8_t firmwareArea, SPI_Flash_Firmware_Metadata_t* pMetadata)
{
  if (!flashInitialized || pMetadata == NULL)
    return 0;
  
  uint32_t address = SPI_Flash_GetFirmwareAddress(firmwareArea);
  
  // Читаем метаданные из начала области прошивки
  return SPI_Flash_Read(address, (uint8_t*)pMetadata, sizeof(SPI_Flash_Firmware_Metadata_t));
}

/**
  * @brief  Writes firmware metadata to SPI Flash.
  * @param  firmwareArea: 0 for main firmware, 1 for backup firmware
  * @param  pMetadata: metadata to write
  * @retval Success (1) or failure (0)
  */
uint8_t SPI_Flash_WriteFirmwareMetadata(uint8_t firmwareArea, const SPI_Flash_Firmware_Metadata_t* pMetadata)
{
  if (!flashInitialized || pMetadata == NULL)
    return 0;
  
  uint32_t address = SPI_Flash_GetFirmwareAddress(firmwareArea);
  
  // Записываем метаданные в начало области прошивки
  return SPI_Flash_Write(address, (const uint8_t*)pMetadata, sizeof(SPI_Flash_Firmware_Metadata_t));
}

/**
  * @brief  Reads firmware from SPI Flash to a buffer.
  * @param  firmwareArea: 0 for main firmware, 1 for backup firmware
  * @param  pBuffer: buffer to store the firmware
  * @param  bufferSize: size of the buffer
  * @param  pActualSize: pointer to store the actual size read
  * @retval Success (1) or failure (0)
  */
uint8_t SPI_Flash_ReadFirmware(uint8_t firmwareArea, uint8_t* pBuffer, uint32_t bufferSize, uint32_t* pActualSize)
{
  if (!flashInitialized || pBuffer == NULL)
    return 0;
  
  SPI_Flash_Firmware_Metadata_t metadata;
  
  // Сначала читаем метаданные
  if (!SPI_Flash_ReadFirmwareMetadata(firmwareArea, &metadata))
    return 0;
  
  // Проверяем, достаточно ли размера буфера для прошивки
  if (metadata.FirmwareSize > bufferSize)
  {
    if (pActualSize != NULL)
      *pActualSize = 0;
    return 0;
  }
  
  uint32_t address = SPI_Flash_GetFirmwareAddress(firmwareArea) + sizeof(SPI_Flash_Firmware_Metadata_t);
  
  // Читаем прошивку после метаданных
  if (!SPI_Flash_Read(address, pBuffer, metadata.FirmwareSize))
    return 0;
  
  if (pActualSize != NULL)
    *pActualSize = metadata.FirmwareSize;
  
  return 1;
}

/**
  * @brief  Writes firmware from a buffer to SPI Flash.
  * @param  firmwareArea: 0 for main firmware, 1 for backup firmware
  * @param  pBuffer: buffer containing the firmware
  * @param  firmwareSize: size of the firmware
  * @retval Success (1) or failure (0)
  */
uint8_t SPI_Flash_WriteFirmware(uint8_t firmwareArea, const uint8_t* pBuffer, uint32_t firmwareSize)
{
  if (!flashInitialized || pBuffer == NULL || firmwareSize == 0)
    return 0;
  
  uint32_t address = SPI_Flash_GetFirmwareAddress(firmwareArea);
  uint32_t maxSize = SPI_Flash_GetFirmwareSize(firmwareArea);
  
  // Проверяем, не превышает ли размер прошивки максимальный размер области
  if (firmwareSize + sizeof(SPI_Flash_Firmware_Metadata_t) > maxSize)
    return 0;
  
  // Создаем и заполняем метаданные
  SPI_Flash_Firmware_Metadata_t metadata;
  metadata.FirmwareSize = firmwareSize;
  metadata.FirmwareCRC = CRC_Calculate((uint8_t*)pBuffer, firmwareSize);
  metadata.FirmwareVersion = 0x00010000; // Версия 1.0.0.0
  metadata.TargetAddress = MAIN_PROGRAM_START_ADDRESS; // Используем определение из flash_utils.h
  
  // Стираем блоки для прошивки - вычисляем количество блоков
  uint32_t numBlocks = (firmwareSize + sizeof(SPI_Flash_Firmware_Metadata_t) + w25qxx_dev.Info.BlockSize - 1) / w25qxx_dev.Info.BlockSize;
  
  for (uint32_t i = 0; i < numBlocks; i++)
  {
    SPI_Flash_EraseBlock(address + i * w25qxx_dev.Info.BlockSize);
  }
  
  // Записываем метаданные
  if (!SPI_Flash_Write(address, (const uint8_t*)&metadata, sizeof(SPI_Flash_Firmware_Metadata_t)))
    return 0;
  
  // Записываем прошивку
  if (!SPI_Flash_Write(address + sizeof(SPI_Flash_Firmware_Metadata_t), pBuffer, firmwareSize))
    return 0;
  
  return 1;
}

/**
  * @brief  Verifies firmware in SPI Flash against internal Flash.
  * @param  firmwareArea: 0 for main firmware, 1 for backup firmware
  * @retval 1 if identical, 0 if different
  */
uint8_t SPI_Flash_VerifyFirmware(uint8_t firmwareArea)
{
  if (!flashInitialized)
    return 0;
  
  SPI_Flash_Firmware_Metadata_t metadata;
  
  // Читаем метаданные
  if (!SPI_Flash_ReadFirmwareMetadata(firmwareArea, &metadata))
    return 0;
  
  // Буфер для временного хранения блока данных для сравнения
  // Используем буфер размером в страницу для оптимального чтения
  uint8_t tempBuffer[256];
  uint32_t bytesCompared = 0;
  uint32_t address = SPI_Flash_GetFirmwareAddress(firmwareArea) + sizeof(SPI_Flash_Firmware_Metadata_t);
  
  // Сравниваем прошивку блоками
  while (bytesCompared < metadata.FirmwareSize)
  {
    uint32_t bytesToCompare = (metadata.FirmwareSize - bytesCompared > 256) ? 256 : (metadata.FirmwareSize - bytesCompared);
    
    // Читаем блок из SPI Flash
    if (!SPI_Flash_Read(address + bytesCompared, tempBuffer, bytesToCompare))
      return 0;
    
    // Сравниваем с внутренней Flash
    for (uint32_t i = 0; i < bytesToCompare; i++)
    {
      if (tempBuffer[i] != *((uint8_t*)(metadata.TargetAddress + bytesCompared + i)))
        return 0;
    }
    
    bytesCompared += bytesToCompare;
  }
  
  return 1;
}

/**
  * @brief  Copies firmware from SPI Flash to internal Flash.
  * @param  firmwareArea: 0 for main firmware, 1 for backup firmware
  * @retval Success (1) or failure (0)
  */
uint8_t SPI_Flash_CopyFirmwareToInternal(uint8_t firmwareArea)
{
  if (!flashInitialized)
    return 0;
  
  SPI_Flash_Firmware_Metadata_t metadata;
  
  // Читаем метаданные
  if (!SPI_Flash_ReadFirmwareMetadata(firmwareArea, &metadata))
    return 0;
  
  // Проверяем CRC прошивки перед копированием
  uint8_t tempBuffer[512]; // Буфер для чтения прошивки
  uint32_t bytesRead = 0;
  uint32_t address = SPI_Flash_GetFirmwareAddress(firmwareArea) + sizeof(SPI_Flash_Firmware_Metadata_t);
  uint32_t calculatedCRC = 0;
  
  // Инициализируем CRC модуль
  __HAL_RCC_CRC_CLK_ENABLE();
  CRC->CR = CRC_CR_RESET;
  
  // Читаем и считаем CRC прошивки блоками
  while (bytesRead < metadata.FirmwareSize)
  {
    uint32_t bytesToRead = (metadata.FirmwareSize - bytesRead > 512) ? 512 : (metadata.FirmwareSize - bytesRead);
    
    // Читаем блок из SPI Flash
    if (!SPI_Flash_Read(address + bytesRead, tempBuffer, bytesToRead))
      return 0;
    
    // Обновляем CRC
    for (uint32_t i = 0; i < bytesToRead / 4; i++)
    {
      CRC->DR = ((uint32_t*)tempBuffer)[i];
    }
    
    bytesRead += bytesToRead;
  }
  
  // Получаем итоговый CRC
  calculatedCRC = CRC->DR;
  
  // Проверяем CRC
  if (calculatedCRC != metadata.FirmwareCRC)
    return 0;
  
  // Стираем необходимые сектора во внутренней Flash
  FLASH_Utils_Unlock();
  
  // Определяем сектора для стирания
  uint32_t startSector = FLASH_Utils_GetSector(metadata.TargetAddress);
  uint32_t endSector = FLASH_Utils_GetSector(metadata.TargetAddress + metadata.FirmwareSize - 1);
  
  for (uint32_t sector = startSector; sector <= endSector; sector++)
  {
    if (FLASH_Utils_EraseSector(sector) != HAL_OK)
    {
      FLASH_Utils_Lock();
      return 0;
    }
  }
  
  // Копируем прошивку во внутреннюю Flash
  bytesRead = 0;
  
  while (bytesRead < metadata.FirmwareSize)
  {
    uint32_t bytesToRead = (metadata.FirmwareSize - bytesRead > 512) ? 512 : (metadata.FirmwareSize - bytesRead);
    
    // Читаем блок из SPI Flash
    if (!SPI_Flash_Read(address + bytesRead, tempBuffer, bytesToRead))
    {
      FLASH_Utils_Lock();
      return 0;
    }
    
    // Записываем во внутреннюю Flash с выравниванием по 4 байта
    for (uint32_t i = 0; i < (bytesToRead / 4) * 4; i += 4)
    {
      uint32_t dataWord = *(uint32_t*)&tempBuffer[i];
      
      if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, metadata.TargetAddress + bytesRead + i, dataWord) != HAL_OK)
      {
        FLASH_Utils_Lock();
        return 0;
      }
    }

    // Обрабатываем оставшиеся байты, если есть
    if (bytesToRead % 4 != 0)
    {
      uint32_t dataWord = 0xFFFFFFFF; // Начальное значение для стертой флеш
      uint8_t remainBytes = bytesToRead % 4;

      for (uint8_t i = 0; i < remainBytes; i++)
      {
        ((uint8_t*)&dataWord)[i] = tempBuffer[(bytesToRead / 4) * 4 + i];
      }

      if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, metadata.TargetAddress + bytesRead + (bytesToRead / 4) * 4, dataWord) != HAL_OK)
      {
        FLASH_Utils_Lock();
        return 0;
      }
    }
    
    bytesRead += bytesToRead;
  }
  
  FLASH_Utils_Lock();
  
  // Верифицируем записанную прошивку
  return SPI_Flash_VerifyFirmware(firmwareArea);
}

/**
  * @brief  Copies firmware from internal Flash to SPI Flash.
  * @param  firmwareArea: 0 for main firmware, 1 for backup firmware
  * @retval Success (1) or failure (0)
  */
uint8_t SPI_Flash_CopyFirmwareFromInternal(uint8_t firmwareArea)
{
  if (!flashInitialized)
    return 0;
  
  // Определяем размер прошивки на основе образа во внутренней Flash
  // Для примера, мы будем использовать фиксированный размер
  uint32_t firmwareSize = 0x20000; // 128KB (это пример, нужно реализовать правильное определение размера)
  
  // Буфер для чтения блоков прошивки
  uint8_t tempBuffer[512];
  uint32_t bytesCopied = 0;
  
  // Создаем метаданные
  SPI_Flash_Firmware_Metadata_t metadata;
  metadata.FirmwareSize = firmwareSize;
  metadata.TargetAddress = MAIN_PROGRAM_START_ADDRESS;
  metadata.FirmwareVersion = 0x00010000; // Версия 1.0.0.0
  
  // Вычисляем CRC прошивки
  metadata.FirmwareCRC = CRC_Calculate((uint8_t*)MAIN_PROGRAM_START_ADDRESS, firmwareSize);
  
  // Стираем область во внешней Flash
  uint32_t address = SPI_Flash_GetFirmwareAddress(firmwareArea);
  uint32_t numBlocks = (firmwareSize + sizeof(SPI_Flash_Firmware_Metadata_t) + w25qxx_dev.Info.BlockSize - 1) / w25qxx_dev.Info.BlockSize;
  
  for (uint32_t i = 0; i < numBlocks; i++)
  {
    SPI_Flash_EraseBlock(address + i * w25qxx_dev.Info.BlockSize);
  }
  
  // Записываем метаданные
  if (!SPI_Flash_Write(address, (const uint8_t*)&metadata, sizeof(SPI_Flash_Firmware_Metadata_t)))
    return 0;
  
  // Копируем прошивку блоками
  while (bytesCopied < firmwareSize)
  {
    uint32_t bytesToCopy = (firmwareSize - bytesCopied > 512) ? 512 : (firmwareSize - bytesCopied);
    
    // Копируем данные из внутренней Flash во временный буфер
    memcpy(tempBuffer, (uint8_t*)(MAIN_PROGRAM_START_ADDRESS + bytesCopied), bytesToCopy);
    
    // Записываем во внешнюю Flash
    if (!SPI_Flash_Write(address + sizeof(SPI_Flash_Firmware_Metadata_t) + bytesCopied, tempBuffer, bytesToCopy))
      return 0;
    
    bytesCopied += bytesToCopy;
  }
  
  return 1;
}
