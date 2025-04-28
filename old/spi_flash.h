/**
  ******************************************************************************
  * @file    spi_flash.h
  * @brief   Header for spi_flash.c file.
  *          This file contains functions for external SPI Flash operations.
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __SPI_FLASH_H
#define __SPI_FLASH_H

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include "flash_spi.h" // Включаем существующий драйвер
#include "main.h"
/* Exported types ------------------------------------------------------------*/

/* Exported constants --------------------------------------------------------*/

/* SPI Flash memory layout constants - adjust based on your flash size */
#define SPI_FLASH_CONFIG_ADDRESS        0x00000000  /* Start of configuration area */
#define SPI_FLASH_CONFIG_SIZE           0x00001000  /* 4KB for configuration */
#define SPI_FLASH_MAIN_FW_ADDRESS       0x00001000  /* Start of main firmware area */
#define SPI_FLASH_MAIN_FW_SIZE          0x00080000  /* 512KB for main firmware */
#define SPI_FLASH_BACKUP_FW_ADDRESS     0x00081000  /* Start of backup firmware area */
#define SPI_FLASH_BACKUP_FW_SIZE        0x00080000  /* 512KB for backup firmware */
#define SPI_FLASH_APP_DATA_ADDRESS      0x00101000  /* Start of application data area */
#define SPI_FLASH_APP_DATA_SIZE         0x00080000  /* 512KB for application data */

/* Exported macros -----------------------------------------------------------*/
/* Exported functions --------------------------------------------------------*/

/**
  * @brief  Initializes the SPI Flash interface.
  * @param  None
  * @retval Success (1) or failure (0)
  */
uint8_t SPI_Flash_Init(void);

/**
  * @brief  Gets the SPI Flash memory layout.
  * @param  pLayout: pointer to store the layout information
  * @retval None
  */
void SPI_Flash_GetLayout(SPI_Flash_Layout_t* pLayout);

/**
  * @brief  Reads data from SPI Flash.
  * @param  address: address to read from
  * @param  pData: buffer to store read data
  * @param  dataSize: size of data to read in bytes
  * @retval Success (1) or failure (0)
  */
uint8_t SPI_Flash_Read(uint32_t address, uint8_t* pData, uint32_t dataSize);

/**
  * @brief  Writes data to SPI Flash.
  * @param  address: address to write to
  * @param  pData: data to write
  * @param  dataSize: size of data to write in bytes
  * @retval Success (1) or failure (0)
  */
uint8_t SPI_Flash_Write(uint32_t address, const uint8_t* pData, uint32_t dataSize);

/**
  * @brief  Erases a sector of SPI Flash.
  * @param  sectorAddress: address of the sector to erase
  * @retval Success (1) or failure (0)
  */
uint8_t SPI_Flash_EraseSector(uint32_t sectorAddress);

/**
  * @brief  Erases a block of SPI Flash.
  * @param  blockAddress: address of the block to erase
  * @retval Success (1) or failure (0)
  */
uint8_t SPI_Flash_EraseBlock(uint32_t blockAddress);

/**
  * @brief  Reads firmware metadata from SPI Flash.
  * @param  firmwareArea: 0 for main firmware, 1 for backup firmware
  * @param  pMetadata: pointer to store the metadata
  * @retval Success (1) or failure (0)
  */
uint8_t SPI_Flash_ReadFirmwareMetadata(uint8_t firmwareArea, SPI_Flash_Firmware_Metadata_t* pMetadata);

/**
  * @brief  Writes firmware metadata to SPI Flash.
  * @param  firmwareArea: 0 for main firmware, 1 for backup firmware
  * @param  pMetadata: metadata to write
  * @retval Success (1) or failure (0)
  */
uint8_t SPI_Flash_WriteFirmwareMetadata(uint8_t firmwareArea, const SPI_Flash_Firmware_Metadata_t* pMetadata);

/**
  * @brief  Reads firmware from SPI Flash to a buffer.
  * @param  firmwareArea: 0 for main firmware, 1 for backup firmware
  * @param  pBuffer: buffer to store the firmware
  * @param  bufferSize: size of the buffer
  * @param  pActualSize: pointer to store the actual size read
  * @retval Success (1) or failure (0)
  */
uint8_t SPI_Flash_ReadFirmware(uint8_t firmwareArea, uint8_t* pBuffer, uint32_t bufferSize, uint32_t* pActualSize);

/**
  * @brief  Writes firmware from a buffer to SPI Flash.
  * @param  firmwareArea: 0 for main firmware, 1 for backup firmware
  * @param  pBuffer: buffer containing the firmware
  * @param  firmwareSize: size of the firmware
  * @retval Success (1) or failure (0)
  */
uint8_t SPI_Flash_WriteFirmware(uint8_t firmwareArea, const uint8_t* pBuffer, uint32_t firmwareSize);

/**
  * @brief  Verifies firmware in SPI Flash against internal Flash.
  * @param  firmwareArea: 0 for main firmware, 1 for backup firmware
  * @retval 1 if identical, 0 if different
  */
uint8_t SPI_Flash_VerifyFirmware(uint8_t firmwareArea);

/**
  * @brief  Copies firmware from SPI Flash to internal Flash.
  * @param  firmwareArea: 0 for main firmware, 1 for backup firmware
  * @retval Success (1) or failure (0)
  */
uint8_t SPI_Flash_CopyFirmwareToInternal(uint8_t firmwareArea);

/**
  * @brief  Copies firmware from internal Flash to SPI Flash.
  * @param  firmwareArea: 0 for main firmware, 1 for backup firmware
  * @retval Success (1) or failure (0)
  */
uint8_t SPI_Flash_CopyFirmwareFromInternal(uint8_t firmwareArea);

#ifdef __cplusplus
}
#endif

#endif /* __SPI_FLASH_H */
