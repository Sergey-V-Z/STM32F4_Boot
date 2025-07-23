#ifndef _FLASH_NVS_H
#define _FLASH_NVS_H

#include "flash_spi.h"
#include <string.h>

class FlashNVS {
public:
    enum ErrorCode {
        OK = 0,
        ERROR_INVALID_SIZE,
        ERROR_NO_SPACE,
        ERROR_NOT_FOUND,
        ERROR_CRC,
        ERROR_WRITE,
        ERROR_READ,
        ERROR_TYPE_MISMATCH
    };

    enum DataType {
        TYPE_NONE = 0,
        TYPE_STR = 1,
        TYPE_BLOB = 2
    };

    static const uint32_t MAGIC_NUMBER = 0xABCD1234;
    static const uint32_t MAX_ENTRIES_PER_PAGE = 16;
    static const uint32_t NVS_KEY_MAX_LENGTH = 16;
    static const uint32_t SECTOR_SIZE = 4096;

    struct PageHeader {
        uint32_t magicNumber;
        uint16_t count;
        uint32_t sectorNumber;
        uint16_t crc;  // Перемещаем CRC в конец структуры
    };

    struct Entry {
        char key[NVS_KEY_MAX_LENGTH];
        DataType type;
        uint32_t length;
        uint16_t crc;  // Изменено с uint32_t на uint16_t
        uint16_t reserved; // Для выравнивания структуры
    };

    FlashNVS(flash* driver);
    ~FlashNVS();

    ErrorCode init(uint32_t startSector = 0);
    ErrorCode setString(const char* key, const char* value);
    ErrorCode getString(const char* key, char* out_value, uint32_t* length);
    ErrorCode setBlob(const char* key, const void* data, uint32_t length);
    ErrorCode getBlob(const char* key, void* data, uint32_t* length);

private:
    flash* flashDriver;
    uint32_t currentSector;
    uint32_t dataSector;
    
    struct {
        PageHeader header;
        Entry entries[MAX_ENTRIES_PER_PAGE];
    } cache;

    uint8_t sectorBuffer[SECTOR_SIZE];

    ErrorCode writeEntry(const char* key, const void* value, uint32_t length, DataType type);
    ErrorCode findEntry(const char* key, Entry* entry, uint32_t* offset);
    ErrorCode readEntry(const Entry* entry, void* out_value, uint32_t* length);
    ErrorCode compactSector();
    bool isPageValid(const PageHeader* header);
    uint32_t findNextFreeSector();
    bool verifyData(const void* data, size_t length, uint16_t expectedCrc);
    void loadSectorToBuffer(uint32_t sector);
    void saveSectorFromBuffer(uint32_t sector);
    uint16_t calculateCRC(const void* data, size_t length);
};

#endif // _FLASH_NVS_H
