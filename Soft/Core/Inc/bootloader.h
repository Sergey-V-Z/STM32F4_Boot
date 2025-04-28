/**
 ******************************************************************************
 * @file    bootloader.h
 * @brief   Header for bootloader.c file.
 *          This file contains the main bootloader functionality.
 ******************************************************************************
 */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __BOOTLOADER_H
#define __BOOTLOADER_H

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"
#include "flash_spi.h"
#include "main.h"
/* Exported types ------------------------------------------------------------*/

/* Bootloader state enum */
typedef enum {
	BOOTLOADER_STATE_INIT, /* Bootloader initializing */
	BOOTLOADER_STATE_RECOVERY, /* Entering recovery mode */
	BOOTLOADER_STATE_ERROR /* Error state */
} Bootloader_State_t;

/* Bootloader error codes */
typedef enum {
	BOOTLOADER_ERROR_NONE = 0,
	BOOTLOADER_ERROR_INIT_FAILED,
	BOOTLOADER_ERROR_FLASH_INIT_FAILED,
	BOOTLOADER_ERROR_APP_INVALID,
	BOOTLOADER_ERROR_FLASH_ERASE_FAILED,
	BOOTLOADER_ERROR_FLASH_WRITE_FAILED,
	BOOTLOADER_ERROR_CRC_FAILED,
	BOOTLOADER_ERROR_RECOVERY_FAILED,    // ошибки для неудачного восстановления
	BOOTLOADER_ERROR_INVALID_STATE,      //ошибки для неверного состояния
	BOOTLOADER_ERROR_UNKNOWN
} Bootloader_Error_t;

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
		uint32_t ErrorCode; /* Error code if any */
		uint32_t ResetCount; /* Number of consecutive resets */
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
	} metadata;

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

typedef enum {
	not_read_meta = 0, /* Ошибка чтения метаданных */
	not_valid_crc_meta = 1, /* Неверная CRC метаданных */
	need_update = 2, /* Требуется обновление */
	not_valid_app = 3, /* Приложение не валидно */
	not_valid_crc_app = 4, /* Неверная CRC приложения */
	valid_app = 5 /* Приложение валидно */
} meta_valid_t;

typedef enum {
	OK_boot_data = 0,
	ERR_read_boot_data,
	ERR_crc_boot_data,
	EPMTY_boot_data,
	ERR_nuul_ptr
} boot_data_valid_t;

/* Exported constants --------------------------------------------------------*/

/* Bootloader-specific memory locations */
#define BOOTLOADER_START_ADDRESS            0x08000000                  /* Start of bootloader */
#define BOOTLOADER_SIZE                     0x0000C000                  /* 48 KB for bootloader */
#define BOOT_DATA_ADDRESS                   0x0800C000                  /* Boot data area */
#define BOOT_DATA_SIZE                      0x00004000                  /* 16 KB for boot data */
#define MAIN_PROGRAM_START_ADDRESS          0x08010000                  /* Start of application area */
#define MAIN_PROGRAM_SIZE                   0x00070000                  /* 448 KB for application */

/* SPI Flash memory layout constants - adjust based on your flash size */
#define SPI_FLASH_CONFIG_ADDRESS        0x00000000  /* Start of configuration area */
#define SPI_FLASH_CONFIG_SIZE           0x00001000  /* 4KB for configuration */
#define SPI_FLASH_MAIN_FW_ADDRESS       0x00001000  /* Start of main firmware area */
#define SPI_FLASH_MAIN_FW_SIZE          0x00080000  /* 512KB for main firmware */
#define SPI_FLASH_BACKUP_FW_ADDRESS     0x00081000  /* Start of backup firmware area */
#define SPI_FLASH_BACKUP_FW_SIZE        0x00080000  /* 512KB for backup firmware */
#define SPI_FLASH_APP_DATA_ADDRESS      0x00101000  /* Start of application data area */
#define SPI_FLASH_APP_DATA_SIZE         0x00080000  /* 512KB for application data */

/* Flash constants */
#define FLASH_PAGE_SIZE                     0x4000      /* 16 KB pages for STM32F407 */
#define FLASH_SECTOR_SIZE                   FLASH_PAGE_SIZE

/* Максимальное количество сбросов */
#define MAX_RESET_COUNT                 3

#define BOOTLOADER_VERSION                 0x00010000  /* Bootloader version 1.0.0.0 */
#define BOOTLOADER_FIRMWARE_BUFFER_SIZE    1024        /* Size of buffer for firmware updates */
/* Exported macros -----------------------------------------------------------*/

/* Exported functions --------------------------------------------------------*/

/**
 * @brief  Initializes the bootloader.
 * @param  None
 * @retval None
 */
void Bootloader_Init(void);

/**
 * @brief  Runs the bootloader main process.
 * @param  None
 * @retval None
 */
void Bootloader_Run(void);

/**
 * @brief  Forces entry into recovery mode.
 * @param  None
 * @retval None
 */
void Bootloader_ForceRecovery(void);

/**
 * @brief  Jumps to the application.
 * @param  None
 * @retval None (does not return if successful)
 */
void Bootloader_JumpToApplication(void);

/**
 * @brief  Checks if the application is valid.
 * @param  None
 * @retval 1 if valid, 0 if invalid
 */
uint8_t Bootloader_IsApplicationValid(void);

/**
 * @brief  Checks if backup firmware exists in SPI Flash.
 * @param  None
 * @retval 1 if valid backup firmware exists, 0 if not
 */
uint8_t Bootloader_CheckBackupFirmware(void);

/**
 * @brief  Saves the bootloader status to BOOT_DATA area.
 * @param  data: Pointer to boot_data structure
 * @retval HAL status
 */
HAL_StatusTypeDef Bootloader_SaveStatus(const boot_data_t *data);

/**
 * @brief  Loads the bootloader status from BOOT_DATA area.
 * @param  boot_data: Pointer to boot_data structure to load into
 * @retval HAL status
 */
boot_data_valid_t Bootloader_LoadStatus(boot_data_t *boot_data);

/**
 * @brief  Устанавливает флаг необходимости обновления прошивки
 * @param  newFirmwareSize: Размер новой прошивки
 * @param  newFirmwareCRC: CRC новой прошивки
 * @param  newFirmwareVersion: Версия новой прошивки
 * @retval HAL_StatusTypeDef: Статус операции
 */
HAL_StatusTypeDef Bootloader_RequestUpdate(uint32_t newFirmwareSize,
		uint32_t newFirmwareCRC, uint32_t newFirmwareVersion);

/***********************************************************************************************************************
 ***********************************************************************************************************************
 *
 * Утилиты для работв с Flash
 *
 ***********************************************************************************************************************
 ***********************************************************************************************************************/

/**
 * @brief   Unlocks the Flash for writing.
 * @param   None
 * @retval  HAL status
 */
HAL_StatusTypeDef FLASH_Utils_Unlock(void);

/**
 * @brief   Locks the Flash after writing.
 * @param   None
 * @retval  HAL status
 */
HAL_StatusTypeDef FLASH_Utils_Lock(void);

/**
 * @brief   Erases the specified Flash sector.
 * @param   sectorNumber: Sector number to erase
 * @retval  HAL status
 */
HAL_StatusTypeDef FLASH_Utils_EraseSector(uint32_t sectorNumber);

/**
 * @brief   Writes data to Flash.
 * @param   address: address to write to
 * @param   data: data to write
 * @param   dataSize: size of data in bytes (must be a multiple of 4)
 * @retval  HAL status
 */
HAL_StatusTypeDef FLASH_Utils_Write(uint32_t address, const uint8_t *data,
		uint32_t dataSize);

/**
 * @brief   Reads data from Flash.
 * @param   address: address to read from
 * @param   data: buffer to store read data
 * @param   dataSize: size of data to read in bytes
 * @retval  HAL status
 */
HAL_StatusTypeDef FLASH_Utils_Read(uint32_t address, uint8_t *data,
		uint32_t dataSize);

/**
 * @brief   Gets the sector number for a given address.
 * @param   address: Flash address
 * @retval  Sector number
 */
uint32_t FLASH_Utils_GetSector(uint32_t address);

/**
 * @brief   Increments the reset counter.
 * @param   None
 * @retval  New value of reset counter
 */
uint32_t FLASH_Utils_IncrementResetCounter(void);

/**
 * @brief   Reads the reset counter.
 * @param   None
 * @retval  Current value of reset counter
 */
uint32_t FLASH_Utils_ReadResetCounter(void);

/**
 * @brief   Resets the reset counter to zero.
 * @param   None
 * @retval  HAL status
 */
HAL_StatusTypeDef FLASH_Utils_ResetResetCounter(void);

/**
 * @brief   Validates firmware at given address
 * @param   startAddress: starting address of firmware
 * @retval  1 if valid, 0 if invalid
 */
uint8_t FLASH_Utils_ValidateFirmware(uint32_t startAddress);

/**
 * @brief   Checks if application is up to date based on metadata
 * @param   None
 * @retval  meta_valid_t: Статус проверки приложения
 */
meta_valid_t FLASH_Utils_IsAppUpToDate(void);

/**
 * @brief   Marks application as valid and up to date
 * @param   firmwareSize: Size of firmware
 * @param   firmwareCRC: CRC of firmware
 * @param   firmwareVersion: Version of firmware
 * @retval  HAL status
 */
HAL_StatusTypeDef FLASH_Utils_MarkAppAsValid(uint32_t firmwareSize,
		uint32_t firmwareCRC, uint32_t firmwareVersion);

#ifdef __cplusplus
}
#endif

#endif /* __BOOTLOADER_H */
