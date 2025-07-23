/**
 * @file firmware_update.cpp
 * @brief Реализация системы обновления прошивки через TCP
 */

#include "firmware_update.h"
#include "crc.h"
#include <cstdio>
//#include "err.h"

/* Приватные определения и глобальные переменные */
static flash* g_spiFlash = NULL;
static FirmwareUpdateContext g_updateContext;
static TaskHandle_t g_updateTaskHandle = NULL;
static uint8_t g_firmwareBuffer[FIRMWARE_BUFFER_SIZE];
static boot_data_t g_bootData;

/* Определение задачи обновления прошивки */
static void FirmwareUpdateTask(void *pvParameters);

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
 * @brief Запускает задачу обновления прошивки
 *
 * @return uint8_t Код ошибки или UPDATE_ERROR_NONE при успехе
 */
uint8_t FirmwareUpdate_StartTask(void) {
    // Проверяем, что задача еще не запущена
    if (g_updateTaskHandle != NULL) {
        return UPDATE_ERROR_BUSY;
    }

    // Проверяем, что модуль инициализирован
    if (g_spiFlash == NULL || g_updateContext.mutex == NULL) {
        return UPDATE_ERROR_INVALID_CMD;
    }

    // Создаем задачу обновления прошивки
    BaseType_t result = xTaskCreate(
        FirmwareUpdateTask,      // Функция задачи
        "FirmwareUpdate",        // Имя задачи
        512,                     // Размер стека в словах
        NULL,                    // Параметры задачи
		osPriorityNormal,    // Приоритет
        &g_updateTaskHandle      // Указатель на дескриптор задачи
    );

    if (result != pdPASS) {
        STM_LOG("Failed to create firmware update task");
        return UPDATE_ERROR_FLASH_FAILURE;
    }

    STM_LOG("Firmware update task started");
    return UPDATE_ERROR_NONE;
}

/**
 * @brief Начинает процесс обновления прошивки
 *
 * @param size Размер прошивки в байтах
 * @param version Версия прошивки
 * @return uint8_t Код ошибки или UPDATE_ERROR_NONE при успехе
 */
uint8_t FirmwareUpdate_StartUpdate(uint32_t size, uint32_t version) {
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
    g_updateContext.firmwareVersion = version;
    g_updateContext.firmwareCRC = 0;
    g_updateContext.declaredCRC = 0;

    xSemaphoreGive(g_updateContext.mutex);

    w25qxx_t flash_parametrs = g_spiFlash->getFlashParam();

    // Стираем соответствующую область SPI Flash
    uint32_t sectorsToErase = (size + flash_parametrs.SectorSize - 1) / flash_parametrs.SectorSize;

    STM_LOG("Starting firmware update. Size: %lu bytes, Version: 0x%08lX", size, version);
    STM_LOG("Erasing %lu sectors in SPI Flash", sectorsToErase);

    for (uint32_t i = 0; i < sectorsToErase; i++) {
        // Вычисляем адрес сектора относительно начала области прошивки
        uint32_t sectorAddr = SPI_FLASH_MAIN_FW_ADDRESS / flash_parametrs.SectorSize + i;
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
uint8_t FirmwareUpdate_StartBackupUpdate(uint32_t size, uint32_t version) {
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
    g_updateContext.firmwareVersion = version;
    g_updateContext.firmwareCRC = 0;
    g_updateContext.declaredCRC = 0;
    g_updateContext.isBackupUpdate = 1;  // Устанавливаем флаг резервного обновления

    xSemaphoreGive(g_updateContext.mutex);

    w25qxx_t flash_parametrs = g_spiFlash->getFlashParam();

    // Стираем соответствующую область SPI Flash для резервной прошивки
    uint32_t sectorsToErase = (size + flash_parametrs.SectorSize - 1) / flash_parametrs.SectorSize;

    STM_LOG("Starting backup firmware update. Size: %lu bytes, Version: 0x%08lX", size, version);
    STM_LOG("Erasing %lu sectors in SPI Flash", sectorsToErase);

    for (uint32_t i = 0; i < sectorsToErase; i++) {
        // Вычисляем адрес сектора относительно начала области резервной прошивки
        uint32_t sectorAddr = SPI_FLASH_BACKUP_FW_ADDRESS / flash_parametrs.SectorSize + i;
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
    uint32_t flashAddress = (g_updateContext.isBackupUpdate ? SPI_FLASH_BACKUP_FW_ADDRESS : SPI_FLASH_MAIN_FW_ADDRESS) + g_updateContext.receivedSize;
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

    // распечатать даамп прошивки из spi flash
    //DumpFirmwareHex(g_spiFlash, SPI_FLASH_MAIN_FW_ADDRESS, 1024, 16, 10);

    // Сохраняем значение CRC
    g_updateContext.declaredCRC = crc;
    g_updateContext.status = UPDATE_STATUS_COMPLETE;
    xSemaphoreGive(g_updateContext.mutex);

    STM_LOG("Firmware update completed successfully. Size: %lu bytes, CRC: 0x%08lX",
            g_updateContext.totalSize, g_updateContext.declaredCRC);

    // Подготавливаем структуру boot_data_t
    // Для этого сначала считываем текущие данные
    uint8_t bootDataBuffer[sizeof(boot_data_t)];
    uint32_t bootDataAddress = BOOT_DATA_ADDRESS;

    // Чтение текущих данных загрузки из внутренней флеш-памяти
    memcpy(&g_bootData, (void*)bootDataAddress, sizeof(boot_data_t));

    // Обновляем поля структуры
    if (g_updateContext.isBackupUpdate) {
        // Обновляем метаданные резервной прошивки
        g_bootData.backup_metadata.firmwareSize = g_updateContext.totalSize;
        g_bootData.backup_metadata.firmwareCRC = g_updateContext.declaredCRC;
        g_bootData.backup_metadata.firmwareVersion = g_updateContext.firmwareVersion;
        g_bootData.backup_metadata.update = 1; // Устанавливаем флаг, что требуется обновление

        // Обновляем данные о расположении резервной прошивки в SPI Flash
        g_bootData.SPI_Flash_Layout.BackupFirmwareAddress = SPI_FLASH_BACKUP_FW_ADDRESS;
        g_bootData.SPI_Flash_Layout.BackupFirmwareSize = g_updateContext.totalSize;
    } else {
        // Обновляем метаданные основной прошивки (существующий код)
        g_bootData.main_metadata.firmwareSize = g_updateContext.totalSize;
        g_bootData.main_metadata.firmwareCRC = g_updateContext.declaredCRC;
        g_bootData.main_metadata.firmwareVersion = g_updateContext.firmwareVersion;
        g_bootData.main_metadata.update = 1;

        // Обновляем данные о расположении основной прошивки в SPI Flash
        g_bootData.SPI_Flash_Layout.MainFirmwareAddress = SPI_FLASH_MAIN_FW_ADDRESS;
        g_bootData.SPI_Flash_Layout.MainFirmwareSize = g_updateContext.totalSize;
    }

    // Обновляем метаданные прошивки
    g_bootData.SPI_Flash_Firmware_Metadata.FirmwareVersion = g_updateContext.firmwareVersion;
    g_bootData.SPI_Flash_Firmware_Metadata.FirmwareSize = g_updateContext.totalSize;
    g_bootData.SPI_Flash_Firmware_Metadata.FirmwareCRC = g_updateContext.declaredCRC;
    g_bootData.SPI_Flash_Firmware_Metadata.TargetAddress = MAIN_PROGRAM_START_ADDRESS;

    // Рассчитываем CRC структуры (за исключением поля structCRC)
    //uint32_t structCRCValue = crc32_calculate(&g_bootData, offsetof(boot_data_t, structCRC), 0);
    uint32_t structCRCValue =  CRC_Calculate((uint8_t *)&g_bootData, offsetof(boot_data_t, structCRC)); // для загрузчика

    g_bootData.structCRC = structCRCValue;

    // Обновляем статус, что готовы к перезагрузке
    /*if (xSemaphoreTake(g_updateContext.mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        g_updateContext.status = UPDATE_STATUS_READY_REBOOT;
        xSemaphoreGive(g_updateContext.mutex);
    }

    STM_LOG("Writing boot data to internal flash...");*/

	HAL_FLASH_Unlock();

	// Стирание сектора BOOT_DATA_ADDRESS
	FLASH_EraseInitTypeDef eraseInit;
	eraseInit.TypeErase = FLASH_TYPEERASE_SECTORS;
	eraseInit.Sector = (BOOT_DATA_ADDRESS - FLASH_BASE) / FLASH_SECTOR_SIZE;
	eraseInit.NbSectors = 1;
	eraseInit.VoltageRange = FLASH_VOLTAGE_RANGE_3;

	uint32_t sectorError = 0;
	if (HAL_FLASHEx_Erase(&eraseInit, &sectorError) != HAL_OK) {
		STM_LOG("Failed to erase flash sector for boot data");
		return UPDATE_ERROR_FLASH_FAILURE;
	}

	// Запись структуры boot_data побайтово
	uint32_t *data = (uint32_t *)&g_bootData;
	uint32_t address = BOOT_DATA_ADDRESS;

	for (uint32_t i = 0; i < sizeof(boot_data_t) / 4; i++) {
		if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, data[i]) != HAL_OK) {
			STM_LOG("Failed to write boot data to flash");
			break;
		}
		address += 4;
	}

	HAL_FLASH_Lock();

	//STM_LOG("Boot data written. System will reboot in 5 seconds.");
	//vTaskDelay(pdMS_TO_TICKS(5000));

	// Перезагрузка системы
	//NVIC_SystemReset();

	// сбросить контекст
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
    g_updateContext.firmwareVersion = 0;
    g_updateContext.firmwareCRC = 0;
    g_updateContext.isBackupUpdate = 0;
    g_updateContext.declaredCRC = 0;

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
    g_updateContext.firmwareVersion = 0;
    g_updateContext.firmwareCRC = 0;
    g_updateContext.isBackupUpdate = 0;
    g_updateContext.declaredCRC = 0;

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
            if (i == (bytesPerLine / 2) - 1 && i + 1 < bytesToRead)
            {
                pos += snprintf(hexLine + pos, sizeof(hexLine) - pos, " ");
            }
        }

        // Добавляем пробелы для выравнивания при неполной строке
        for (uint32_t i = bytesToRead; i < bytesPerLine; i++)
        {
            pos += snprintf(hexLine + pos, sizeof(hexLine) - pos, "   ");
            if (i == (bytesPerLine / 2) - 1)
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

/**
 * @brief Задача обновления прошивки
 *
 * @param pvParameters Параметры задачи (не используются)
 */
static void FirmwareUpdateTask(void *pvParameters) {
    (void)pvParameters; // Подавляем предупреждение о неиспользуемом параметре

    STM_LOG("Firmware update task started");

    for (;;) {
        uint8_t status = FirmwareUpdate_GetStatus(NULL);

        if (status == UPDATE_STATUS_READY_REBOOT) {
            // Записываем структуру boot_data во внутреннюю флеш-память
            // Для STM32 нужно использовать HAL_FLASH_* функции


        }

        // Пауза перед следующей проверкой
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
