/**
  ******************************************************************************
  * @file    flash_spi.h
  * @brief   Header for flash_spi.c file.
  *          This file contains SPI Flash driver functions.
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __FLASH_SPI_H
#define __FLASH_SPI_H

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Exported types ------------------------------------------------------------*/

/* SPI Pin structure */
typedef struct
{
  GPIO_TypeDef* GPIO_Port;
  uint16_t GPIO_Pin;
} pins_spi_t;

/* W25Qxx Flash ID */
typedef enum {
  W25Q10_ID = 1,
  W25Q20_ID,
  W25Q40_ID,
  W25Q80_ID,
  W25Q16_ID,
  W25Q32_ID,
  W25Q64_ID,
  W25Q128_ID,
  W25Q256_ID,
  W25Q512_ID
} W25QXX_ID_t;

/* W25Qxx information structure */
typedef struct {
  W25QXX_ID_t ID;
  uint8_t     UniqID[8];
  uint16_t    PageSize;
  uint32_t    PageCount;
  uint32_t    SectorSize;
  uint32_t    SectorCount;
  uint32_t    BlockSize;
  uint32_t    BlockCount;
  uint32_t    CapacityInKiloByte;
  uint8_t     StatusRegister1;
  uint8_t     StatusRegister2;
  uint8_t     StatusRegister3;
  uint8_t     Lock;
} W25QXX_Info_t;

/* Settings structure */
typedef struct {
    // Network settings
    uint8_t MAC[6];
    struct {
        uint8_t ip[4];
        uint8_t mask[4];
        uint8_t gateway[4];
    } saveIP;
    uint8_t DHCPset;

    // Adding other fields from Основные настройки.txt
    uint32_t Direct;                // направление вращения
    uint32_t mod_rotation;          // режим вращения
    uint32_t res2;                  // тип мотора
    uint32_t Speed;                 // скорость
    uint32_t StartSpeed;            // начальная скорость
    uint32_t Accel;                 // ускорение шагов в милисекунду
    uint32_t Slowdown;              // торможение шагов
    uint32_t res1;
    uint32_t res;

    // Параметры энкодера
    uint32_t stepsENC;              // шаги энкодера между датчиками
    uint32_t stepsENCtoOneStepMotor; // шаги энкодера на шаг мотора

    // Временные параметры
    uint32_t TimeOut;               // таймаут при отсутствии движения

    // Конфигурация датчиков
    struct {
        uint8_t CW_sensor;
        uint8_t CCW_sensor;
        uint8_t detected;
    } sensors_map;

    // Промежуточные позиции
    uint32_t intermediate_positions[3]; // позиции промежуточных остановок
    uint8_t use_intermediate;          // флаг использования промежуточных остановок

    struct {
        uint32_t points[10];       // массив точек
        uint32_t count;            // количество точек
        uint32_t target_point;     // целевая точка
        uint32_t current_point;    // текущая точка
        uint8_t is_calibrated;     // флаг калибровки
    } points;

    // Системные параметры
    uint8_t version;               // версия конфигурации
} Settings_t;

/* W25Q SPI Flash device instance */
typedef struct {
    SPI_HandleTypeDef *hspi;       // SPI handle
    pins_spi_t ChipSelect;         // CS pin
    pins_spi_t WriteProtect;       // WP pin
    pins_spi_t Hold;               // HOLD pin
    uint8_t UsedInOS;              // Flag for OS usage
    W25QXX_Info_t Info;            // Flash information
    HAL_StatusTypeDef lastStatus;  // Last operation status
} W25QXX_Device_t;

/* Exported constants --------------------------------------------------------*/
/* W25Q Commands */
#define W25_WRITE_DISABLE           0x04
#define W25_WRITE_ENABLE            0x06
#define W25_CHIP_ERASE              0xC7 // Alternative: 0x60
#define W25_SECTOR_ERASE            0x20
#define W25_BLOCK_ERASE             0xD8
#define W25_FAST_READ               0x0B
#define W25_PAGE_PROGRAMM           0x02
#define W25_GET_JEDEC_ID            0x9F
#define W25_READ_STATUS_1           0x05
#define W25_READ_STATUS_2           0x35
#define W25_READ_STATUS_3           0x15
#define W25_WRITE_STATUS_1          0x01
#define W25_WRITE_STATUS_2          0x31
#define W25_WRITE_STATUS_3          0x11
#define W25_READ_UNIQUE_ID          0x4B

#define W25QXX_DUMMY_BYTE           0xA5

/* Debug settings */
#define INIT_DEBUG                  0

/* Define for IO operations */
#define W25QFLASH_CS_SELECT(dev)    HAL_GPIO_WritePin((dev)->ChipSelect.GPIO_Port, (dev)->ChipSelect.GPIO_Pin, GPIO_PIN_RESET)
#define W25QFLASH_CS_UNSELECT(dev)  HAL_GPIO_WritePin((dev)->ChipSelect.GPIO_Port, (dev)->ChipSelect.GPIO_Pin, GPIO_PIN_SET)

/* Exported functions --------------------------------------------------------*/

/**
  * @brief  Initialize device instance
  * @param  device: Pointer to device instance
  * @retval None
  */
void W25QXX_InitDevice(W25QXX_Device_t *device);

/**
  * @brief  Initialize the SPI Flash.
  * @param  device: Pointer to device instance
  * @param  hspi: SPI handle pointer
  * @param  startAddr: Start address in Flash
  * @param  ChipSelect: Chip select pins
  * @param  WriteProtect: Write protect pins
  * @param  Hold: Hold pins
  * @param  UsedInOS: Flag indicating if used in OS
  * @retval 1 if successful, 0 if failed
  */
uint8_t W25QXX_Init(W25QXX_Device_t *device, SPI_HandleTypeDef *hspi, uint32_t startAddr, pins_spi_t ChipSelect, pins_spi_t WriteProtect, pins_spi_t Hold, uint8_t UsedInOS);

/**
  * @brief  Set the OS usage flag.
  * @param  device: Pointer to device instance
  * @param  UsedInOS: Flag indicating if used in OS
  * @retval None
  */
void W25QXX_SetUsedInOS(W25QXX_Device_t *device, uint8_t UsedInOS);

/**
  * @brief  Reads settings from SPI Flash.
  * @param  device: Pointer to device instance
  * @param  settings: Pointer to settings structure
  * @retval None
  */
void W25QXX_ReadSettings(W25QXX_Device_t *device, Settings_t *settings);

/**
  * @brief  Writes settings to SPI Flash.
  * @param  device: Pointer to device instance
  * @param  settings: Settings structure to write
  * @retval None
  */
void W25QXX_WriteSettings(W25QXX_Device_t *device, Settings_t settings);

/**
  * @brief  Delay for the device
  * @param  device: Pointer to device instance
  * @param  delay: Delay in ms
  * @retval None
  */
void W25QXX_Delay(W25QXX_Device_t *device, uint32_t delay);

/**
  * @brief  Transmit/Receive byte over SPI
  * @param  device: Pointer to device instance
  * @param  Data: Data to transmit
  * @retval Received data
  */
uint8_t W25QXX_SPI(W25QXX_Device_t *device, uint8_t Data);

/**
  * @brief  Read device ID
  * @param  device: Pointer to device instance
  * @retval Device ID
  */
uint32_t W25QXX_ReadID(W25QXX_Device_t *device);

/**
  * @brief  Read unique ID
  * @param  device: Pointer to device instance
  * @retval None
  */
void W25QXX_ReadUniqID(W25QXX_Device_t *device);

/**
  * @brief  Enable write operations
  * @param  device: Pointer to device instance
  * @retval None
  */
void W25QXX_WriteEnable(W25QXX_Device_t *device);

/**
  * @brief  Disable write operations
  * @param  device: Pointer to device instance
  * @retval None
  */
void W25QXX_WriteDisable(W25QXX_Device_t *device);

/**
  * @brief  Wait for write operations to complete
  * @param  device: Pointer to device instance
  * @retval None
  */
void W25QXX_WaitForWriteEnd(W25QXX_Device_t *device);

/**
  * @brief  Erases a chip.
  * @param  device: Pointer to device instance
  * @retval None
  */
void W25QXX_EraseChip(W25QXX_Device_t *device);

/**
  * @brief  Erases a sector.
  * @param  device: Pointer to device instance
  * @param  SectorAddr: Sector address
  * @retval None
  */
void W25QXX_EraseSector(W25QXX_Device_t *device, uint32_t SectorAddr);

/**
  * @brief  Erases a block.
  * @param  device: Pointer to device instance
  * @param  BlockAddr: Block address
  * @retval None
  */
void W25QXX_EraseBlock(W25QXX_Device_t *device, uint32_t BlockAddr);

/**
  * @brief  Convert page address to sector address
  * @param  device: Pointer to device instance
  * @param  PageAddress: Page address
  * @retval Sector address
  */
uint32_t W25QXX_PageToSector(W25QXX_Device_t *device, uint32_t PageAddress);

/**
  * @brief  Convert page address to block address
  * @param  device: Pointer to device instance
  * @param  PageAddress: Page address
  * @retval Block address
  */
uint32_t W25QXX_PageToBlock(W25QXX_Device_t *device, uint32_t PageAddress);

/**
  * @brief  Convert sector address to block address
  * @param  device: Pointer to device instance
  * @param  SectorAddress: Sector address
  * @retval Block address
  */
uint32_t W25QXX_SectorToBlock(W25QXX_Device_t *device, uint32_t SectorAddress);

/**
  * @brief  Convert sector address to page address
  * @param  device: Pointer to device instance
  * @param  SectorAddress: Sector address
  * @retval Page address
  */
uint32_t W25QXX_SectorToPage(W25QXX_Device_t *device, uint32_t SectorAddress);

/**
  * @brief  Convert block address to page address
  * @param  device: Pointer to device instance
  * @param  BlockAddress: Block address
  * @retval Page address
  */
uint32_t W25QXX_BlockToPage(W25QXX_Device_t *device, uint32_t BlockAddress);

/**
  * @brief  Check if page is empty
  * @param  device: Pointer to device instance
  * @param  Page_Address: Page address
  * @param  OffsetInByte: Offset in bytes
  * @retval 1 if empty, 0 if not
  */
uint8_t W25QXX_IsEmptyPage(W25QXX_Device_t *device, uint32_t Page_Address, uint32_t OffsetInByte);

/**
  * @brief  Check if sector is empty
  * @param  device: Pointer to device instance
  * @param  Sector_Address: Sector address
  * @param  OffsetInByte: Offset in bytes
  * @retval 1 if empty, 0 if not
  */
uint8_t W25QXX_IsEmptySector(W25QXX_Device_t *device, uint32_t Sector_Address, uint32_t OffsetInByte);

/**
  * @brief  Check if block is empty
  * @param  device: Pointer to device instance
  * @param  Block_Address: Block address
  * @param  OffsetInByte: Offset in bytes
  * @retval 1 if empty, 0 if not
  */
uint8_t W25QXX_IsEmptyBlock(W25QXX_Device_t *device, uint32_t Block_Address, uint32_t OffsetInByte);

/**
  * @brief  Write a single byte
  * @param  device: Pointer to device instance
  * @param  byte: Byte to write
  * @param  addr: Address to write to
  * @retval None
  */
void W25QXX_WriteByte(W25QXX_Device_t *device, uint8_t byte, uint32_t addr);

/**
  * @brief  Write multiple bytes
  * @param  device: Pointer to device instance
  * @param  pBuffer: Buffer containing data
  * @param  addr: Address to write to
  * @param  NumByteToWrite: Number of bytes to write
  * @retval None
  */
void W25QXX_WriteBytes(W25QXX_Device_t *device, uint8_t *pBuffer, uint32_t addr, uint32_t NumByteToWrite);

/**
  * @brief  Write to a page
  * @param  device: Pointer to device instance
  * @param  pBuffer: Buffer containing data
  * @param  Page_Address: Page address
  * @param  OffsetInByte: Offset in bytes
  * @param  NumByteToWrite_up_to_PageSize: Number of bytes to write (up to page size)
  * @retval None
  */
void W25QXX_WritePage(W25QXX_Device_t *device, uint8_t *pBuffer, uint32_t Page_Address, uint32_t OffsetInByte, uint32_t NumByteToWrite_up_to_PageSize);

/**
  * @brief  Write to a sector
  * @param  device: Pointer to device instance
  * @param  pBuffer: Buffer containing data
  * @param  Sector_Address: Sector address
  * @param  OffsetInByte: Offset in bytes
  * @param  NumByteToWrite_up_to_SectorSize: Number of bytes to write (up to sector size)
  * @retval None
  */
void W25QXX_WriteSector(W25QXX_Device_t *device, uint8_t *pBuffer, uint32_t Sector_Address, uint32_t OffsetInByte, uint32_t NumByteToWrite_up_to_SectorSize);

/**
  * @brief  Write to a block
  * @param  device: Pointer to device instance
  * @param  pBuffer: Buffer containing data
  * @param  Block_Address: Block address
  * @param  OffsetInByte: Offset in bytes
  * @param  NumByteToWrite_up_to_BlockSize: Number of bytes to write (up to block size)
  * @retval None
  */
void W25QXX_WriteBlock(W25QXX_Device_t *device, uint8_t* pBuffer, uint32_t Block_Address, uint32_t OffsetInByte, uint32_t NumByteToWrite_up_to_BlockSize);

/**
  * @brief  Read a single byte
  * @param  device: Pointer to device instance
  * @param  pBuffer: Buffer to store data
  * @param  Bytes_Address: Address to read from
  * @retval None
  */
void W25QXX_ReadByte(W25QXX_Device_t *device, uint8_t *pBuffer, uint32_t Bytes_Address);

/**
  * @brief  Read multiple bytes
  * @param  device: Pointer to device instance
  * @param  pBuffer: Buffer to store data
  * @param  ReadAddr: Address to read from
  * @param  NumByteToRead: Number of bytes to read
  * @retval None
  */
void W25QXX_ReadBytes(W25QXX_Device_t *device, uint8_t* pBuffer, uint32_t ReadAddr, uint32_t NumByteToRead);

/**
  * @brief  Read from a page
  * @param  device: Pointer to device instance
  * @param  pBuffer: Buffer to store data
  * @param  Page_Address: Page address
  * @param  OffsetInByte: Offset in bytes
  * @param  NumByteToRead_up_to_PageSize: Number of bytes to read (up to page size)
  * @retval None
  */
void W25QXX_ReadPage(W25QXX_Device_t *device, uint8_t *pBuffer, uint32_t Page_Address, uint32_t OffsetInByte, uint32_t NumByteToRead_up_to_PageSize);

/**
  * @brief  Read from a sector
  * @param  device: Pointer to device instance
  * @param  pBuffer: Buffer to store data
  * @param  Sector_Address: Sector address
  * @param  OffsetInByte: Offset in bytes
  * @param  NumByteToRead_up_to_SectorSize: Number of bytes to read (up to sector size)
  * @retval None
  */
void W25QXX_ReadSector(W25QXX_Device_t *device, uint8_t *pBuffer, uint32_t Sector_Address, uint32_t OffsetInByte, uint32_t NumByteToRead_up_to_SectorSize);

/**
  * @brief  Read from a block
  * @param  device: Pointer to device instance
  * @param  pBuffer: Buffer to store data
  * @param  Block_Address: Block address
  * @param  OffsetInByte: Offset in bytes
  * @param  NumByteToRead_up_to_BlockSize: Number of bytes to read (up to block size)
  * @retval None
  */
void W25QXX_ReadBlock(W25QXX_Device_t *device, uint8_t *pBuffer, uint32_t Block_Address, uint32_t OffsetInByte, uint32_t NumByteToRead_up_to_BlockSize);

/**
  * @brief  Проверяет данные на указанном адресе во внешней Flash
  * @param  device: Указатель на устройство
  * @param  address: Адрес для проверки
  * @param  size: Размер данных для проверки (в байтах)
  * @retval 1 если данные обнаружены (не все 0xFF), 0 если данные пустые
  */
uint8_t W25QXX_CheckDataAtAddress(W25QXX_Device_t *device, uint32_t address, uint32_t size);

/**
  * @brief  Сбрасывает последнюю ошибку
  * @param  device: Указатель на устройство
  * @retval None
  */
void W25QXX_ClearLastStatus(W25QXX_Device_t *device);

/**
  * @brief  Возвращает последний статус операции
  * @param  device: Указатель на устройство
  * @retval HAL_StatusTypeDef: Последний статус операции
  */
HAL_StatusTypeDef W25QXX_GetLastStatus(W25QXX_Device_t *device);

#ifdef __cplusplus
}
#endif

#endif /* __FLASH_SPI_H */
