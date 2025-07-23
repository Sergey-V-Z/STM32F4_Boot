#include "flash_nvs.h"

FlashNVS::FlashNVS(flash* driver) : flashDriver(driver) {
    memset(&cache, 0, sizeof(cache));
    memset(sectorBuffer, 0xFF, SECTOR_SIZE);
}

FlashNVS::~FlashNVS() {
}

void FlashNVS::loadSectorToBuffer(uint32_t sector) {
    // Читаем сектор во временный буфер
    memset(sectorBuffer, 0xFF, SECTOR_SIZE);
    flashDriver->W25qxx_ReadSector(sectorBuffer, sector, 0, SECTOR_SIZE);

    // Обновляем кэш заголовка
    PageHeader* header = (PageHeader*)sectorBuffer;
    STM_LOG("loadSectorToBuffer: sector=%lu, magic=0x%08lX, count=%d, crc=0x%04X",
            sector, header->magicNumber, header->count, header->crc);
}

void FlashNVS::saveSectorFromBuffer(uint32_t sector) {
    // Стираем сектор
    flashDriver->W25qxx_EraseSector(sector);

    // Делаем небольшую паузу после стирания
    HAL_Delay(1);

    // Записываем данные из буфера
    flashDriver->W25qxx_WriteSector(sectorBuffer, sector, 0, SECTOR_SIZE);

    // Делаем паузу после записи
    HAL_Delay(1);

    // Проверяем записанные данные
    uint8_t* verifyBuffer = new uint8_t[SECTOR_SIZE];
    if (verifyBuffer) {
        flashDriver->W25qxx_ReadSector(verifyBuffer, sector, 0, SECTOR_SIZE);

        if (memcmp(verifyBuffer, sectorBuffer, SECTOR_SIZE) != 0) {
            STM_LOG("Error: sector %lu verification failed, retrying", sector);

            // Повторная попытка записи
            flashDriver->W25qxx_EraseSector(sector);
            HAL_Delay(1);
            flashDriver->W25qxx_WriteSector(sectorBuffer, sector, 0, SECTOR_SIZE);
            HAL_Delay(1);

            // Повторная проверка
            flashDriver->W25qxx_ReadSector(verifyBuffer, sector, 0, SECTOR_SIZE);
            if (memcmp(verifyBuffer, sectorBuffer, SECTOR_SIZE) != 0) {
                STM_LOG("Error: sector %lu verification failed after retry", sector);
            } else {
                STM_LOG("Sector %lu verified successfully after retry", sector);
            }
        } else {
            STM_LOG("Sector %lu verified successfully", sector);
        }

        delete[] verifyBuffer;
    }
}

FlashNVS::ErrorCode FlashNVS::init(uint32_t startSector) {
    currentSector = startSector;
    dataSector = startSector + 1;

    // Читаем заголовок страницы
    loadSectorToBuffer(currentSector);
    memcpy(&cache.header, sectorBuffer, sizeof(PageHeader));
    
    STM_LOG("Init: Read header from sector %lu", currentSector);

    // Проверяем валидность страницы
    if (!isPageValid(&cache.header)) {
        STM_LOG("Init: Invalid page, creating new");

        memset(&cache.header, 0, sizeof(PageHeader));
        cache.header.magicNumber = MAGIC_NUMBER;
        cache.header.count = 0;
        cache.header.sectorNumber = currentSector;
        cache.header.crc = calculateCRC(&cache.header, offsetof(PageHeader, crc));

        memset(sectorBuffer, 0xFF, SECTOR_SIZE);
        memcpy(sectorBuffer, &cache.header, sizeof(PageHeader));

        saveSectorFromBuffer(currentSector);
        STM_LOG("Init: New page created with CRC=0x%04X", cache.header.crc);
    }
    else if (cache.header.count > MAX_ENTRIES_PER_PAGE/2) {
        // Если занято больше половины записей, выполняем уплотнение
        STM_LOG("Init: Too many entries (%d), performing compaction", cache.header.count);
        ErrorCode err = compactSector();
        if (err != OK) {
            STM_LOG("Init: Compaction failed with error %d", err);
            return err;
        }
        STM_LOG("Init: Compaction completed, entries reduced to %d", cache.header.count);
    }

    STM_LOG("Init: Sector ready, entries: %d", cache.header.count);
    return OK;
}

uint32_t FlashNVS::findNextFreeSector() {
    uint8_t tempBuffer[256];  // Используем маленький буфер для проверки начала сектора

    STM_LOG("Searching for free sector starting from %lu", dataSector);

    for (uint32_t sector = dataSector; sector < dataSector + 16; sector++) {
        // Читаем начало сектора
        flashDriver->W25qxx_ReadSector(tempBuffer, sector, 0, sizeof(tempBuffer));

        bool isEmpty = true;
        // Проверяем, содержит ли сектор только 0xFF
        for (uint32_t i = 0; i < sizeof(tempBuffer); i++) {
            if (tempBuffer[i] != 0xFF) {
                isEmpty = false;
                break;
            }
        }

        if (isEmpty) {
            STM_LOG("Found free sector: %lu", sector);
            return sector;
        }
    }

    STM_LOG("No free sectors found");
    return UINT32_MAX;
}

FlashNVS::ErrorCode FlashNVS::setString(const char* key, const char* value) {
    size_t length = strlen(value) + 1;
    return writeEntry(key, value, length, TYPE_STR);
}

FlashNVS::ErrorCode FlashNVS::getString(const char* key, char* out_value, uint32_t* length) {
    Entry entry;
    uint32_t offset;
    ErrorCode err = findEntry(key, &entry, &offset);

    if (err != OK) return err;
    if (entry.type != TYPE_STR) return ERROR_TYPE_MISMATCH;

    return readEntry(&entry, out_value, length);
}

FlashNVS::ErrorCode FlashNVS::setBlob(const char* key, const void* data, uint32_t length) {
    STM_LOG("Writing blob with key '%s', size: %lu bytes", key, length);

    if (!data || !key || length == 0) {
        STM_LOG("Invalid parameters");
        return ERROR_INVALID_SIZE;
    }

    // Читаем весь сектор в буфер
    loadSectorToBuffer(currentSector);

    // Ищем существующую запись с таким ключом
    bool entryExists = false;
    uint32_t existingEntryIndex = 0;
    uint32_t existingDataOffset = sizeof(PageHeader) + MAX_ENTRIES_PER_PAGE * sizeof(Entry);
    uint32_t entryOffset = sizeof(PageHeader);

    STM_LOG("Searching for existing entry with key '%s'", key);

    for (uint16_t i = 0; i < cache.header.count; i++) {
        const Entry* currentEntry = (const Entry*)(sectorBuffer + entryOffset);
        if (strncmp(currentEntry->key, key, NVS_KEY_MAX_LENGTH) == 0) {
            entryExists = true;
            existingEntryIndex = i;
            STM_LOG("Found existing entry at index %lu", existingEntryIndex);
            break;
        }
        entryOffset += sizeof(Entry);
        existingDataOffset += currentEntry->length;
    }

    // Готовим новую запись
    Entry newEntry;
    memset(&newEntry, 0, sizeof(Entry));
    strncpy(newEntry.key, key, sizeof(newEntry.key) - 1);
    newEntry.type = TYPE_BLOB;
    newEntry.length = length;
    newEntry.crc = calculateCRC(data, length);

    uint32_t newDataOffset;
    if (entryExists) {
        // Заменяем существующую запись
        entryOffset = sizeof(PageHeader) + existingEntryIndex * sizeof(Entry);
        newDataOffset = existingDataOffset;
        STM_LOG("Replacing existing entry at offset %lu, data at offset %lu",
                entryOffset, newDataOffset);
    } else {
        // Добавляем новую запись
        entryOffset = sizeof(PageHeader) + cache.header.count * sizeof(Entry);
        newDataOffset = sizeof(PageHeader) + MAX_ENTRIES_PER_PAGE * sizeof(Entry);

        // Пропускаем данные существующих записей
        uint32_t currentOffset = sizeof(PageHeader);
        for (uint16_t i = 0; i < cache.header.count; i++) {
            const Entry* currentEntry = (const Entry*)(sectorBuffer + currentOffset);
            newDataOffset += currentEntry->length;
            currentOffset += sizeof(Entry);
        }
        STM_LOG("Adding new entry at offset %lu, data at offset %lu",
                entryOffset, newDataOffset);
    }

    // Проверка места
    if (newDataOffset + length > SECTOR_SIZE) {
        STM_LOG("Not enough space in sector");
        return ERROR_NO_SPACE;
    }

    // Обновляем буфер сектора
    // 1. Копируем метаданные записи
    memcpy(sectorBuffer + entryOffset, &newEntry, sizeof(Entry));

    // 2. Копируем данные
    memcpy(sectorBuffer + newDataOffset, data, length);

    // 3. Обновляем заголовок если это новая запись
    if (!entryExists) {
        cache.header.count++;
    }
    cache.header.crc = calculateCRC(&cache.header, offsetof(PageHeader, crc));
    memcpy(sectorBuffer, &cache.header, sizeof(PageHeader));

    STM_LOG("Writing sector with %d entries", cache.header.count);

    // Стираем и записываем сектор
    flashDriver->W25qxx_EraseSector(currentSector);
    HAL_Delay(10);

    // Записываем весь сектор за один раз
    flashDriver->W25qxx_WriteSector(sectorBuffer, currentSector, 0, SECTOR_SIZE);
    HAL_Delay(10);

    // Проверяем записанные данные
    uint8_t* verifyBuffer = new uint8_t[SECTOR_SIZE];
    if (!verifyBuffer) {
        return ERROR_NO_SPACE;
    }

    // Читаем весь сектор для проверки
    flashDriver->W25qxx_ReadSector(verifyBuffer, currentSector, 0, SECTOR_SIZE);

    // Проверяем корректность записи всего сектора
    if (memcmp(sectorBuffer, verifyBuffer, SECTOR_SIZE) != 0) {
        STM_LOG("Sector verification failed");
        delete[] verifyBuffer;
        return ERROR_WRITE;
    }

    delete[] verifyBuffer;
    STM_LOG("Write and verification successful");
    return OK;
}

FlashNVS::ErrorCode FlashNVS::getBlob(const char* key, void* data, uint32_t* length) {
    Entry entry;
    uint32_t offset;
    ErrorCode err = findEntry(key, &entry, &offset);

    STM_LOG("getBlob: searching for key '%s'", key);

    if (err != OK) {
        STM_LOG("getBlob: findEntry failed with error %d", err);
        return err;
    }

    if (entry.type != TYPE_BLOB) {
        STM_LOG("getBlob: wrong type %d for key '%s'", entry.type, key);
        return ERROR_TYPE_MISMATCH;
    }

    STM_LOG("getBlob: found entry, length=%lu, crc=0x%08lX", entry.length, entry.crc);
    return readEntry(&entry, data, length);
}

FlashNVS::ErrorCode FlashNVS::writeEntry(const char* key, const void* value, uint32_t length, DataType type) {
    // Загружаем текущий сектор в буфер
    loadSectorToBuffer(currentSector);

    // Проверяем, нужно ли выполнить уплотнение
    if (cache.header.count >= MAX_ENTRIES_PER_PAGE) {
        ErrorCode err = compactSector();
        if (err != OK) return err;
        loadSectorToBuffer(currentSector);
    }

    // Подготавливаем запись
    Entry entry;
    strncpy(entry.key, key, NVS_KEY_MAX_LENGTH);
    entry.type = type;
    entry.length = length;
    entry.crc = calculateCRC(value, length);

    // Вычисляем смещения
    uint32_t entryOffset = sizeof(PageHeader) + cache.header.count * sizeof(Entry);
    uint32_t dataOffset = sizeof(PageHeader) + MAX_ENTRIES_PER_PAGE * sizeof(Entry);

    // Копируем данные в буфер
    memcpy(sectorBuffer + entryOffset, &entry, sizeof(Entry));
    memcpy(sectorBuffer + dataOffset, value, length);

    // Обновляем заголовок
    cache.header.count++;
    cache.header.crc = calculateCRC(&cache.header, sizeof(PageHeader) - sizeof(uint16_t));
    memcpy(sectorBuffer, &cache.header, sizeof(PageHeader));

    // Записываем буфер в сектор
    saveSectorFromBuffer(currentSector);

    return OK;
}

FlashNVS::ErrorCode FlashNVS::findEntry(const char* key, Entry* entry, uint32_t* offset) {
    loadSectorToBuffer(currentSector);

    // Копируем заголовок из буфера для проверки
    PageHeader* header = (PageHeader*)sectorBuffer;

    STM_LOG("findEntry: Checking header from sector %lu", currentSector);
    if (!isPageValid(header)) {
        STM_LOG("findEntry: Invalid header in sector %lu", currentSector);
        return ERROR_NOT_FOUND;
    }

    uint32_t entryOffset = sizeof(PageHeader);
    STM_LOG("findEntry: Searching %d entries for key '%s'", header->count, key);

    for (uint16_t i = 0; i < header->count; i++) {
        Entry* currentEntry = (Entry*)(sectorBuffer + entryOffset);

        if (strncmp(currentEntry->key, key, NVS_KEY_MAX_LENGTH) == 0) {
            memcpy(entry, currentEntry, sizeof(Entry));
            *offset = entryOffset;
            STM_LOG("findEntry: Found entry at offset %lu", entryOffset);
            return OK;
        }

        entryOffset += sizeof(Entry);
    }

    STM_LOG("findEntry: Entry not found");
    return ERROR_NOT_FOUND;
}

FlashNVS::ErrorCode FlashNVS::readEntry(const Entry* entry, void* out_value, uint32_t* length) {
    if (*length < entry->length) {
        *length = entry->length;
        return ERROR_INVALID_SIZE;
    }

    loadSectorToBuffer(currentSector);

    // Базовое смещение для данных
    uint32_t dataOffset = sizeof(PageHeader) + MAX_ENTRIES_PER_PAGE * sizeof(Entry);
    uint32_t entryOffset = sizeof(PageHeader);

    // Проходим по всем записям до нашей
    for (uint16_t i = 0; i < cache.header.count; i++) {
        const Entry* currentEntry = (const Entry*)(sectorBuffer + entryOffset);

        // Если нашли нашу запись
        if (strncmp(currentEntry->key, entry->key, NVS_KEY_MAX_LENGTH) == 0) {
            STM_LOG("Found data for key '%s' at offset %lu", entry->key, dataOffset);

            // Копируем данные
            memcpy(out_value, sectorBuffer + dataOffset, entry->length);

            if (strcmp(entry->key, "settings") == 0) {
                STM_LOG("Read settings data at offset %lu:", dataOffset);
                for(uint32_t j = 0; j < 32 && j < entry->length; j++) {
                    STM_LOG("Byte[%lu]: 0x%02X", j, ((uint8_t*)out_value)[j]);
                }
            }

            // Проверяем CRC
            uint16_t calculatedCRC = calculateCRC(out_value, entry->length);
            STM_LOG("CRC verification: calc=0x%04X, expected=0x%04X",
                    calculatedCRC, entry->crc);

            if (calculatedCRC != entry->crc) {
                STM_LOG("CRC verification failed");
                return ERROR_CRC;
            }

            *length = entry->length;
            return OK;
        }

        // Перемещаем указатель на следующую запись и данные
        entryOffset += sizeof(Entry);
        dataOffset += currentEntry->length;
        STM_LOG("Skip entry '%s', next data offset: %lu", currentEntry->key, dataOffset);
    }

    STM_LOG("Entry not found");
    return ERROR_NOT_FOUND;
}

FlashNVS::ErrorCode FlashNVS::compactSector() {
    STM_LOG("Starting sector compaction");

    // Создаем временный буфер
    uint8_t* tempBuffer = new uint8_t[SECTOR_SIZE];
    if (!tempBuffer) {
        STM_LOG("Failed to allocate temporary buffer");
        return ERROR_NO_SPACE;
    }

    memset(tempBuffer, 0xFF, SECTOR_SIZE);

    // Подготовка нового заголовка
    PageHeader newHeader;
    newHeader.magicNumber = MAGIC_NUMBER;
    newHeader.count = 0;
    newHeader.sectorNumber = currentSector;

    // Начальные смещения
    uint32_t newEntryOffset = sizeof(PageHeader);
    uint32_t newDataOffset = sizeof(PageHeader) + MAX_ENTRIES_PER_PAGE * sizeof(Entry);

    // Массив для отслеживания обработанных ключей
    char processedKeys[MAX_ENTRIES_PER_PAGE][NVS_KEY_MAX_LENGTH] = {0};
    int processedCount = 0;

    // Проходим от последних записей к первым
    for (int16_t i = cache.header.count - 1; i >= 0; i--) {
        const Entry* currentEntry = (const Entry*)(sectorBuffer + sizeof(PageHeader) + i * sizeof(Entry));

        // Проверяем, не обработан ли уже этот ключ
        bool isProcessed = false;
        for (int j = 0; j < processedCount; j++) {
            if (strncmp(currentEntry->key, processedKeys[j], NVS_KEY_MAX_LENGTH) == 0) {
                isProcessed = true;
                break;
            }
        }

        if (isProcessed) {
            continue;
        }

        // Находим данные для текущей записи
        uint32_t oldDataOffset = sizeof(PageHeader) + MAX_ENTRIES_PER_PAGE * sizeof(Entry);
        for (uint16_t j = 0; j < i; j++) {
            const Entry* prevEntry = (Entry*)(sectorBuffer + sizeof(PageHeader) + j * sizeof(Entry));
            oldDataOffset += prevEntry->length;
        }

        // Копируем запись и данные
        memcpy(tempBuffer + newEntryOffset, currentEntry, sizeof(Entry));
        memcpy(tempBuffer + newDataOffset, sectorBuffer + oldDataOffset, currentEntry->length);

        // Запоминаем обработанный ключ
        strncpy(processedKeys[processedCount++], currentEntry->key, NVS_KEY_MAX_LENGTH);

        newEntryOffset += sizeof(Entry);
        newDataOffset += currentEntry->length;
        newHeader.count++;
    }

    // Обновляем CRC заголовка
    newHeader.crc = calculateCRC(&newHeader, offsetof(PageHeader, crc));
    memcpy(tempBuffer, &newHeader, sizeof(PageHeader));

    STM_LOG("Compacting: %d entries -> %d entries", cache.header.count, newHeader.count);

    // Записываем новые данные
    flashDriver->W25qxx_EraseSector(currentSector);
    flashDriver->W25qxx_WriteSector(tempBuffer, currentSector, 0, SECTOR_SIZE);

    // Проверяем записанные данные
    loadSectorToBuffer(currentSector);
    if (memcmp(tempBuffer, sectorBuffer, SECTOR_SIZE) != 0) {
        delete[] tempBuffer;
        STM_LOG("Compaction verification failed");
        return ERROR_WRITE;
    }

    delete[] tempBuffer;

    // Обновляем кэш
    memcpy(&cache.header, &newHeader, sizeof(PageHeader));

    STM_LOG("Compaction completed successfully");
    return OK;
}

bool FlashNVS::isPageValid(const PageHeader* header) {
    if (!header) {
        STM_LOG("Header is NULL");
        return false;
    }

    STM_LOG("Header check: magic=0x%08lX (expected 0x%08lX), count=%d",
            header->magicNumber, MAGIC_NUMBER, header->count);

    if (header->magicNumber != MAGIC_NUMBER) {
        STM_LOG("Invalid magic number");
        return false;
    }

    if (header->count > MAX_ENTRIES_PER_PAGE) {
        STM_LOG("Invalid entry count: %d > %d", header->count, MAX_ENTRIES_PER_PAGE);
        return false;
    }

    // Вычисляем CRC без учета поля crc
    uint16_t calculatedCRC = calculateCRC(header, offsetof(PageHeader, crc));
    if (calculatedCRC != header->crc) {
        STM_LOG("Invalid CRC: calculated=0x%04X, stored=0x%04X", calculatedCRC, header->crc);
        return false;
    }

    STM_LOG("Header is valid");
    return true;
}

bool FlashNVS::verifyData(const void* data, size_t length, uint16_t expectedCrc) {
    if (!data || length == 0) {
        return false;
    }

    uint16_t calculatedCRC = calculateCRC(data, length);
    STM_LOG("CRC verification: calculated=0x%04X, expected=0x%04X",
            calculatedCRC, expectedCrc);
    return calculatedCRC == expectedCrc;
}

uint16_t FlashNVS::calculateCRC(const void* data, size_t length) {
    uint16_t crc = 0xFFFF;
    const uint8_t* p = (const uint8_t*)data;
    
    while (length--) {
        uint8_t byte = *p++;
        crc ^= byte;
        for (int i = 0; i < 8; i++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;  // Полином для CRC-16-MODBUS
            } else {
                crc = crc >> 1;
            }
        }
    }
    
    return crc;
}
