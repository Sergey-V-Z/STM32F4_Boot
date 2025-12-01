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
typedef enum {
	CMD_PING                    = 0x00000001,    // Проверка связи
	CMD_START_UPDATE            = 0x00000002,    // Начало процесса обновления
	CMD_FIRMWARE_DATA           = 0x00000003,    // Блок данных прошивки
	CMD_END_UPDATE              = 0x00000004,    // Завершение процесса обновления
	CMD_GET_STATUS              = 0x00000005,    // Запрос статуса обновления
	CMD_ABORT_UPDATE            = 0x00000006,    // Отмена процесса обновления
	CMD_START_BACKUP_UPDATE     = 0x00000007,    // Начало процесса обновления резервной прошивки
	CMD_GET_METADATA            = 0x00000008,    // Получить метаданные текущей прошивки
	CMD_REBOOT                  = 0x00000009,    // Перезагрузка
	// команды для прошивки вторичных плат такие же как и для основной прошивки. общие команды для всех плат, различие будет только в метаданных!
	CMD_SECOND_PING             = 0x0000000A,    // Проверка связи вторичной платы
	CMD_START_SAVE_SECOND       = 0x0000000B,    // Начало процесса сохранения на spi flash вторичной платы
	CMD_START_SECONDARY_UPDATE  = 0x0000000C,    // Начало процесса обновления вторичной платы
	CMD_SECOND_FIRMWARE_DATA    = 0x0000000D,    // Блок данных прошивки вторичной платы
	CMD_END_SECOND_UPDATE       = 0x0000000E,    // Завершение обновления прошивки вторичной платы
	CMD_GET_SECOND_METADATA     = 0x0000000F,    // Получить метаданные прошивки вторичной платы
	CMD_SECOND_REBOOT           = 0x00000010,    // Перезагрузка вторичной платы
	CMD_STATUS_CELLS            = 0x00000011,    // отправка данных клиенту о содержании ячеек прошивок
	CMD_SECOND_FW_START         = 0x00000012,    // старт автопрошивки из ячейки в доступные платы
	CMD_SECOND_FW_STATUS        = 0x00000013,    // статус вторичных прошивок (сколько и каких плат подключено, какие из них прошиваются в данный момент)
	CMD_RESPONSE                = 0x00000080     // Ответ сервера
} firmware_update_cmd_t;

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
#define UPDATE_ERROR_NO_SECTOR_FOUND  0x00000008	// Не найден свободный сектор для прошивки
#define UPDATE_ERROR_INVALID_METADATA  0x00000009    // Неверные метаданные
#define UPDATE_ERROR_INVALID_PARAM     0x0000000A    // Неверный тип прошивки

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


#define SPI_SECOND_CONFIG_ADDRESS 0x00181000 /* Хранит структуры данных для вторичной платы где и какая прошивка лежит */
#define SPI_SECOND_CONFIG_SIZE 0x00001000 /* 4KB for secondary configuration */

#define SPI_FIRMWARE_SIZE 0x00040000 /* 256KB for firmware */
#define SPI_FIRMWARE_S1_ADDRESS (SPI_SECOND_CONFIG_ADDRESS + SPI_SECOND_CONFIG_SIZE) /* Адрес начала прошивки для вторичной платы сектор 1 */
#define SPI_FIRMWARE_S2_ADDRESS (SPI_FIRMWARE_S1_ADDRESS + SPI_FIRMWARE_SIZE) /* Адрес начала прошивки для вторичной платы сектор 2 */
#define SPI_FIRMWARE_S3_ADDRESS (SPI_FIRMWARE_S2_ADDRESS + SPI_FIRMWARE_SIZE) /* Адрес начала прошивки для вторичной платы сектор 3 */
#define SPI_FIRMWARE_S4_ADDRESS (SPI_FIRMWARE_S3_ADDRESS + SPI_FIRMWARE_SIZE) /* Адрес начала прошивки для вторичной платы сектор 4 */
#define SPI_FIRMWARE_S5_ADDRESS (SPI_FIRMWARE_S4_ADDRESS + SPI_FIRMWARE_SIZE) /* Адрес начала прошивки для вторичной платы сектор 5 */
#define SPI_FIRMWARE_S6_ADDRESS (SPI_FIRMWARE_S5_ADDRESS + SPI_FIRMWARE_SIZE) /* Адрес начала прошивки для вторичной платы сектор 6 */

/* Flash constants */
#define FLASH_PAGE_SIZE 0x4000 /* 16 KB pages for STM32F407 */
#define FLASH_SECTOR_SIZE FLASH_PAGE_SIZE

/* Максимальное количество сбросов */
#define MAX_RESET_COUNT 3

#define BOOTLOADER_VERSION 0x00010000 /* Bootloader version 1.0.0.0 */
#define BOOTLOADER_FIRMWARE_BUFFER_SIZE 1024 /* Size of buffer for firmware updates */

typedef enum {
    Main = 0,
    Backup,
    Secondary,
    Unknown
} fw_type_t;

/* Bootloader state enum */
typedef enum {
	BOOTLOADER_STATE_INIT, /* Bootloader initializing */
	BOOTLOADER_STATE_RECOVERY, /* Entering recovery mode */
	BOOTLOADER_STATE_ERROR /* Error state */
} Bootloader_State_t;

// Формат заголовка пакета (упрощенный)
typedef struct {
	uint32_t command;       // Команда
	uint32_t variable; 		// Номер блока или доп. параметр
	uint32_t size;          // Размер данных после заголовка
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
	uint32_t status;                       	// Текущий статус обновления
	uint32_t error;                        	// Код ошибки (если есть)
	uint32_t receivedSize;                 	// Полученный размер
	uint32_t expectedBlockNumber;          	// Ожидаемый номер блока
	uint32_t firmwareCRC;                  	// Расчетный CRC32 прошивки
	SemaphoreHandle_t mutex;               	// Мьютекс для доступа к структуре
	uint32_t sectorAddress;                 // Адрес сектора для записи прошивки
	fw_type_t fw_type;                      // Тип прошивки
	meta_t metadata;                    	// Метаданные прошивки
	FWUpdateParams update_params;        	// Параметры обновления
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

// перечисления статусов обновления прошивки вторичных плат
typedef enum {
	SECOND_UPDATE_STATUS_IDLE = 0,          // Ожидание
	SECOND_UPDATE_STATUS_IN_PROGRESS,       // В процессе
	SECOND_UPDATE_STATUS_ERROR,             // Ошибка
	SECOND_UPDATE_STATUS_READY_REBOOT       // Готов к перезагрузке
} Second_Update_Status_t;

// ответ от вторичной платы
typedef enum status_flash_t
{
    idle = 0,
    write,
    ready,
    error,
    OK_boot_data,
    ERR_read_boot_data,
    ERR_write_boot_data,
    ERR_crc_boot_data,
    EMPTY_boot_data,
    ERR_null_ptr
} status_flash_t;

// структура контекст обновления прошивки вторичной платы
// их этого контекста будет сделан массив в нем должно хранится метаданные прошивки вторичной платы
// и состояние обновления прошивки вторичной платы
// для каждой вторичной платы будет свой контекст
// в этом контексте будет хранится адрес платы на шине, тип платы, адрес для хранения прошивки во внешней флеш-памяти, общий размер прошивки
typedef struct {
	uint32_t boardType;                   // Тип платы
	uint32_t firmwareStorageAddress;      // Адрес для хранения прошивки во внешней флеш-памяти
	uint32_t firmwareSize;                // Общий размер прошивки
	FirmwareUpdateContext updateContext;  // Контекст обновления прошивки
	Second_Update_Status_t status;
	status_flash_t error;                    // Код ошибки (если есть)
	meta_t metadata;                // Метаданные прошивки вторичной платы
} FirmwareUpdateContext_Sec;

// Функции инициализации и управления
void FirmwareUpdate_Init(flash *spiFlash);
void FirmwareUpdate_Deinit(void);
uint32_t FirmwareUpdate_GetStatus(uint32_t *error);

// Функции обработки команд протокола
uint8_t FirmwareUpdate_StartUpdate(meta_t *metadata, FWUpdateParams *update_params);
uint8_t FirmwareUpdate_ProcessDataBlock(uint32_t blockNumber, const uint8_t *data, uint32_t size);
uint32_t FirmwareUpdate_EndUpdate(void);
uint8_t FirmwareUpdate_AbortUpdate(void);
uint32_t CalculateFirmwareCRC(flash* spiFlash, uint32_t startAddress, uint32_t length);
uint8_t FirmwareUpdate_StartBackupUpdate(meta_t *metadata, FWUpdateParams *update_params);

void DumpFirmwareHex(flash* spiFlash, uint32_t startAddress, uint32_t length, uint8_t bytesPerLine, uint32_t delayMs);
void FirmwareUpdate_PrintMetadata(void);

// Функция старта сохранения вторичной платы
uint8_t FirmwareUpdate_StartSecondaryUpdate(meta_t *metadata, FWUpdateParams *update_params);
uint32_t FirmwareUpdate_FindSectorForSecondaryFirmware(meta_t *metadata);
uint8_t FirmwareUpdate_GetSecondaryMetaData(uint32_t sectorAddress, meta_t* metadata);
uint8_t SetSecondaryFirmwareConfig(SecondaryFirmwareConfig *config);
uint8_t GetSecondaryFirmwareConfig(SecondaryFirmwareConfig *config);
void ResetSecondaryFirmwareConfig(void);
uint8_t TransferFirmwareToSecondaryBoard(DEV_t *dev, FirmwareUpdateCellState cell);

// api работы со вторичными платами
void FirmwareUpdate_Secondary(DEV_t *dev, uint8_t size_dev);
#endif // FIRMWARE_UPDATE_H
