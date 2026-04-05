/**
 * @file firmware_update.h
 * @brief Определения и интерфейсы для системы обновления прошивки
 */

#ifndef FIRMWARE_UPDATE_H
#define FIRMWARE_UPDATE_H

#include <stdint.h>
#include "flash_spi.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "main.h"

// Команды протокола обновления
#define CMD_PING                	0x00000001    // Проверка связи
#define CMD_START_UPDATE        	0x00000002    // Начало процесса обновления
#define CMD_FIRMWARE_DATA       	0x00000003    // Блок данных прошивки
#define CMD_END_UPDATE          	0x00000004    // Завершение процесса обновления
#define CMD_GET_STATUS          	0x00000005    // Запрос статуса обновления
#define CMD_ABORT_UPDATE        	0x00000006    // Отмена процесса обновления
#define CMD_START_BACKUP_UPDATE   	0x00000007    // Начало процесса обновления резервной прошивки
#define CMD_GET_METADATA          	0x00000008    // Получить метаданные текущей прошивки
#define CMD_REBOOT          		0x00000009    // Перезагрузка
#define CMD_RESPONSE            	0x00000080    // Ответ сервера

// Коды состояния обновления
#define UPDATE_STATUS_IDLE            0x00000000    // Ожидание
#define UPDATE_STATUS_IN_PROGRESS     0x00000001    // Процесс выполняется
#define UPDATE_STATUS_COMPLETE        0x00000002    // Обновление завершено успешно
#define UPDATE_STATUS_ERROR           0x00000003    // Ошибка обновления
#define UPDATE_STATUS_READY_REBOOT    0x00000004    // Готов к перезагрузке

// Коды ошибок
#define UPDATE_ERROR_NONE             0x00000000    // Нет ошибок
#define UPDATE_ERROR_INVALID_CMD      0x00000001    // Неверная команда
#define UPDATE_ERROR_INVALID_SIZE     0x00000002    // Неверный размер
#define UPDATE_ERROR_FLASH_FAILURE    0x00000003    // Ошибка при записи во флеш
#define UPDATE_ERROR_CRC_MISMATCH     0x00000004    // Ошибка CRC
#define UPDATE_ERROR_SEQ_ERROR        0x00000005    // Ошибка последовательности
#define UPDATE_ERROR_BUSY             0x00000006    // Система занята
#define UPDATE_ERROR_ABORT            0x00000007    // Обновление отменено

// Размер буфера для приема данных прошивки
#define FIRMWARE_BUFFER_SIZE          1024

/* Bootloader-specific memory locations */
#define BOOTLOADER_START_ADDRESS 0x08000000 /* Start of bootloader */
#define BOOTLOADER_SIZE 0x0000C000 /* 48 KB for bootloader */
#define BOOT_DATA_ADDRESS 0x0800C000 /* Boot data area */
#define BOOT_DATA_SIZE 0x00004000 /* 16 KB for boot data */
#define MAIN_PROGRAM_START_ADDRESS 0x08010000 /* Start of application area */
#define MAIN_PROGRAM_SIZE 0x00070000 /* 448 KB for application */
// Определение смещения метаданных (512 байт от начала программы)
#define METADATA_OFFSET     512
#define METADATA_ADDRESS    (MAIN_PROGRAM_START_ADDRESS + METADATA_OFFSET) // Базовый адрес + смещение

/* SPI Flash memory layout constants - adjust based on your flash size */
#define SPI_FLASH_CONFIG_ADDRESS 0x00000000 /* Start of configuration area */
#define SPI_FLASH_CONFIG_SIZE 0x00001000 /* 4KB for configuration */
#define SPI_FLASH_MAIN_FW_ADDRESS 0x00001000 /* Start of main firmware area */
#define SPI_FLASH_MAIN_FW_SIZE 0x00080000 /* 512KB for main firmware */
#define SPI_FLASH_BACKUP_FW_ADDRESS 0x00081000 /* Start of backup firmware area */
#define SPI_FLASH_BACKUP_FW_SIZE 0x00080000 /* 512KB for backup firmware */
#define SPI_FLASH_APP_DATA_ADDRESS 0x00101000 /* Start of application data area */
#define SPI_FLASH_APP_DATA_SIZE 0x00080000 /* 512KB for application data */

/* Flash constants */
#define FLASH_PAGE_SIZE 0x4000 /* 16 KB pages for STM32F407 */
#define FLASH_SECTOR_SIZE FLASH_PAGE_SIZE

/* Максимальное количество сбросов */
#define MAX_RESET_COUNT 3

#define BOOTLOADER_VERSION 0x00010000 /* Bootloader version 1.0.0.0 */
#define BOOTLOADER_FIRMWARE_BUFFER_SIZE 1024 /* Size of buffer for firmware updates */

/* Bootloader state enum */
typedef enum {
	BOOTLOADER_STATE_INIT, /* Bootloader initializing */
	BOOTLOADER_STATE_RECOVERY, /* Entering recovery mode */
	BOOTLOADER_STATE_ERROR /* Error state */
} Bootloader_State_t;

// Структура параметров обновления, передаваемых в payload пакета CMD_START_UPDATE
typedef struct {
    uint32_t fw_size;    // Размер прошивки в байтах
    uint32_t fw_crc;     // CRC32 прошивки
    uint32_t cell_num;   // Номер ячейки (для устройств с вторичными платами)
    uint32_t reserved;   // Зарезервировано
} FWUpdateParams;

// Формат заголовка пакета (упрощенный)
typedef struct {
	uint32_t command;       // Команда
	uint32_t size;         // Размер данных
	uint32_t block_number; // Номер блока или доп. параметр
} PacketHeader;

// Формат ответного пакета
typedef struct {
	uint32_t command;       // Команда CMD_RESPONSE
	uint32_t status;        // Статус операции
	uint32_t error;        // Код ошибки (если есть)
	uint32_t reserv;
} ResponsePacket;

// структура ответа с метаданными
typedef struct {
    uint32_t command;       // Команда CMD_RESPONSE
    uint32_t status;        // Статус операции
    uint32_t error;         // Код ошибки
    meta_t metadata;        // Метаданные прошивки
} MetadataResponsePacket;

// Структура обновления
typedef struct {
	uint32_t status;                       // Текущий статус обновления
	uint32_t error;                        // Код ошибки (если есть)
	uint32_t totalSize;                    // Общий размер прошивки
	uint32_t receivedSize;                 // Полученный размер
	uint32_t expectedBlockNumber;          // Ожидаемый номер блока
	uint32_t firmwareVersion;              // Версия прошивки
	uint32_t firmwareCRC;                  // Расчетный CRC32 прошивки
	uint32_t declaredCRC;                  // Заявленный CRC32 прошивки
	SemaphoreHandle_t mutex;               // Мьютекс для доступа к структуре
	uint8_t isBackupUpdate;                // Флаг обновления резервной прошивки
} FirmwareUpdateContext;

/* Recovery mode reason enum */
typedef enum {
	RECOVERY_REASON_NONE = 0,
	RECOVERY_REASON_NO_APP,
	RECOVERY_REASON_INVALID_APP,
	RECOVERY_REASON_RESET_COUNT,
	RECOVERY_REASON_APP_OUTDATED,
	RECOVERY_REASON_APP_UPDATE
} Recovery_Reason_t;

typedef struct {
	struct Bootloader_Status_t {
		Bootloader_State_t State; /* Current state of bootloader */
		Recovery_Reason_t RecoveryReason;
		uint32_t ErrorCode; /* Error code if any */
		//uint32_t ResetCount; /* Number of consecutive resets */
		uint8_t IsAppValid; /* Is the application valid */
		uint8_t IsRecovery; /* Is in recovery mode */
		uint32_t LastFlashAddress; /* Last address flashed */
		uint32_t BytesFlashed; /* Number of bytes flashed */
	} status;

	struct AppMetadata_t {
		uint32_t firmwareSize; /* Size of firmware in bytes */
		uint32_t firmwareCRC; /* CRC of firmware */
		uint32_t firmwareVersion; /* Version number of firmware */
		uint32_t update; /* Flag indicating if app need update */
		uint32_t reserved; /* Reserved for future use */
	} main_metadata;

	struct backup_Metadata_t {
		uint32_t firmwareSize; /* Size of firmware in bytes */
		uint32_t firmwareCRC; /* CRC of firmware */
		uint32_t firmwareVersion; /* Version number of firmware */
		uint32_t update; /* Flag indicating if app need update */
		uint32_t reserved; /* Reserved for future use */
	} backup_metadata;

	/* SPI Flash memory layout structure */
	struct SPI_Flash_Layout_t {
		uint32_t ConfigAreaAddress; /* Address of configuration storage area */
		uint32_t ConfigAreaSize; /* Size of configuration storage area */
		uint32_t MainFirmwareAddress; /* Address of main firmware storage area */
		uint32_t MainFirmwareSize; /* Size of main firmware storage area */
		uint32_t BackupFirmwareAddress; /* Address of backup firmware storage area */
		uint32_t BackupFirmwareSize; /* Size of backup firmware storage area */
		uint32_t AppDataAddress; /* Address of application data area */
		uint32_t AppDataSize; /* Size of application data area */
	} SPI_Flash_Layout;

	/* Firmware metadata structure */
	struct SPI_Flash_Firmware_Metadata_t {
		uint32_t FirmwareVersion; /* Firmware version */
		uint32_t FirmwareSize; /* Firmware size in bytes */
		uint32_t FirmwareCRC; /* Firmware CRC */
		uint32_t TargetAddress; /* Target address in internal flash */
		uint32_t Reserved[4]; /* Reserved for future use */
	} SPI_Flash_Firmware_Metadata;

	uint32_t flashInitialized;
	uint32_t reset_counter;
	uint32_t structCRC; /* CRC of this structure (except structCRC field) */

} boot_data_t;

// Функции инициализации и управления
void FirmwareUpdate_Init(flash *spiFlash);
void FirmwareUpdate_Deinit(void);
uint32_t FirmwareUpdate_GetStatus(uint32_t *error);
uint8_t FirmwareUpdate_StartTask(void);

// Функции обработки команд протокола
uint8_t FirmwareUpdate_StartUpdate(uint32_t size, uint32_t version);
uint8_t FirmwareUpdate_ProcessDataBlock(uint32_t blockNumber, const uint8_t *data, uint32_t size, uint32_t crc);
uint8_t FirmwareUpdate_EndUpdate(uint32_t crc);
uint8_t FirmwareUpdate_AbortUpdate(void);
uint8_t FirmwareUpdate_StartBackupUpdate(uint32_t size, uint32_t version);

void DumpFirmwareHex(flash* spiFlash, uint32_t startAddress, uint32_t length, uint8_t bytesPerLine, uint32_t delayMs);
#endif // FIRMWARE_UPDATE_H
