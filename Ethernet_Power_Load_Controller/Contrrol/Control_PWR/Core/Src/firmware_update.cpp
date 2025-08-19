/**
 * @file firmware_update.cpp
 * @brief Реализация системы обновления прошивки через TCP
 */


#include "crc.h"
#include <cstdio>
//#include "err.h"
#include "firmware_update.h"

/* Приватные определения и глобальные переменные */
static flash* g_spiFlash = NULL;
static FirmwareUpdateContext g_updateContext;
static TaskHandle_t g_updateTaskHandle = NULL;
static uint8_t g_firmwareBuffer[FIRMWARE_BUFFER_SIZE];
static boot_data_t g_bootData;

/**
 * @brief Инициализирует модуль обновления прошивки
 *
 * @param spiFlash Указатель на экземпляр класса flash для работы с SPI Flash
 */
void FirmwareUpdate_Init(flash* spiFlash) {
    // Сохраняем указатель на класс работы с SPI Flash
    g_spiFlash = spiFlash;

    // Инициализируем контекст обновления
    memset(&g_updateContext, 0, sizeof(g_updateContext));
    g_updateContext.status = UPDATE_STATUS_IDLE;
    g_updateContext.mutex = xSemaphoreCreateMutex();

    // Инициализируем буфер прошивки
    memset(g_firmwareBuffer, 0, FIRMWARE_BUFFER_SIZE);

    STM_LOG("Firmware Update module initialized");
    FirmwareUpdate_PrintMetadata();
}

/**
 * @brief Освобождает ресурсы модуля обновления
 */
void FirmwareUpdate_Deinit(void) {
    // Проверяем, что задача уже не запущена
    if (g_updateTaskHandle != NULL) {
        vTaskDelete(g_updateTaskHandle);
        g_updateTaskHandle = NULL;
    }

    // Освобождаем мьютекс
    if (g_updateContext.mutex != NULL) {
        vSemaphoreDelete(g_updateContext.mutex);
        g_updateContext.mutex = NULL;
    }

    g_spiFlash = NULL;
    STM_LOG("Firmware Update module deinitialized");
}

/**
 * @brief Получает текущий статус обновления
 *
 * @param error Указатель для возврата кода ошибки (может быть NULL)
 * @return uint8_t Текущий статус обновления
 */
uint32_t FirmwareUpdate_GetStatus(uint32_t* error) {
    uint32_t status;
    uint32_t err;

    // Получаем доступ к контексту через мьютекс
    if (xSemaphoreTake(g_updateContext.mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        status = g_updateContext.status;
        err = g_updateContext.error;
        xSemaphoreGive(g_updateContext.mutex);
    } else {
        status = UPDATE_STATUS_ERROR;
        err = UPDATE_ERROR_BUSY;
    }

    if (error != NULL) {
        *error = err;
    }

    return status;
}

/**
 * @brief Начинает процесс обновления прошивки
 *
 * @param size Размер прошивки в байтах
 * @param version Версия прошивки
 * @return uint8_t Код ошибки или UPDATE_ERROR_NONE при успехе
 */
uint8_t FirmwareUpdate_StartUpdate(uint32_t size, meta_t *metadata) {
    // Проверяем входные параметры
    if (metadata == NULL) {
        return UPDATE_ERROR_INVALID_PARAM;
    }

    // Проверяем возможность доступа к контексту
    if (xSemaphoreTake(g_updateContext.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return UPDATE_ERROR_BUSY;
    }

    // Проверяем, что текущий статус - IDLE
    if (g_updateContext.status != UPDATE_STATUS_IDLE) {
        xSemaphoreGive(g_updateContext.mutex);
        return UPDATE_ERROR_BUSY;
    }

    // Проверяем размер прошивки
    if (size == 0 || size > SPI_FLASH_MAIN_FW_SIZE) {
        xSemaphoreGive(g_updateContext.mutex);
        return UPDATE_ERROR_INVALID_SIZE;
    }

    // Инициализируем контекст для нового обновления
    g_updateContext.status = UPDATE_STATUS_IN_PROGRESS;
    g_updateContext.error = UPDATE_ERROR_NONE;
    g_updateContext.totalSize = size;
    g_updateContext.receivedSize = 0;
    g_updateContext.expectedBlockNumber = 0;
    g_updateContext.firmwareCRC = 0;
    g_updateContext.declaredCRC = 0;
    g_updateContext.fw_type = fw_type_t::Main; // Устанавливаем тип прошивки как основная
    g_updateContext.sectorAddress = SPI_FLASH_MAIN_FW_ADDRESS; // Устанавливаем начальный адрес для основной прошивки  
    g_updateContext.metadata = *metadata; // Сохраняем метаданные прошивки

    xSemaphoreGive(g_updateContext.mutex);

    w25qxx_t flash_parametrs = g_spiFlash->getFlashParam();

    // Стираем соответствующую область SPI Flash
    uint32_t sectorsToErase = (size + flash_parametrs.SectorSize - 1) / flash_parametrs.SectorSize;

    STM_LOG("Starting firmware update. Size: %lu bytes, Version: 0x%08lX", size, metadata->version);
    STM_LOG("Erasing %lu sectors in SPI Flash", sectorsToErase);

    for (uint32_t i = 0; i < sectorsToErase; i++) {
        // Вычисляем адрес сектора относительно начала области прошивки
        uint32_t sectorAddr = g_updateContext.sectorAddress / flash_parametrs.SectorSize + i;
        g_spiFlash->W25qxx_EraseSector(sectorAddr);

        // Проверяем отмену операции после каждого сектора
        if (xSemaphoreTake(g_updateContext.mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            if (g_updateContext.status != UPDATE_STATUS_IN_PROGRESS) {
                xSemaphoreGive(g_updateContext.mutex);
                STM_LOG("Firmware update aborted during sector erase");
                return UPDATE_ERROR_ABORT;
            }
            xSemaphoreGive(g_updateContext.mutex);
        }
    }

    STM_LOG("SPI Flash erased, ready to receive firmware");
    return UPDATE_ERROR_NONE;
}

/**
 * @brief Начинает процесс обновления резервной прошивки
 *
 * @param size Размер прошивки в байтах
 * @param version Версия прошивки
 * @return uint8_t Код ошибки или UPDATE_ERROR_NONE при успехе
 */
uint8_t FirmwareUpdate_StartBackupUpdate(uint32_t size, meta_t *metadata) {
    // Проверяем возможность доступа к контексту
    if (xSemaphoreTake(g_updateContext.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return UPDATE_ERROR_BUSY;
    }

    // Проверяем, что текущий статус - IDLE
    if (g_updateContext.status != UPDATE_STATUS_IDLE) {
        xSemaphoreGive(g_updateContext.mutex);
        return UPDATE_ERROR_BUSY;
    }

    // Проверяем размер прошивки
    if (size == 0 || size > SPI_FLASH_BACKUP_FW_SIZE) {
        xSemaphoreGive(g_updateContext.mutex);
        return UPDATE_ERROR_INVALID_SIZE;
    }

    // Инициализируем контекст для нового обновления резервной прошивки
    g_updateContext.status = UPDATE_STATUS_IN_PROGRESS;
    g_updateContext.error = UPDATE_ERROR_NONE;
    g_updateContext.totalSize = size;
    g_updateContext.receivedSize = 0;
    g_updateContext.expectedBlockNumber = 0;
    g_updateContext.firmwareCRC = 0;
    g_updateContext.declaredCRC = 0;
    g_updateContext.fw_type = fw_type_t::Backup; // Устанавливаем тип прошивки как резервная
    g_updateContext.sectorAddress = SPI_FLASH_BACKUP_FW_ADDRESS; // Устанавливаем начальный адрес для резервной прошивки
    g_updateContext.metadata = *metadata; // Сохраняем метаданные резервной прошивки

    xSemaphoreGive(g_updateContext.mutex);

    w25qxx_t flash_parametrs = g_spiFlash->getFlashParam();

    // Стираем соответствующую область SPI Flash для резервной прошивки
    uint32_t sectorsToErase = (size + flash_parametrs.SectorSize - 1) / flash_parametrs.SectorSize;

    STM_LOG("Starting backup firmware update. Size: %lu bytes, Version: 0x%08lX", size, metadata->version);
    STM_LOG("Erasing %lu sectors in SPI Flash", sectorsToErase);

    for (uint32_t i = 0; i < sectorsToErase; i++) {
        // Вычисляем адрес сектора относительно начала области резервной прошивки
        uint32_t sectorAddr = g_updateContext.sectorAddress / flash_parametrs.SectorSize + i;
        g_spiFlash->W25qxx_EraseSector(sectorAddr);

        // Проверяем отмену операции после каждого сектора
        if (xSemaphoreTake(g_updateContext.mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            if (g_updateContext.status != UPDATE_STATUS_IN_PROGRESS) {
                xSemaphoreGive(g_updateContext.mutex);
                STM_LOG("Backup firmware update aborted during sector erase");
                return UPDATE_ERROR_ABORT;
            }
            xSemaphoreGive(g_updateContext.mutex);
        }
    }

    STM_LOG("SPI Flash erased, ready to receive backup firmware");
    return UPDATE_ERROR_NONE;
}

/**
 * @brief Обрабатывает блок данных прошивки
 *
 * @param blockNumber Номер блока данных
 * @param data Указатель на данные
 * @param size Размер данных
 * @param crc CRC блока данных
 * @return uint8_t Код ошибки или UPDATE_ERROR_NONE при успехе
 */
uint8_t FirmwareUpdate_ProcessDataBlock(uint32_t blockNumber, const uint8_t* data, uint32_t size, uint32_t crc) {
    // Проверяем возможность доступа к контексту
    if (xSemaphoreTake(g_updateContext.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return UPDATE_ERROR_BUSY;
    }

    // Проверяем, что текущий статус - IN_PROGRESS
    if (g_updateContext.status != UPDATE_STATUS_IN_PROGRESS) {
        xSemaphoreGive(g_updateContext.mutex);
        return UPDATE_ERROR_SEQ_ERROR;
    }

    // Проверяем номер блока
    if (blockNumber != g_updateContext.expectedBlockNumber) {
        STM_LOG("Block sequence error. Expected: %lu, Got: %lu",
                g_updateContext.expectedBlockNumber, blockNumber);
        xSemaphoreGive(g_updateContext.mutex);
        return UPDATE_ERROR_SEQ_ERROR;
    }

    // Проверяем размер блока
    if (size == 0 || size > FIRMWARE_BUFFER_SIZE ||
        g_updateContext.receivedSize + size > g_updateContext.totalSize) {
        xSemaphoreGive(g_updateContext.mutex);
        return UPDATE_ERROR_INVALID_SIZE;
    }

    // Вычисляем и проверяем CRC блока
    uint32_t calculatedCRC = crc32_calculate(data, size, 0);
    if (calculatedCRC != crc) {
        STM_LOG("Block CRC mismatch. Expected: 0x%08lX, Calculated: 0x%08lX", crc, calculatedCRC);
        xSemaphoreGive(g_updateContext.mutex);
        return UPDATE_ERROR_CRC_MISMATCH;
    }

    // Обновляем CRC всей прошивки
    g_updateContext.firmwareCRC = crc32_calculate(data, size, g_updateContext.firmwareCRC);

    // Рассчитываем адрес в SPI Flash
    //uint32_t flashAddress = SPI_FLASH_MAIN_FW_ADDRESS + g_updateContext.receivedSize;
    uint32_t flashAddress = (g_updateContext.sectorAddress + g_updateContext.receivedSize);
    uint32_t pageAddress = flashAddress / 256;  // 256 - размер страницы в W25Qxx
    uint32_t offsetInPage = flashAddress % 256;

    // Копируем данные во временный буфер
    memcpy(g_firmwareBuffer, data, size);

    // Освобождаем мьютекс перед продолжительной операцией записи
    g_updateContext.expectedBlockNumber++;
    g_updateContext.receivedSize += size;
    xSemaphoreGive(g_updateContext.mutex);

    // Записываем данные в SPI Flash
    g_spiFlash->W25qxx_WritePage(g_firmwareBuffer, pageAddress, offsetInPage, size);

    // Проверяем успешность записи
    uint8_t verifyBuffer[FIRMWARE_BUFFER_SIZE];
    g_spiFlash->W25qxx_ReadPage(verifyBuffer, pageAddress, offsetInPage, size);

    if (memcmp(g_firmwareBuffer, verifyBuffer, size) != 0) {
        STM_LOG("Firmware block write verification failed");

        if (xSemaphoreTake(g_updateContext.mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            g_updateContext.status = UPDATE_STATUS_ERROR;
            g_updateContext.error = UPDATE_ERROR_FLASH_FAILURE;
            xSemaphoreGive(g_updateContext.mutex);
        }

        return UPDATE_ERROR_FLASH_FAILURE;
    }

    return UPDATE_ERROR_NONE;
}

/**
 * @brief Завершает процесс обновления прошивки
 *
 * @param crc Ожидаемое значение CRC всей прошивки
 * @return uint8_t Код ошибки или UPDATE_ERROR_NONE при успехе
 */
uint8_t FirmwareUpdate_EndUpdate(uint32_t crc) {
    // Проверяем возможность доступа к контексту
    if (xSemaphoreTake(g_updateContext.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return UPDATE_ERROR_BUSY;
    }

    // Проверяем, что текущий статус - IN_PROGRESS
    if (g_updateContext.status != UPDATE_STATUS_IN_PROGRESS) {
        xSemaphoreGive(g_updateContext.mutex);
        return UPDATE_ERROR_SEQ_ERROR;
    }

    // Проверяем размер полученной прошивки
    if (g_updateContext.receivedSize != g_updateContext.totalSize) {
        STM_LOG("Firmware size mismatch. Expected: %lu, Received: %lu", 
                g_updateContext.totalSize, g_updateContext.receivedSize);
        g_updateContext.status = UPDATE_STATUS_ERROR;
        g_updateContext.error = UPDATE_ERROR_INVALID_SIZE;
        xSemaphoreGive(g_updateContext.mutex);
        return UPDATE_ERROR_INVALID_SIZE;
    }

    // Проверяем CRC всей прошивки
    if (g_updateContext.firmwareCRC != crc) {
        STM_LOG("Firmware CRC mismatch. Expected: 0x%08lX, Calculated: 0x%08lX", 
                crc, g_updateContext.firmwareCRC);
        g_updateContext.status = UPDATE_STATUS_ERROR;
        g_updateContext.error = UPDATE_ERROR_CRC_MISMATCH;
        xSemaphoreGive(g_updateContext.mutex);
        return UPDATE_ERROR_CRC_MISMATCH;
    }

    // Сохраняем значение CRC
    g_updateContext.declaredCRC = crc;
    g_updateContext.status = UPDATE_STATUS_COMPLETE;

    STM_LOG("Firmware update completed successfully. Size: %lu bytes, CRC: 0x%08lX, Address: 0x%08lX",
            g_updateContext.totalSize, g_updateContext.declaredCRC, g_updateContext.sectorAddress);
    // Если это обновление не для вторичных плат
    if (g_updateContext.fw_type != fw_type_t::Secondary) {
        // Подготавливаем структуру boot_data_t
        uint32_t bootDataAddress = BOOT_DATA_ADDRESS;
        memcpy(&g_bootData, (void*)bootDataAddress, sizeof(boot_data_t));

        // Обновляем поля структуры в зависимости от типа прошивки
        if (g_updateContext.fw_type == fw_type_t::Backup) {
			g_bootData.backup_metadata.firmwareSize = g_updateContext.totalSize;
			g_bootData.backup_metadata.firmwareCRC = g_updateContext.declaredCRC;
			g_bootData.backup_metadata.firmwareVersion = g_updateContext.metadata.version;
			g_bootData.backup_metadata.update = 1;
            g_bootData.SPI_Flash_Layout.BackupFirmwareAddress = SPI_FLASH_BACKUP_FW_ADDRESS;
            g_bootData.SPI_Flash_Layout.BackupFirmwareSize = g_updateContext.totalSize;
        } else {
			g_bootData.main_metadata.firmwareSize = g_updateContext.totalSize;
			g_bootData.main_metadata.firmwareCRC = g_updateContext.declaredCRC;
			g_bootData.main_metadata.firmwareVersion = g_updateContext.metadata.version;
			g_bootData.main_metadata.update = 1;
            g_bootData.SPI_Flash_Layout.MainFirmwareAddress = SPI_FLASH_MAIN_FW_ADDRESS;
            g_bootData.SPI_Flash_Layout.MainFirmwareSize = g_updateContext.totalSize;
        }

        // Обновляем общие метаданные прошивки
		g_bootData.SPI_Flash_Firmware_Metadata.FirmwareVersion = g_updateContext.metadata.version;
		g_bootData.SPI_Flash_Firmware_Metadata.FirmwareSize = g_updateContext.totalSize;
		g_bootData.SPI_Flash_Firmware_Metadata.FirmwareCRC = g_updateContext.declaredCRC;
		g_bootData.SPI_Flash_Firmware_Metadata.TargetAddress = MAIN_PROGRAM_START_ADDRESS;

        // Рассчитываем CRC структуры
        g_bootData.structCRC = CRC_Calculate((uint8_t *)&g_bootData, offsetof(boot_data_t, structCRC));

        // Записываем boot_data во флеш
        HAL_StatusTypeDef status = HAL_ERROR;
        HAL_FLASH_Unlock();

        FLASH_EraseInitTypeDef eraseInit = {
            .TypeErase = FLASH_TYPEERASE_SECTORS,
            .Sector = (BOOT_DATA_ADDRESS - FLASH_BASE) / FLASH_SECTOR_SIZE,
            .NbSectors = 1,
            .VoltageRange = FLASH_VOLTAGE_RANGE_3
        };

        uint32_t sectorError = 0;
        if (HAL_FLASHEx_Erase(&eraseInit, &sectorError) == HAL_OK) {
            uint32_t *data = (uint32_t *)&g_bootData;
            uint32_t address = BOOT_DATA_ADDRESS;
            status = HAL_OK;

            for (uint32_t i = 0; i < sizeof(boot_data_t) / 4 && status == HAL_OK; i++) {
                status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, data[i]);
                address += 4;
            }
        }

        HAL_FLASH_Lock();

        if (status != HAL_OK) {
            STM_LOG("Failed to write boot data to flash");
            g_updateContext.status = UPDATE_STATUS_ERROR;
            g_updateContext.error = UPDATE_ERROR_FLASH_FAILURE;
            xSemaphoreGive(g_updateContext.mutex);
            return UPDATE_ERROR_FLASH_FAILURE;
        }
    }

    // Сбрасываем контекст обновления
    g_updateContext.status = UPDATE_STATUS_IDLE;
    g_updateContext.error = UPDATE_ERROR_NONE;
    g_updateContext.totalSize = 0;
    g_updateContext.receivedSize = 0;
    g_updateContext.expectedBlockNumber = 0;
    g_updateContext.firmwareCRC = 0;
    g_updateContext.declaredCRC = 0;
    g_updateContext.fw_type = fw_type_t::Main;
    g_updateContext.sectorAddress = 0;
    memset(&g_updateContext.metadata, 0, sizeof(meta_t));

    xSemaphoreGive(g_updateContext.mutex);
    return UPDATE_ERROR_NONE;
}

/**
 * @brief Отменяет процесс обновления прошивки
 *
 * @return uint8_t Код ошибки или UPDATE_ERROR_NONE при успехе
 */
uint8_t FirmwareUpdate_AbortUpdate(void) {
    // Проверяем возможность доступа к контексту
    if (xSemaphoreTake(g_updateContext.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return UPDATE_ERROR_BUSY;
    }

    // Проверяем, что можно отменить
    if (g_updateContext.status != UPDATE_STATUS_IN_PROGRESS &&
        g_updateContext.status != UPDATE_STATUS_COMPLETE) {
        xSemaphoreGive(g_updateContext.mutex);
        return UPDATE_ERROR_SEQ_ERROR;
    }

    // Сбрасываем контекст
    g_updateContext.status = UPDATE_STATUS_IDLE;
    g_updateContext.error = UPDATE_ERROR_ABORT;
    g_updateContext.totalSize = 0;
    g_updateContext.receivedSize = 0;
    g_updateContext.expectedBlockNumber = 0;
    g_updateContext.firmwareCRC = 0;
    g_updateContext.declaredCRC = 0;
    g_updateContext.fw_type = fw_type_t::Main;
    g_updateContext.sectorAddress = 0;
    memset(&g_updateContext.metadata, 0, sizeof(meta_t));

    xSemaphoreGive(g_updateContext.mutex);

    STM_LOG("Firmware update aborted");
    return UPDATE_ERROR_NONE;
}

/**
 * @brief Выводит содержимое прошивки из SPI Flash в HEX-формате через UART
 *
 * @param spiFlash Указатель на объект flash для работы с SPI Flash
 * @param startAddress Начальный адрес для чтения в SPI Flash
 * @param length Количество байт для вывода
 * @param bytesPerLine Количество байт в одной строке вывода (обычно 16)
 * @param delayMs Задержка между строками в миллисекундах
 */
void DumpFirmwareHex(flash* spiFlash, uint32_t startAddress, uint32_t length, uint8_t bytesPerLine, uint32_t delayMs)
{
    uint8_t buffer[32]; // Буфер для чтения данных (максимум 32 байта за раз)
    char hexLine[128];  // Буфер для форматированной строки вывода

    if (bytesPerLine > 32) bytesPerLine = 32; // Ограничение для безопасности
    if (bytesPerLine < 1) bytesPerLine = 16;  // Значение по умолчанию

    STM_LOG("Дамп прошивки из SPI Flash:");
    STM_LOG("Адрес начала: 0x%08lX, Длина: %lu байт", startAddress, length);
    STM_LOG("--------------------------------------------------");

    for (uint32_t offset = 0; offset < length; offset += bytesPerLine)
    {
        uint32_t currentAddress = startAddress + offset;
        uint32_t bytesToRead = (offset + bytesPerLine <= length) ? bytesPerLine : (length - offset);

        // Чтение блока данных из SPI Flash
        spiFlash->W25qxx_ReadBytes(buffer, currentAddress, bytesToRead);

        // Форматирование адреса
        int pos = snprintf(hexLine, sizeof(hexLine), "0x%08lX: ", currentAddress);

        // Форматирование HEX-значений
        for (uint32_t i = 0; i < bytesToRead; i++)
        {
            pos += snprintf(hexLine + pos, sizeof(hexLine) - pos, "%02X ", buffer[i]);

            // Добавляем дополнительный пробел в середине строки для лучшей читаемости
            if (i == ((uint32_t)bytesPerLine / 2) - 1 && i + 1 < bytesToRead)
            {
                pos += snprintf(hexLine + pos, sizeof(hexLine) - pos, " ");
            }
        }

        // Добавляем пробелы для выравнивания при неполной строке
        for (uint32_t i = bytesToRead; i < bytesPerLine; i++)
        {
            pos += snprintf(hexLine + pos, sizeof(hexLine) - pos, "   ");
            if (i == ((uint32_t)bytesPerLine / 2) - 1)
            {
                pos += snprintf(hexLine + pos, sizeof(hexLine) - pos, " ");
            }
        }

        // Добавляем ASCII-представление
        pos += snprintf(hexLine + pos, sizeof(hexLine) - pos, " | ");
        for (uint32_t i = 0; i < bytesToRead; i++)
        {
            char ch = (buffer[i] >= 32 && buffer[i] <= 126) ? buffer[i] : '.';
            pos += snprintf(hexLine + pos, sizeof(hexLine) - pos, "%c", ch);
        }

        // Вывод строки
        STM_LOG("%s", hexLine);

        // Задержка между строками
        if (delayMs > 0)
        {
            osDelay(delayMs);
        }

        // Дополнительный вывод заголовка каждые 16 строк для удобства чтения
        if ((offset / bytesPerLine) % 16 == 15 && offset + bytesPerLine < length)
        {
            STM_LOG("--------------------------------------------------");
            STM_LOG("Адрес       : 00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F | ASCII");
            STM_LOG("--------------------------------------------------");
        }
    }

    STM_LOG("--------------------------------------------------");
    STM_LOG("Дамп завершен");
}

/* Функция которая проходит по всем адресам 
SPI_FLASH_MAIN_FW_ADDRESS
SPI_FLASH_BACKUP_FW_ADDRESS
SPI_FIRMWARE_S1_ADDRESS
SPI_FIRMWARE_S2_ADDRESS
SPI_FIRMWARE_S3_ADDRESS
SPI_FIRMWARE_S4_ADDRESS
SPI_FIRMWARE_S5_ADDRESS
SPI_FIRMWARE_S6_ADDRESS
Считывает метаданные из этих секторов и если там есть прошивка то распечатывает метаданные
*/

void FirmwareUpdate_PrintMetadata(void)
{
    uint32_t addresses[] = {
        SPI_FLASH_MAIN_FW_ADDRESS,
        SPI_FLASH_BACKUP_FW_ADDRESS,
        SPI_FIRMWARE_S1_ADDRESS,
        SPI_FIRMWARE_S2_ADDRESS,
        SPI_FIRMWARE_S3_ADDRESS,
        SPI_FIRMWARE_S4_ADDRESS,
        SPI_FIRMWARE_S5_ADDRESS,
        SPI_FIRMWARE_S6_ADDRESS
    };

    for (uint32_t addr : addresses)
    {
        meta_t metadata;
        if (FirmwareUpdate_GetSecondaryMetaData(addr, &metadata) == 0)
        {
            STM_LOG("Found firmware metadata at 0x%08lX: Version: 0x%08lX, Name: %s", addr, metadata.version, metadata.name_proj);
        }
        else
        {
            STM_LOG("No firmware metadata found at 0x%08lX", addr);
        }
    }
}

/*******************************************************************************************************************************
Блок обновление прошивки вторичных плат
********************************************************************************************************************************/

uint8_t FirmwareUpdate_StartSecondaryUpdate(uint32_t size, meta_t *metadata) {
    // Проверяем возможность доступа к контексту
    if (xSemaphoreTake(g_updateContext.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return UPDATE_ERROR_BUSY;
    }

    // Проверяем, что размер и версия корректны
    if (size == 0 || metadata == NULL) {
        xSemaphoreGive(g_updateContext.mutex);
        return UPDATE_ERROR_INVALID_SIZE;
    }

    // получить адрес сектора для записи прошивки вторичной платы
    uint32_t sectorAddress = FirmwareUpdate_FindSectorForSecondaryFirmware(metadata);

    if (sectorAddress == 0xFFFFFFFF) {
        // не нашли ни одного сектора для записи прошивки вторичной платы
        xSemaphoreGive(g_updateContext.mutex);
        return UPDATE_ERROR_NO_SECTOR_FOUND;
    }
    // Инициализируем контекст для нового обновления вторичной платы
    g_updateContext.status = UPDATE_STATUS_IN_PROGRESS;
    g_updateContext.error = UPDATE_ERROR_NONE;
    g_updateContext.totalSize = size;
    g_updateContext.receivedSize = 0;
    g_updateContext.expectedBlockNumber = 0;
    g_updateContext.firmwareCRC = 0;
    g_updateContext.declaredCRC = 0;
    g_updateContext.sectorAddress = sectorAddress; // сохраняем адрес сектора для записи прошивки вторичной платы
    g_updateContext.fw_type = fw_type_t::Secondary; // Устанавливаем тип прошивки как вторичная
    g_updateContext.metadata = *metadata; // Сохраняем метаданные прошивки вторичной платы

    // вычисляем и очищаем место на флеш памяти для записи прошивки вторичной платы
    w25qxx_t flash_parametrs = g_spiFlash->getFlashParam();
    uint32_t sectorsToErase = (size + flash_parametrs.SectorSize - 1) / flash_parametrs.SectorSize;
    STM_LOG("Starting secondary firmware update. Size: %lu bytes, Version: 0x%08lX", size, metadata->version);
    STM_LOG("Erasing %lu sectors in SPI Flash for secondary firmware", sectorsToErase);
    for (uint32_t i = 0; i < sectorsToErase; i++) {
        // Вычисляем адрес сектора относительно начала области прошивки вторичной платы
        uint32_t sectorAddr = g_updateContext.sectorAddress / flash_parametrs.SectorSize + i;
        g_spiFlash->W25qxx_EraseSector(sectorAddr);
        // Проверяем отмену операции после каждого сектора
        if (xSemaphoreTake(g_updateContext.mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            if (g_updateContext.status != UPDATE_STATUS_IN_PROGRESS) {
                xSemaphoreGive(g_updateContext.mutex);
                STM_LOG("Secondary firmware update aborted during sector erase");
                return UPDATE_ERROR_ABORT;
            }
            xSemaphoreGive(g_updateContext.mutex);
        }
    }

    STM_LOG("SPI Flash erased for secondary firmware, ready to receive data");
    xSemaphoreGive(g_updateContext.mutex);
    return UPDATE_ERROR_NONE;
}

// функция для поиска свободного сектора в SPI Flash для записи прошивки вторичной платы
// принимает указатель на метаданные прошивки вторичной платы
// возвращает адрес сектора в SPI Flash, где можно записать прошивку вторичной платы
// если не найдено ни одного свободного сектора, возвращает 0xFFFFFFFF
// если указатель на метаданные NULL, возвращает 0xFFFFFFFF
uint32_t FirmwareUpdate_FindSectorForSecondaryFirmware(meta_t *metadata) {
    // Проверяем, что указатель на метаданные не NULL
    if (metadata == NULL) {
        return 0xFFFFFFFF; // некорректный указатель
    }

    // Поиск сектора в SPI Flash
    for (uint32_t sector = SPI_FIRMWARE_S1_ADDRESS; sector < SPI_FIRMWARE_S6_ADDRESS; sector += SPI_FIRMWARE_SIZE) {
        meta_t meta;
        if (FirmwareUpdate_GetSecondaryMetaData(sector, &meta) == 0) {
            // Сравнение имени проекта
            if (strcmp((const char*)metadata->name_proj, (const char*)meta.name_proj) == 0) {
                return sector; // Найден совпадающий сектор
            }
        }
        else // Если метаданные не прочитаны, значит сектор пустой или ошибка чтения можно использовать этот сектор для записи прошивки вторичной платы
        {
            return sector;
        }
        
    }

    // Если не найдено ни одного сектора, возвращаем 0xFFFFFFFF
    return 0xFFFFFFFF;
}

// функция для получения метаданных прошивки вторичной платы из SPI Flash
// принимает адрес сектора в SPI Flash и заполняет структуру meta_t
// метаданные смещенны от начала прошивка на 512 байта. в секторе хранится одна прошивка вторичной платы адрес сектора в SPI Flash это начало прошивки
// возвращает 0 - если успешно, 1 - если ошибка чтения
uint8_t FirmwareUpdate_GetSecondaryMetaData(uint32_t sectorAddress, meta_t* metadata) {
	/*
    if (sectorAddress < SPI_FIRMWARE_S1_ADDRESS || sectorAddress >= SPI_FIRMWARE_S6_ADDRESS) {
        return 1; // некорректный адрес сектора
    }*/

    if (metadata == NULL) {
        return 1; // указатель на метаданные не должен быть NULL
    }

    // Чтение метаданных из SPI Flash с учетом смещения 512 байт
    uint8_t buffer[sizeof(meta_t)];
    g_spiFlash->W25qxx_ReadBytes(buffer, sectorAddress + 512, sizeof(meta_t));

    meta_t readMetadata;
    memcpy(&readMetadata, buffer, sizeof(meta_t));

    // Проверка ключа начала метаданных
    if (readMetadata.key_start != METADATA_KEY) {
        return 1; // ошибка чтения, ключ не совпадает
    }
    // версия не 0 и не ff
    if (readMetadata.version == 0 || readMetadata.version == 0xFFFFFFFF) {
        return 1; // ошибка чтения, версия некорректна
    }
    // Проверка имя проекта строка не должна быть пустой
    if (strlen((const char*)readMetadata.name_proj) == 0) {
        return 1; // ошибка чтения, имя проекта пустое
    }
    // Копируем метаданные в выходную структуру
    memcpy(metadata, &readMetadata, sizeof(meta_t));

    return 0; // успешно
}
