/**
  ******************************************************************************
  * @file    flash_spi.c
  * @brief   SPI Flash driver implementation.
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/

#include "cmsis_os.h"
#include "flash_spi.h"
#include <string.h>

/* Private function prototypes -----------------------------------------------*/
static void W25QXX_ResetInfo(W25QXX_Device_t *device);

/* External variables ---------------------------------------------------------*/

#if (INIT_DEBUG == 1)
#include <stdio.h>
char debug_buff[64] = {0,};
#endif

/* Functions -----------------------------------------------------------------*/

/**
  * @brief  Initialize device instance
  * @param  device: Pointer to device instance
  * @retval None
  */
void W25QXX_InitDevice(W25QXX_Device_t *device)
{
    memset(device, 0, sizeof(W25QXX_Device_t));

    W25QXX_ResetInfo(device);
}

/**
  * @brief  Reset device information structure
  * @param  device: Pointer to device instance
  * @retval None
  */
static void W25QXX_ResetInfo(W25QXX_Device_t *device)
{
    memset(&device->Info, 0, sizeof(W25QXX_Info_t));
}

/**
  * @brief  Set the OS usage flag.
  * @param  device: Pointer to device instance
  * @param  UsedInOS: Flag indicating if used in OS
  * @retval None
  */
void W25QXX_SetUsedInOS(W25QXX_Device_t *device, uint8_t UsedInOS)
{
    device->UsedInOS = UsedInOS;
}

/**
  * @brief  Delay for the device
  * @param  device: Pointer to device instance
  * @param  delay: Delay in ms
  * @retval None
  */
void W25QXX_Delay(W25QXX_Device_t *device, uint32_t delay)
{
    if (device->UsedInOS) {
        // If used in OS, use OS delay function
        osDelay(delay);
    } else {
        // Otherwise use HAL delay
        HAL_Delay(delay);
    }
}

/**
  * @brief  Transmit/Receive byte over SPI
  * @param  device: Pointer to device instance
  * @param  Data: Data to transmit
  * @retval Received data
  */
uint8_t W25QXX_SPI(W25QXX_Device_t *device, uint8_t Data)
{
    uint8_t ret;

    HAL_SPI_TransmitReceive(device->hspi, &Data, &ret, 1, 100);

    return ret;
}

/**
  * @brief  Read device ID
  * @param  device: Pointer to device instance
  * @retval Device ID
  */
uint32_t W25QXX_ReadID(W25QXX_Device_t *device)
{
    uint32_t Temp = 0, Temp0 = 0, Temp1 = 0, Temp2 = 0;

    W25QFLASH_CS_SELECT(device);

    W25QXX_SPI(device, W25_GET_JEDEC_ID);

    Temp0 = W25QXX_SPI(device, W25QXX_DUMMY_BYTE);
    Temp1 = W25QXX_SPI(device, W25QXX_DUMMY_BYTE);
    Temp2 = W25QXX_SPI(device, W25QXX_DUMMY_BYTE);

    W25QFLASH_CS_UNSELECT(device);

    Temp = (Temp0 << 16) | (Temp1 << 8) | Temp2;

    return Temp;
}

/**
  * @brief  Read unique ID
  * @param  device: Pointer to device instance
  * @retval None
  */
void W25QXX_ReadUniqID(W25QXX_Device_t *device)
{
    W25QFLASH_CS_SELECT(device);
    W25QXX_SPI(device, W25_READ_UNIQUE_ID);

    for (uint8_t i = 0; i < 4; i++)
        W25QXX_SPI(device, W25QXX_DUMMY_BYTE);

    for (uint8_t i = 0; i < 8; i++)
        device->Info.UniqID[i] = W25QXX_SPI(device, W25QXX_DUMMY_BYTE);

    W25QFLASH_CS_UNSELECT(device);
}

/**
  * @brief  Enable write operations
  * @param  device: Pointer to device instance
  * @retval None
  */
void W25QXX_WriteEnable(W25QXX_Device_t *device)
{
    W25QFLASH_CS_SELECT(device);
    W25QXX_SPI(device, W25_WRITE_ENABLE);
    W25QFLASH_CS_UNSELECT(device);
    W25QXX_Delay(device, 1);
}

/**
  * @brief  Disable write operations
  * @param  device: Pointer to device instance
  * @retval None
  */
void W25QXX_WriteDisable(W25QXX_Device_t *device)
{
    W25QFLASH_CS_SELECT(device);
    W25QXX_SPI(device, W25_WRITE_DISABLE);
    W25QFLASH_CS_UNSELECT(device);
    W25QXX_Delay(device, 1);
}

/**
  * @brief  Wait for write operations to complete
  * @param  device: Pointer to device instance
  * @retval None
  */
void W25QXX_WaitForWriteEnd(W25QXX_Device_t *device)
{
    W25QXX_Delay(device, 1);
    W25QFLASH_CS_SELECT(device);
    W25QXX_SPI(device, W25_READ_STATUS_1);

    do {
        device->Info.StatusRegister1 = W25QXX_SPI(device, W25QXX_DUMMY_BYTE);
        W25QXX_Delay(device, 1);
    } while ((device->Info.StatusRegister1 & 0x01) == 0x01);

    W25QFLASH_CS_UNSELECT(device);
}

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
uint8_t W25QXX_Init(W25QXX_Device_t *device, SPI_HandleTypeDef *hspi, uint32_t startAddr,
        pins_spi_t ChipSelect, pins_spi_t WriteProtect,
        pins_spi_t Hold, uint8_t UsedInOS)
{
    device->WriteProtect = WriteProtect;
    device->ChipSelect = ChipSelect;
    device->Hold = Hold;
    device->hspi = hspi;
    device->UsedInOS = UsedInOS;

    HAL_GPIO_WritePin(WriteProtect.GPIO_Port, WriteProtect.GPIO_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(Hold.GPIO_Port, Hold.GPIO_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(ChipSelect.GPIO_Port, ChipSelect.GPIO_Pin, GPIO_PIN_SET);

    device->Info.Lock = 1;
    while (HAL_GetTick() < 100)
        W25QXX_Delay(device, 1);

    W25QFLASH_CS_UNSELECT(device);
    W25QXX_Delay(device, 100);

    uint32_t id;

#if (INIT_DEBUG == 1)
    //HAL_UART_Transmit(DEBUG_UART, (uint8_t*)"Init Begin...\n", 14, 1000);
#endif

    id = W25QXX_ReadID(device);

#if (INIT_DEBUG == 1)
    snprintf(debug_buff, 64, "ID:0x%lX\n", id);
    //HAL_UART_Transmit(DEBUG_UART, (uint8_t*)debug_buff, strlen(debug_buff), 1000);
#endif

    switch (id & 0x0000FFFF)
    {
        case 0x401A:    //  w25q512
            device->Info.ID = W25Q512_ID;
            device->Info.BlockCount = 1024;
#if (INIT_DEBUG == 1)
            //HAL_UART_Transmit(DEBUG_UART, (uint8_t*)"Chip: w25q512\n", 14, 1000);
#endif
            break;

        case 0x4019:    //  w25q256
            device->Info.ID = W25Q256_ID;
            device->Info.BlockCount = 512;
#if (INIT_DEBUG == 1)
            //HAL_UART_Transmit(DEBUG_UART, (uint8_t*)"Chip: w25q256\n", 14, 1000);
#endif
            break;

        case 0x4018:    //  w25q128
            device->Info.ID = W25Q128_ID;
            device->Info.BlockCount = 256;
#if (INIT_DEBUG == 1)
            //HAL_UART_Transmit(DEBUG_UART, (uint8_t*)"Chip: w25q128\n", 14, 1000);
#endif
            break;

        case 0x7018:    //  w25q128 (клон/альтернативная модификация с Memory Type 0x70)
            device->Info.ID = W25Q128_ID;
            device->Info.BlockCount = 256;
#if (INIT_DEBUG == 1)
            //HAL_UART_Transmit(DEBUG_UART, (uint8_t*)"Chip: w25q128 (non-standard Memory Type 0x70)\n", 47, 1000);
#endif
            break;

        case 0x4017:    //  w25q64
            device->Info.ID = W25Q64_ID;
            device->Info.BlockCount = 128;
#if (INIT_DEBUG == 1)
            //HAL_UART_Transmit(DEBUG_UART, (uint8_t*)"Chip: w25q64\n", 13, 1000);
#endif
            break;

        case 0x4016:    //  w25q32
            device->Info.ID = W25Q32_ID;
            device->Info.BlockCount = 64;
#if (INIT_DEBUG == 1)
            //HAL_UART_Transmit(DEBUG_UART, (uint8_t*)"Chip: w25q32\n", 13, 1000);
#endif
            break;

        case 0x4015:    //  w25q16
            device->Info.ID = W25Q16_ID;
            device->Info.BlockCount = 32;
#if (INIT_DEBUG == 1)
            //HAL_UART_Transmit(DEBUG_UART, (uint8_t*)"Chip: w25q16\n", 13, 1000);
#endif
            break;

        case 0x4014:    //  w25q80
            device->Info.ID = W25Q80_ID;
            device->Info.BlockCount = 16;
#if (INIT_DEBUG == 1)
            //HAL_UART_Transmit(DEBUG_UART, (uint8_t*)"Chip: w25q80\n", 13, 1000);
#endif
            break;

        case 0x4013:    //  w25q40
            device->Info.ID = W25Q40_ID;
            device->Info.BlockCount = 8;
#if (INIT_DEBUG == 1)
            //HAL_UART_Transmit(DEBUG_UART, (uint8_t*)"Chip: w25q40\n", 13, 1000);
#endif
            break;

        case 0x4012:    //  w25q20
            device->Info.ID = W25Q20_ID;
            device->Info.BlockCount = 4;
#if (INIT_DEBUG == 1)
            //HAL_UART_Transmit(DEBUG_UART, (uint8_t*)"Chip: w25q20\n", 13, 1000);
#endif
            break;

        case 0x4011:    //  w25q10
            device->Info.ID = W25Q10_ID;
            device->Info.BlockCount = 2;
#if (INIT_DEBUG == 1)
            //HAL_UART_Transmit(DEBUG_UART, (uint8_t*)"Chip: w25q10\n", 13, 1000);
#endif
            break;

        ////////////////////////////////////////////////////////////////////////////////

        case 0x3017:    //  w25x64
            //device->Info.ID = W25Q64_ID;
            device->Info.BlockCount = 128;
#if (INIT_DEBUG == 1)
            //HAL_UART_Transmit(DEBUG_UART, (uint8_t*)"Chip: w25x64\n", 13, 1000);
#endif
            break;

        case 0x3016:    //  w25x32
            //device->Info.ID = W25Q32_ID;
            device->Info.BlockCount = 64;
#if (INIT_DEBUG == 1)
            //HAL_UART_Transmit(DEBUG_UART, (uint8_t*)"Chip: w25x32\n", 13, 1000);
#endif
            break;

        case 0x3015:    //  w25q16
            //device->Info.ID = W25Q16_ID;
            device->Info.BlockCount = 32;
#if (INIT_DEBUG == 1)
            //HAL_UART_Transmit(DEBUG_UART, (uint8_t*)"Chip: w25x16\n", 13, 1000);
#endif
            break;

            ////////////////////////////////////////////////////////////////////////////////
        case 0x3014:    //  w25x80
            //device->Info.ID = W25Q80_ID;
            device->Info.BlockCount = 16;
#if (INIT_DEBUG == 1)
            //HAL_UART_Transmit(DEBUG_UART, (uint8_t*)"Chip: w25x80\n", 13, 1000);
#endif
            break;

        case 0x3013:    //  w25x40
            //device->Info.ID = W25Q40_ID;
            device->Info.BlockCount = 8;
#if (INIT_DEBUG == 1)
            //HAL_UART_Transmit(DEBUG_UART, (uint8_t*)"Chip: w25x40\n", 13, 1000);
#endif
            break;

        case 0x3012:    //  w25x20
            //device->Info.ID = W25Q20_ID;
            device->Info.BlockCount = 4;
#if (INIT_DEBUG == 1)
            //HAL_UART_Transmit(DEBUG_UART, (uint8_t*)"Chip: w25x20\n", 13, 1000);
#endif
            break;

        case 0x3011:    //  w25x10
            //device->Info.ID = W25Q10_ID;
            device->Info.BlockCount = 2;
#if (INIT_DEBUG == 1)
            //HAL_UART_Transmit(DEBUG_UART, (uint8_t*)"Chip: w25x10\n", 13, 1000);
#endif
            break;

        default:
#if (INIT_DEBUG == 1)
            //HAL_UART_Transmit(DEBUG_UART, (uint8_t*)"Unknown ID\n", 11, 1000);
#endif
            device->Info.Lock = 0;
            return 0;
    }

    device->Info.PageSize = 256;
    device->Info.SectorSize = 0x1000;
    device->Info.SectorCount = device->Info.BlockCount * 16;
    device->Info.PageCount = (device->Info.SectorCount * device->Info.SectorSize) / device->Info.PageSize;
    device->Info.BlockSize = device->Info.SectorSize * 16;
    device->Info.CapacityInKiloByte = (device->Info.SectorCount * device->Info.SectorSize) / 1024;

    W25QXX_ReadUniqID(device);

    device->Info.Lock = 0;
    device->lastStatus = HAL_OK;
    return 1;
}

/**
  * @brief  Erases a chip.
  * @param  device: Pointer to device instance
  * @retval None
  */
void W25QXX_EraseChip(W25QXX_Device_t *device)
{
    while (device->Info.Lock == 1)
        W25QXX_Delay(device, 1);

    device->Info.Lock = 1;

    W25QXX_WriteEnable(device);

    W25QFLASH_CS_SELECT(device);
    W25QXX_SPI(device, W25_CHIP_ERASE);
    W25QFLASH_CS_UNSELECT(device);

    W25QXX_WaitForWriteEnd(device);

    W25QXX_Delay(device, 10);

    device->Info.Lock = 0;
}

/**
  * @brief  Erases a sector.
  * @param  device: Pointer to device instance
  * @param  SectorAddr: Sector address
  * @retval None
  */
void W25QXX_EraseSector(W25QXX_Device_t *device, uint32_t SectorAddr)
{
    while (device->Info.Lock == 1)
        W25QXX_Delay(device, 1);

    device->Info.Lock = 1;

    W25QXX_WaitForWriteEnd(device);
    SectorAddr = SectorAddr * device->Info.SectorSize;

    W25QXX_WriteEnable(device);

    W25QFLASH_CS_SELECT(device);

    W25QXX_SPI(device, W25_SECTOR_ERASE);

    if (device->Info.ID >= W25Q256_ID)
        W25QXX_SPI(device, (SectorAddr & 0xFF000000) >> 24);

    W25QXX_SPI(device, (SectorAddr & 0xFF0000) >> 16);
    W25QXX_SPI(device, (SectorAddr & 0xFF00) >> 8);
    W25QXX_SPI(device, SectorAddr & 0xFF);

    W25QFLASH_CS_UNSELECT(device);

    W25QXX_WaitForWriteEnd(device);

    W25QXX_Delay(device, 1);
    device->Info.Lock = 0;
}

/**
  * @brief  Erases a block.
  * @param  device: Pointer to device instance
  * @param  BlockAddr: Block address
  * @retval None
  */
void W25QXX_EraseBlock(W25QXX_Device_t *device, uint32_t BlockAddr)
{
    while (device->Info.Lock == 1)
        W25QXX_Delay(device, 1);

    device->Info.Lock = 1;

    W25QXX_WaitForWriteEnd(device);

    BlockAddr = BlockAddr * device->Info.SectorSize * 16;

    W25QXX_WriteEnable(device);

    W25QFLASH_CS_SELECT(device);

    W25QXX_SPI(device, W25_BLOCK_ERASE);

    if (device->Info.ID >= W25Q256_ID)
        W25QXX_SPI(device, (BlockAddr & 0xFF000000) >> 24);

    W25QXX_SPI(device, (BlockAddr & 0xFF0000) >> 16);
    W25QXX_SPI(device, (BlockAddr & 0xFF00) >> 8);
    W25QXX_SPI(device, BlockAddr & 0xFF);

    W25QFLASH_CS_UNSELECT(device);

    W25QXX_WaitForWriteEnd(device);

    W25QXX_Delay(device, 1);
    device->Info.Lock = 0;
}

/**
  * @brief  Convert page address to sector address
  * @param  device: Pointer to device instance
  * @param  PageAddress: Page address
  * @retval Sector address
  */
uint32_t W25QXX_PageToSector(W25QXX_Device_t *device, uint32_t PageAddress)
{
    return ((PageAddress * device->Info.PageSize) / device->Info.SectorSize);
}

/**
  * @brief  Convert page address to block address
  * @param  device: Pointer to device instance
  * @param  PageAddress: Page address
  * @retval Block address
  */
uint32_t W25QXX_PageToBlock(W25QXX_Device_t *device, uint32_t PageAddress)
{
    return ((PageAddress * device->Info.PageSize) / device->Info.BlockSize);
}

/**
  * @brief  Convert sector address to block address
  * @param  device: Pointer to device instance
  * @param  SectorAddress: Sector address
  * @retval Block address
  */
uint32_t W25QXX_SectorToBlock(W25QXX_Device_t *device, uint32_t SectorAddress)
{
    return ((SectorAddress * device->Info.SectorSize) / device->Info.BlockSize);
}

/**
  * @brief  Convert sector address to page address
  * @param  device: Pointer to device instance
  * @param  SectorAddress: Sector address
  * @retval Page address
  */
uint32_t W25QXX_SectorToPage(W25QXX_Device_t *device, uint32_t SectorAddress)
{
    return (SectorAddress * device->Info.SectorSize) / device->Info.PageSize;
}

/**
  * @brief  Convert block address to page address
  * @param  device: Pointer to device instance
  * @param  BlockAddress: Block address
  * @retval Page address
  */
uint32_t W25QXX_BlockToPage(W25QXX_Device_t *device, uint32_t BlockAddress)
{
    return (BlockAddress * device->Info.BlockSize) / device->Info.PageSize;
}

/**
  * @brief  Check if page is empty
  * @param  device: Pointer to device instance
  * @param  Page_Address: Page address
  * @param  OffsetInByte: Offset in bytes
  * @retval 1 if empty, 0 if not
  */
uint8_t W25QXX_IsEmptyPage(W25QXX_Device_t *device, uint32_t Page_Address, uint32_t OffsetInByte)
{
    while (device->Info.Lock == 1)
        W25QXX_Delay(device, 1);

    device->Info.Lock = 1;

    uint8_t pBuffer[256] = {0,};
    uint32_t WorkAddress = 0;
    uint16_t size = 0;

    size = device->Info.PageSize - OffsetInByte;
    WorkAddress = (OffsetInByte + Page_Address * device->Info.PageSize);

    W25QFLASH_CS_SELECT(device);

    W25QXX_SPI(device, W25_FAST_READ);

    if (device->Info.ID >= W25Q256_ID)
        W25QXX_SPI(device, (WorkAddress & 0xFF000000) >> 24);

    W25QXX_SPI(device, (WorkAddress & 0xFF0000) >> 16);
    W25QXX_SPI(device, (WorkAddress & 0xFF00) >> 8);
    W25QXX_SPI(device, WorkAddress & 0xFF);

    W25QXX_SPI(device, 0);

    HAL_SPI_Receive(device->hspi, pBuffer, size, 100);

    W25QFLASH_CS_UNSELECT(device);

    for (uint16_t i = 0; i < size; i++)
    {
        if (pBuffer[i] != 0xFF)
        {
            device->Info.Lock = 0;
            return 0;
        }
    }

    device->Info.Lock = 0;
    return 1;
}

/**
  * @brief  Check if sector is empty
  * @param  device: Pointer to device instance
  * @param  Sector_Address: Sector address
  * @param  OffsetInByte: Offset in bytes
  * @retval 1 if empty, 0 if not
  */
uint8_t W25QXX_IsEmptySector(W25QXX_Device_t *device, uint32_t Sector_Address, uint32_t OffsetInByte)
{
    while (device->Info.Lock == 1)
        W25QXX_Delay(device, 1);

    device->Info.Lock = 1;

    uint8_t pBuffer[256] = {0,};
    uint32_t WorkAddress = 0;
    uint16_t s_buff = 256;
    uint16_t size = 0;

    size = device->Info.SectorSize - OffsetInByte;
    WorkAddress = (OffsetInByte + Sector_Address * device->Info.SectorSize);

    uint16_t cikl = size / 256;
    uint16_t cikl2 = size % 256;
    uint16_t count_cikle = 0;

    if (size <= 256)
    {
        count_cikle = 1;
    }
    else if (cikl2 == 0)
    {
        count_cikle = cikl;
    }
    else
    {
        count_cikle = cikl + 1;
    }

    for (uint16_t i = 0; i < count_cikle; i++)
    {
        W25QFLASH_CS_SELECT(device);
        W25QXX_SPI(device, W25_FAST_READ);

        if (device->Info.ID >= W25Q256_ID)
            W25QXX_SPI(device, (WorkAddress & 0xFF000000) >> 24);

        W25QXX_SPI(device, (WorkAddress & 0xFF0000) >> 16);
        W25QXX_SPI(device, (WorkAddress & 0xFF00) >> 8);
        W25QXX_SPI(device, WorkAddress & 0xFF);

        W25QXX_SPI(device, 0);

        if (size < 256) s_buff = size;

        HAL_SPI_Receive(device->hspi, pBuffer, s_buff, 100);

        W25QFLASH_CS_UNSELECT(device);

        for (uint16_t j = 0; j < s_buff; j++)
        {
            if (pBuffer[j] != 0xFF)
            {
                device->Info.Lock = 0;
                return 0;
            }
        }

        size = size - 256;
        WorkAddress = WorkAddress + 256;
    }

    device->Info.Lock = 0;
    return 1;
}

/**
  * @brief  Check if block is empty
  * @param  device: Pointer to device instance
  * @param  Block_Address: Block address
  * @param  OffsetInByte: Offset in bytes
  * @retval 1 if empty, 0 if not
  */
uint8_t W25QXX_IsEmptyBlock(W25QXX_Device_t *device, uint32_t Block_Address, uint32_t OffsetInByte)
{
    while (device->Info.Lock == 1)
        W25QXX_Delay(device, 1);

    device->Info.Lock = 1;

    uint8_t pBuffer[256] = {0,};
    uint32_t WorkAddress = 0;
    uint16_t s_buff = 256;
    uint32_t size = 0;

    size = device->Info.BlockSize - OffsetInByte;
    WorkAddress = (OffsetInByte + Block_Address * device->Info.BlockSize);

    uint16_t cikl = size / 256;
    uint16_t cikl2 = size % 256;
    uint16_t count_cikle = 0;

    if (size <= 256)
    {
        count_cikle = 1;
    }
    else if (cikl2 == 0)
    {
        count_cikle = cikl;
    }
    else
    {
        count_cikle = cikl + 1;
    }

    for (uint16_t i = 0; i < count_cikle; i++)
    {
        W25QFLASH_CS_SELECT(device);
        W25QXX_SPI(device, W25_FAST_READ);

        if (device->Info.ID >= W25Q256_ID)
            W25QXX_SPI(device, (WorkAddress & 0xFF000000) >> 24);

        W25QXX_SPI(device, (WorkAddress & 0xFF0000) >> 16);
        W25QXX_SPI(device, (WorkAddress & 0xFF00) >> 8);
        W25QXX_SPI(device, WorkAddress & 0xFF);

        W25QXX_SPI(device, 0);

        if (size < 256) s_buff = size;

        HAL_SPI_Receive(device->hspi, pBuffer, s_buff, 100);

        W25QFLASH_CS_UNSELECT(device);

        for (uint16_t j = 0; j < s_buff; j++)
        {
            if (pBuffer[j] != 0xFF)
            {
                device->Info.Lock = 0;
                return 0;
            }
        }

        size = size - 256;
        WorkAddress = WorkAddress + 256;
    }

    device->Info.Lock = 0;
    return 1;
}

/**
  * @brief  Write a single byte
  * @param  device: Pointer to device instance
  * @param  byte: Byte to write
  * @param  addr: Address to write to
  * @retval None
  */
void W25QXX_WriteByte(W25QXX_Device_t *device, uint8_t byte, uint32_t addr)
{
    while (device->Info.Lock == 1)
        W25QXX_Delay(device, 1);

    device->Info.Lock = 1;

    W25QXX_WaitForWriteEnd(device);
    W25QXX_WriteEnable(device);

    W25QFLASH_CS_SELECT(device);

    W25QXX_SPI(device, W25_PAGE_PROGRAMM);

    if (device->Info.ID >= W25Q256_ID)
        W25QXX_SPI(device, (addr & 0xFF000000) >> 24);

    W25QXX_SPI(device, (addr & 0xFF0000) >> 16);
    W25QXX_SPI(device, (addr & 0xFF00) >> 8);
    W25QXX_SPI(device, addr & 0xFF);

    W25QXX_SPI(device, byte);

    W25QFLASH_CS_UNSELECT(device);

    W25QXX_WaitForWriteEnd(device);

    device->Info.Lock = 0;
}

/**
  * @brief  Write multiple bytes
  * @param  device: Pointer to device instance
  * @param  pBuffer: Buffer containing data
  * @param  addr: Address to write to
  * @param  NumByteToWrite: Number of bytes to write
  * @retval None
  */
void W25QXX_WriteBytes(W25QXX_Device_t *device, uint8_t *pBuffer, uint32_t addr, uint32_t NumByteToWrite)
{
    while (device->Info.Lock == 1)
        W25QXX_Delay(device, 1);

    device->Info.Lock = 1;

    // Write by page size chunks to avoid crossing page boundaries
    uint32_t page = addr / device->Info.PageSize;
    uint32_t offset = addr % device->Info.PageSize;
    uint32_t bytesToWrite;

    while (NumByteToWrite > 0)
    {
        // Calculate bytes to write in current page
        if (offset + NumByteToWrite > device->Info.PageSize)
            bytesToWrite = device->Info.PageSize - offset;
        else
            bytesToWrite = NumByteToWrite;

        // Write the page
        W25QXX_WritePage(device, pBuffer, page, offset, bytesToWrite);

        // Adjust variables for next page
        NumByteToWrite -= bytesToWrite;
        pBuffer += bytesToWrite;
        page++;
        offset = 0;
    }

    device->Info.Lock = 0;
}

/**
  * @brief  Write to a page
  * @param  device: Pointer to device instance
  * @param  pBuffer: Buffer containing data
  * @param  Page_Address: Page address
  * @param  OffsetInByte: Offset in bytes
  * @param  NumByteToWrite_up_to_PageSize: Number of bytes to write (up to page size)
  * @retval None
  */
void W25QXX_WritePage(W25QXX_Device_t *device, uint8_t *pBuffer, uint32_t Page_Address, uint32_t OffsetInByte, uint32_t NumByteToWrite_up_to_PageSize)
{
    while (device->Info.Lock == 1)
        W25QXX_Delay(device, 1);

    device->Info.Lock = 1;

    if (((NumByteToWrite_up_to_PageSize + OffsetInByte) > device->Info.PageSize) || (NumByteToWrite_up_to_PageSize == 0))
        NumByteToWrite_up_to_PageSize = device->Info.PageSize - OffsetInByte;

    if ((OffsetInByte + NumByteToWrite_up_to_PageSize) > device->Info.PageSize)
        NumByteToWrite_up_to_PageSize = device->Info.PageSize - OffsetInByte;

    W25QXX_WaitForWriteEnd(device);

    W25QXX_WriteEnable(device);

    W25QFLASH_CS_SELECT(device);

    W25QXX_SPI(device, W25_PAGE_PROGRAMM);

    Page_Address = (Page_Address * device->Info.PageSize) + OffsetInByte;

    if (device->Info.ID >= W25Q256_ID)
        W25QXX_SPI(device, (Page_Address & 0xFF000000) >> 24);

    W25QXX_SPI(device, (Page_Address & 0xFF0000) >> 16);
    W25QXX_SPI(device, (Page_Address & 0xFF00) >> 8);
    W25QXX_SPI(device, Page_Address & 0xFF);

    HAL_SPI_Transmit(device->hspi, pBuffer, NumByteToWrite_up_to_PageSize, 100);

    W25QFLASH_CS_UNSELECT(device);

    W25QXX_WaitForWriteEnd(device);

    W25QXX_Delay(device, 1);
    device->Info.Lock = 0;
}

/**
  * @brief  Write to a sector
  * @param  device: Pointer to device instance
  * @param  pBuffer: Buffer containing data
  * @param  Sector_Address: Sector address
  * @param  OffsetInByte: Offset in bytes
  * @param  NumByteToWrite_up_to_SectorSize: Number of bytes to write (up to sector size)
  * @retval None
  */
void W25QXX_WriteSector(W25QXX_Device_t *device, uint8_t *pBuffer, uint32_t Sector_Address, uint32_t OffsetInByte, uint32_t NumByteToWrite_up_to_SectorSize)
{
    if ((NumByteToWrite_up_to_SectorSize > device->Info.SectorSize) || (NumByteToWrite_up_to_SectorSize == 0))
        NumByteToWrite_up_to_SectorSize = device->Info.SectorSize;

    uint32_t StartPage;
    int32_t BytesToWrite;
    uint32_t LocalOffset;

    if ((OffsetInByte + NumByteToWrite_up_to_SectorSize) > device->Info.SectorSize)
        BytesToWrite = device->Info.SectorSize - OffsetInByte;
    else
        BytesToWrite = NumByteToWrite_up_to_SectorSize;

    StartPage = W25QXX_SectorToPage(device, Sector_Address) + (OffsetInByte / device->Info.PageSize);
    LocalOffset = OffsetInByte % device->Info.PageSize;

    do
    {
        W25QXX_WritePage(device, pBuffer, StartPage, LocalOffset, BytesToWrite);
        StartPage++;

        BytesToWrite -= device->Info.PageSize - LocalOffset;
        pBuffer += device->Info.PageSize - LocalOffset;
        LocalOffset = 0;
    }
    while (BytesToWrite > 0);
}

/**
  * @brief  Write to a block
  * @param  device: Pointer to device instance
  * @param  pBuffer: Buffer containing data
  * @param  Block_Address: Block address
  * @param  OffsetInByte: Offset in bytes
  * @param  NumByteToWrite_up_to_BlockSize: Number of bytes to write (up to block size)
  * @retval None
  */
void W25QXX_WriteBlock(W25QXX_Device_t *device, uint8_t* pBuffer, uint32_t Block_Address, uint32_t OffsetInByte, uint32_t NumByteToWrite_up_to_BlockSize)
{
    if ((NumByteToWrite_up_to_BlockSize > device->Info.BlockSize) || (NumByteToWrite_up_to_BlockSize == 0))
        NumByteToWrite_up_to_BlockSize = device->Info.BlockSize;

    uint32_t StartPage;
    int32_t BytesToWrite;
    uint32_t LocalOffset;

    if ((OffsetInByte + NumByteToWrite_up_to_BlockSize) > device->Info.BlockSize)
        BytesToWrite = device->Info.BlockSize - OffsetInByte;
    else
        BytesToWrite = NumByteToWrite_up_to_BlockSize;

    StartPage = W25QXX_BlockToPage(device, Block_Address) + (OffsetInByte / device->Info.PageSize);
    LocalOffset = OffsetInByte % device->Info.PageSize;

    do
    {
        W25QXX_WritePage(device, pBuffer, StartPage, LocalOffset, BytesToWrite);
        StartPage++;
        BytesToWrite -= device->Info.PageSize - LocalOffset;
        pBuffer += device->Info.PageSize - LocalOffset;
        LocalOffset = 0;
    }
    while (BytesToWrite > 0);
}

/**
  * @brief  Read a single byte
  * @param  device: Pointer to device instance
  * @param  pBuffer: Buffer to store data
  * @param  Bytes_Address: Address to read from
  * @retval None
  */
void W25QXX_ReadByte(W25QXX_Device_t *device, uint8_t *pBuffer, uint32_t Bytes_Address)
{
    while (device->Info.Lock == 1)
        W25QXX_Delay(device, 1);

    device->Info.Lock = 1;

    W25QFLASH_CS_SELECT(device);
    W25QXX_SPI(device, W25_FAST_READ);

    if (device->Info.ID >= W25Q256_ID)
        W25QXX_SPI(device, (Bytes_Address & 0xFF000000) >> 24);

    W25QXX_SPI(device, (Bytes_Address & 0xFF0000) >> 16);
    W25QXX_SPI(device, (Bytes_Address & 0xFF00) >> 8);
    W25QXX_SPI(device, Bytes_Address & 0xFF);
    W25QXX_SPI(device, 0);

    *pBuffer = W25QXX_SPI(device, W25QXX_DUMMY_BYTE);

    W25QFLASH_CS_UNSELECT(device);

    device->Info.Lock = 0;
}

/**
  * @brief  Read multiple bytes
  * @param  device: Pointer to device instance
  * @param  pBuffer: Buffer to store data
  * @param  ReadAddr: Address to read from
  * @param  NumByteToRead: Number of bytes to read
  * @retval None
  */
void W25QXX_ReadBytes(W25QXX_Device_t *device, uint8_t* pBuffer, uint32_t ReadAddr, uint32_t NumByteToRead)
{
    while (device->Info.Lock == 1)
        W25QXX_Delay(device, 1);

    device->Info.Lock = 1;

    W25QFLASH_CS_SELECT(device);

    W25QXX_SPI(device, W25_FAST_READ);

    if (device->Info.ID >= W25Q256_ID)
        W25QXX_SPI(device, (ReadAddr & 0xFF000000) >> 24);

    W25QXX_SPI(device, (ReadAddr & 0xFF0000) >> 16);
    W25QXX_SPI(device, (ReadAddr & 0xFF00) >> 8);
    W25QXX_SPI(device, ReadAddr & 0xFF);
    W25QXX_SPI(device, 0);

    HAL_SPI_Receive(device->hspi, pBuffer, NumByteToRead, 2000);

    W25QFLASH_CS_UNSELECT(device);

    W25QXX_Delay(device, 1);
    device->Info.Lock = 0;
}

/**
  * @brief  Read from a page
  * @param  device: Pointer to device instance
  * @param  pBuffer: Buffer to store data
  * @param  Page_Address: Page address
  * @param  OffsetInByte: Offset in bytes
  * @param  NumByteToRead_up_to_PageSize: Number of bytes to read (up to page size)
  * @retval None
  */
void W25QXX_ReadPage(W25QXX_Device_t *device, uint8_t *pBuffer, uint32_t Page_Address, uint32_t OffsetInByte, uint32_t NumByteToRead_up_to_PageSize)
{
    while (device->Info.Lock == 1)
        W25QXX_Delay(device, 1);

    device->Info.Lock = 1;

    if ((NumByteToRead_up_to_PageSize > device->Info.PageSize) || (NumByteToRead_up_to_PageSize == 0))
        NumByteToRead_up_to_PageSize = device->Info.PageSize;

    if ((OffsetInByte + NumByteToRead_up_to_PageSize) > device->Info.PageSize)
        NumByteToRead_up_to_PageSize = device->Info.PageSize - OffsetInByte;

    Page_Address = Page_Address * device->Info.PageSize + OffsetInByte;
    W25QFLASH_CS_SELECT(device);

    W25QXX_SPI(device, W25_FAST_READ);

    if (device->Info.ID >= W25Q256_ID)
        W25QXX_SPI(device, (Page_Address & 0xFF000000) >> 24);

    W25QXX_SPI(device, (Page_Address & 0xFF0000) >> 16);
    W25QXX_SPI(device, (Page_Address & 0xFF00) >> 8);
    W25QXX_SPI(device, Page_Address & 0xFF);

    W25QXX_SPI(device, 0);

    HAL_SPI_Receive(device->hspi, pBuffer, NumByteToRead_up_to_PageSize, 100);

    W25QFLASH_CS_UNSELECT(device);

    W25QXX_Delay(device, 1);
    device->Info.Lock = 0;
}

/**
  * @brief  Read from a sector
  * @param  device: Pointer to device instance
  * @param  pBuffer: Buffer to store data
  * @param  Sector_Address: Sector address
  * @param  OffsetInByte: Offset in bytes
  * @param  NumByteToRead_up_to_SectorSize: Number of bytes to read (up to sector size)
  * @retval None
  */
void W25QXX_ReadSector(W25QXX_Device_t *device, uint8_t *pBuffer, uint32_t Sector_Address, uint32_t OffsetInByte, uint32_t NumByteToRead_up_to_SectorSize)
{
    if ((NumByteToRead_up_to_SectorSize > device->Info.SectorSize) || (NumByteToRead_up_to_SectorSize == 0))
        NumByteToRead_up_to_SectorSize = device->Info.SectorSize;

    uint32_t StartPage;
    int32_t BytesToRead;
    uint32_t LocalOffset;

    if ((OffsetInByte + NumByteToRead_up_to_SectorSize) > device->Info.SectorSize)
        BytesToRead = device->Info.SectorSize - OffsetInByte;
    else
        BytesToRead = NumByteToRead_up_to_SectorSize;

    StartPage = W25QXX_SectorToPage(device, Sector_Address) + (OffsetInByte / device->Info.PageSize);

    LocalOffset = OffsetInByte % device->Info.PageSize;

    do
    {
        W25QXX_ReadPage(device, pBuffer, StartPage, LocalOffset, BytesToRead);
        StartPage++;
        BytesToRead -= device->Info.PageSize - LocalOffset;
        pBuffer += device->Info.PageSize - LocalOffset;
        LocalOffset = 0;
    }
    while (BytesToRead > 0);
}

/**
  * @brief  Read from a block
  * @param  device: Pointer to device instance
  * @param  pBuffer: Buffer to store data
  * @param  Block_Address: Block address
  * @param  OffsetInByte: Offset in bytes
  * @param  NumByteToRead_up_to_BlockSize: Number of bytes to read (up to block size)
  * @retval None
  */
void W25QXX_ReadBlock(W25QXX_Device_t *device, uint8_t *pBuffer, uint32_t Block_Address, uint32_t OffsetInByte, uint32_t NumByteToRead_up_to_BlockSize)
{
    if ((NumByteToRead_up_to_BlockSize > device->Info.BlockSize) || (NumByteToRead_up_to_BlockSize == 0))
        NumByteToRead_up_to_BlockSize = device->Info.BlockSize;

    uint32_t StartPage;
    int32_t BytesToRead;
    uint32_t LocalOffset;

    if ((OffsetInByte + NumByteToRead_up_to_BlockSize) > device->Info.BlockSize)
        BytesToRead = device->Info.BlockSize - OffsetInByte;
    else
        BytesToRead = NumByteToRead_up_to_BlockSize;

    StartPage = W25QXX_BlockToPage(device, Block_Address) + (OffsetInByte / device->Info.PageSize);

    LocalOffset = OffsetInByte % device->Info.PageSize;

    do
    {
        W25QXX_ReadPage(device, pBuffer, StartPage, LocalOffset, BytesToRead);
        StartPage++;
        BytesToRead -= device->Info.PageSize - LocalOffset;
        pBuffer += device->Info.PageSize - LocalOffset;
        LocalOffset = 0;
    }
    while (BytesToRead > 0);
}

/**
  * @brief  Reads settings from SPI Flash.
  * @param  device: Pointer to device instance
  * @param  settings: Pointer to settings structure
  * @retval None
  */
void W25QXX_ReadSettings(W25QXX_Device_t *device, Settings_t *settings)
{
    W25QXX_ReadSector(device, (uint8_t*)settings, 0, 0, sizeof(Settings_t));
}

/**
  * @brief  Writes settings to SPI Flash.
  * @param  device: Pointer to device instance
  * @param  settings: Settings structure to write
  * @retval None
  */
void W25QXX_WriteSettings(W25QXX_Device_t *device, Settings_t settings)
{
    uint32_t sizeData = sizeof(settings);
    uint8_t *ptrData = (uint8_t*)&settings;

    if (sizeData <= device->Info.PageSize) // 256 bytes
    {
        W25QXX_WritePage(device, ptrData, 0, 0, sizeData);
    }
    else if (sizeData <= device->Info.SectorSize) // 4096 bytes
    {
        W25QXX_WriteSector(device, ptrData, 0, 0, sizeData);
    }
    else if (sizeData <= device->Info.BlockSize) // 65536 bytes
    {
        W25QXX_WriteBlock(device, ptrData, 0, 0, sizeData);
    }
}

/**
  * @brief  Проверяет данные на указанном адресе во внешней Flash
  * @param  device: Указатель на устройство
  * @param  address: Адрес для проверки
  * @param  size: Размер данных для проверки (в байтах)
  * @retval 1 если данные обнаружены (не все 0xFF), 0 если данные пустые
  */
uint8_t W25QXX_CheckDataAtAddress(W25QXX_Device_t *device, uint32_t address, uint32_t size)
{
    uint8_t buffer[32]; // Буфер для проверки данных
    uint32_t checkSize = (size < sizeof(buffer)) ? size : sizeof(buffer);

    // Чтение блока данных для проверки
    W25QXX_ReadBytes(device, buffer, address, checkSize);

    // Проверка содержимого
    for (uint32_t i = 0; i < checkSize; i++) {
        if (buffer[i] != 0xFF) {
            return 1; // Найдены данные
        }
    }

    return 0; // Данные пустые (все 0xFF)
}

/**
  * @brief  Сбрасывает последнюю ошибку
  * @param  device: Указатель на устройство
  * @retval None
  */
void W25QXX_ClearLastStatus(W25QXX_Device_t *device)
{
    device->lastStatus = HAL_OK;
}

/**
  * @brief  Возвращает последний статус операции
  * @param  device: Указатель на устройство
  * @retval HAL_StatusTypeDef: Последний статус операции
  */
HAL_StatusTypeDef W25QXX_GetLastStatus(W25QXX_Device_t *device)
{
    return device->lastStatus;
}
