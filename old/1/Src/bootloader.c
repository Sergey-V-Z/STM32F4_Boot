/**
 ******************************************************************************
 * @file    bootloader.c
 * @brief   Main bootloader functionality.
 *          This file contains the main bootloader logic and state machine.
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"
#include "dma.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"
#include "bootloader.h"
#include "crc.h"
#include "string.h"

/* Private typedef -----------------------------------------------------------*/
typedef void (*pFunction)(void);

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
boot_data_t BootloaderData; // Глобальная переменная для доступа из других модулей
static Recovery_Reason_t RecoveryReason = RECOVERY_REASON_NONE;
//static uint8_t FirmwareBuffer[BOOTLOADER_FIRMWARE_BUFFER_SIZE];

/* Private function prototypes -----------------------------------------------*/
static void Bootloader_InitStatus(void);
static void Bootloader_EnterRecoveryMode(Recovery_Reason_t reason);
static void Bootloader_ProcessState(void);
static void Bootloader_ResetApplication(void);
static uint8_t Bootloader_ValidateApplication(void);
static uint8_t Bootloader_CheckAndInitFlash(void);

/* Functions -----------------------------------------------------------------*/

/**
 * @brief  Initializes the bootloader.
 * @param  None
 * @retval None
 */
void Bootloader_Init(void) {
	/* Initialize status to default values */
	Bootloader_InitStatus();

	/* Загрузка статуса загрузчика из области BOOT_DATA */
	Bootloader_LoadStatus(&BootloaderData);

	/* Initialize Flash utilities */
	if (FLASH_Utils_Unlock() != HAL_OK) {
		BootloaderData.status.State = BOOTLOADER_STATE_ERROR;
		BootloaderData.status.ErrorCode = BOOTLOADER_ERROR_FLASH_INIT_FAILED;
		return;
	}
	FLASH_Utils_Lock();

	/* Check if we need to enter recovery mode due to excessive resets */
	if (BootloaderData.reset_counter >= MAX_RESET_COUNT) {
		Bootloader_EnterRecoveryMode(RECOVERY_REASON_RESET_COUNT);
		return;
	}

	/* Move to application checking state */
	BootloaderData.status.State = BOOTLOADER_STATE_CHECK_APP;

	/* Сохраняем обновленный статус */
	//Bootloader_SaveStatus();
}

/**
 * @brief  Runs the bootloader main process.
 * @param  None
 * @retval None
 */
void Bootloader_Run(void) {
	/* Process the current state */
	Bootloader_ProcessState();
}

/**
 * @brief  Forces entry into recovery mode.
 * @param  None
 * @retval None
 */
void Bootloader_ForceRecovery(void) {
	Bootloader_EnterRecoveryMode(RECOVERY_REASON_APP_OUTDATED);
}

/**
 * @brief  Jumps to the application.
 * @param  None
 * @retval None (does not return if successful)
 */
void Bootloader_JumpToApplication(void) {
	/* Check if application is valid */
	if (!Bootloader_ValidateApplication()) {
		Bootloader_EnterRecoveryMode(RECOVERY_REASON_INVALID_APP);
		return;
	}

	/* Reset recovery reason */
	RecoveryReason = RECOVERY_REASON_NONE;

	/* Disable all interrupts */
	__disable_irq();

	/* Set the vector table offset to the application's vector table */
	SCB->VTOR = MAIN_PROGRAM_START_ADDRESS;

	/* Get the application's reset handler address */
	uint32_t jumpAddress = *(__IO uint32_t*) (MAIN_PROGRAM_START_ADDRESS + 4);
	pFunction jumpToApplication = (pFunction) jumpAddress;

	/* Set the application's stack pointer */
	__set_MSP(*(__IO uint32_t*) MAIN_PROGRAM_START_ADDRESS);

	/* Jump to the application */
	jumpToApplication();

	/* Should never reach here */
	BootloaderData.status.State = BOOTLOADER_STATE_ERROR;
	BootloaderData.status.ErrorCode = BOOTLOADER_ERROR_UNKNOWN;
}

/**
 * @brief  Checks if the application is valid.
 * @param  None
 * @retval 1 if valid, 0 if invalid
 */
uint8_t Bootloader_IsApplicationValid(void) {
	/* Сначала проверяем валидность приложения через метаданные */
	if (!FLASH_Utils_IsAppUpToDate()) {
		/* Если метаданные показывают, что приложение не актуально */
		return 0;
	}

	/* Затем выполняем дополнительную проверку самого приложения */
	return Bootloader_ValidateApplication();
}

/**
 * @brief  Initializes bootloader status to default values.
 * @param  None
 * @retval None
 */
static void Bootloader_InitStatus(void) {
	BootloaderData.status.State = BOOTLOADER_STATE_INIT;
	BootloaderData.status.ErrorCode = BOOTLOADER_ERROR_NONE;
	BootloaderData.status.ResetCount = 0;
	BootloaderData.status.IsAppValid = 0;
	BootloaderData.status.IsRecovery = 0;
	BootloaderData.status.LastFlashAddress = 0;
	BootloaderData.status.BytesFlashed = 0;

	BootloaderData.metadata.firmwareCRC = 0;
	BootloaderData.metadata.firmwareSize = 0;
	BootloaderData.metadata.firmwareVersion = 0;
	BootloaderData.metadata.reserved = 0;
	BootloaderData.metadata.update = 0;

	BootloaderData.reset_counter = 0;
	BootloaderData.structCRC = 0;
}

/**
 * @brief  Saves the bootloader status to BOOT_DATA area.
 * @param  None
 * @retval HAL status
 */
HAL_StatusTypeDef Bootloader_SaveStatus(const boot_data_t *data) {

	HAL_StatusTypeDef status;
	uint32_t sector;

	if (data == NULL)
		return HAL_ERROR;

	status = FLASH_Utils_Unlock();
	if (status != HAL_OK)
		return status;

	/* Erase the sector containing the metadata */
	sector = FLASH_Utils_GetSector(BOOT_DATA_ADDRESS);
	status = FLASH_Utils_EraseSector(sector);
	if (status != HAL_OK) {
		FLASH_Utils_Lock();
		return status;
	}

	/* Write metadata to Flash */
	status = FLASH_Utils_Write(BOOT_DATA_ADDRESS, (const uint8_t*) data, sizeof(boot_data_t));

	FLASH_Utils_Lock();

	return status;
}

/**
 * @brief  Loads the bootloader status from BOOT_DATA area.
 * @param  None
 * @retval HAL status
 */
HAL_StatusTypeDef Bootloader_LoadStatus(boot_data_t *boot_data) {
	uint32_t statusValue = *((uint32_t*) BOOT_DATA_ADDRESS);

	/* Проверяем, что область не пустая (не стертая) */
	if (statusValue == 0xFFFFFFFF) {
		/* Область пустая, используем значения по умолчанию */
		return HAL_ERROR;
	}

	/* Загружаем статус из флеш-памяти */
	return FLASH_Utils_Read(BOOT_DATA_ADDRESS, (uint8_t*) boot_data,
			sizeof(boot_data_t));
}

/**
 * @brief  Enters recovery mode with the specified reason.
 * @param  reason: Reason for entering recovery mode
 * @retval None
 */
static void Bootloader_EnterRecoveryMode(Recovery_Reason_t reason) {
	RecoveryReason = reason;
	BootloaderData.status.State = BOOTLOADER_STATE_RECOVERY;
	BootloaderData.status.IsRecovery = 1;

	/* Сохраняем обновленный статус */
	//Bootloader_SaveStatus();
}

/**
 * @brief  Processes the current bootloader state.
 * @param  None
 * @retval None
 */
static void Bootloader_ProcessState(void) {
	switch (BootloaderData.status.State) {
	case BOOTLOADER_STATE_INIT:
		/* Если загрузчик инициализируется, переходим к проверке приложения */
		BootloaderData.status.State = BOOTLOADER_STATE_CHECK_APP;
		//Bootloader_SaveStatus();
		break;

	case BOOTLOADER_STATE_CHECK_APP:
		/* Проверяем наличие и актуальность приложения во внутренней флеш-памяти */
		switch (FLASH_Utils_IsAppUpToDate()) {
		case not_read_meta:
		case not_valid_crc_meta:
			// нет методанных, проверяем прошивку
			if (Bootloader_ValidateApplication()) {
				BootloaderData.status.IsAppValid = 1;
				BootloaderData.status.State = BOOTLOADER_STATE_JUMP_TO_APP;
				// сохранить мета и запустить
				Bootloader_SaveStatus(&BootloaderData);
				Bootloader_JumpToApplication();
			} else {
				/* Приложение отсутствует или не актуально, переходим в режим восстановления */
				BootloaderData.status.IsAppValid = 0;
				Bootloader_EnterRecoveryMode(RECOVERY_REASON_APP_OUTDATED);
			}
			break;

		case need_update:
			// метаданные есть и требуется обновления
			// проверить есть ли на внешней новое приложение если не то обычная загрузка и выставить в мету ошибку
			BootloaderData.status.IsAppValid = 0;
			Bootloader_EnterRecoveryMode(RECOVERY_REASON_APP_UPDATE);
			break;
		case not_valid_crc_app:
		case not_valid_app:
			// метаданные есть но приложениея нет или поломанно нужно восстановить
			/* Приложение отсутствует или не актуально, переходим в режим восстановления */
			BootloaderData.status.IsAppValid = 0;
			Bootloader_EnterRecoveryMode(RECOVERY_REASON_APP_OUTDATED);
			break;

		case valid_app:
			/* Приложение существует и актуально, переходим к запуску */
			BootloaderData.status.IsAppValid = 1;
			BootloaderData.status.State = BOOTLOADER_STATE_JUMP_TO_APP;
			break;
		default:
			// перезагрузится
			NVIC_SystemReset();
			break;
		}

		//Bootloader_SaveStatus();
		break;

	case BOOTLOADER_STATE_JUMP_TO_APP:
		/* Запускаем приложение */
		Bootloader_JumpToApplication();
		/* Если попали сюда, значит запуск не удался, переходим в режим восстановления */
		Bootloader_EnterRecoveryMode(RECOVERY_REASON_INVALID_APP);
		break;

	case BOOTLOADER_STATE_RECOVERY:
		/* Инициализируем периферию, если не инициализирована */
		MX_GPIO_Init();
		MX_SPI3_Init();

		//SPI_Flash_Init();

		/* Индикация режима восстановления */
		HAL_GPIO_WritePin(G_GPIO_Port, G_Pin, GPIO_PIN_RESET); // Зеленый индикатор

		/* Проверяем наличие резервной прошивки во внешней флеш-памяти */
		if (Bootloader_CheckBackupFirmware()) {
			/* Индикация процесса восстановления */
			HAL_GPIO_WritePin(B_GPIO_Port, B_Pin, GPIO_PIN_RESET); // Синий индикатор

			/* Стираем область приложения */
			Bootloader_ResetApplication();

			/* Копируем прошивку из внешней флеш во внутреннюю */

		} else {
			/* Резервной прошивки нет, проверяем текущую */
			if (Bootloader_ValidateApplication()) {
				/* Существующая прошивка во внутренней флеш-памяти валидна */
				BootloaderData.status.IsAppValid = 1;
				BootloaderData.status.State = BOOTLOADER_STATE_JUMP_TO_APP;
			} else {
				/* Нет валидной прошивки нигде */
				BootloaderData.status.State = BOOTLOADER_STATE_ERROR;
				BootloaderData.status.ErrorCode = BOOTLOADER_ERROR_RECOVERY_FAILED;

				/* Индикация ошибки - отсутствие прошивки */
				HAL_GPIO_WritePin(R_GPIO_Port, R_Pin, GPIO_PIN_RESET);
				HAL_GPIO_WritePin(G_GPIO_Port, G_Pin, GPIO_PIN_SET);
				HAL_GPIO_WritePin(B_GPIO_Port, B_Pin, GPIO_PIN_SET);
			}
		}

		Bootloader_SaveStatus(&BootloaderData);
		break;

	case BOOTLOADER_STATE_FLASHING:
		/* Это состояние не используется в новой логике */
		BootloaderData.status.State = BOOTLOADER_STATE_ERROR;
		BootloaderData.status.ErrorCode = BOOTLOADER_ERROR_INVALID_STATE;
		//Bootloader_SaveStatus();
		break;

	case BOOTLOADER_STATE_ERROR:
		/* В состоянии ошибки ожидаем сброса, индикация SOS */
		for (int i = 0; i < 3; i++) {
			HAL_GPIO_WritePin(R_GPIO_Port, R_Pin, GPIO_PIN_RESET);
			HAL_Delay(200);
			HAL_GPIO_WritePin(R_GPIO_Port, R_Pin, GPIO_PIN_SET);
			HAL_Delay(200);
		}
		HAL_Delay(400);
		for (int i = 0; i < 3; i++) {
			HAL_GPIO_WritePin(R_GPIO_Port, R_Pin, GPIO_PIN_RESET);
			HAL_Delay(500);
			HAL_GPIO_WritePin(R_GPIO_Port, R_Pin, GPIO_PIN_SET);
			HAL_Delay(200);
		}
		HAL_Delay(400);
		for (int i = 0; i < 3; i++) {
			HAL_GPIO_WritePin(R_GPIO_Port, R_Pin, GPIO_PIN_RESET);
			HAL_Delay(200);
			HAL_GPIO_WritePin(R_GPIO_Port, R_Pin, GPIO_PIN_SET);
			HAL_Delay(200);
		}
		HAL_Delay(1000);
		break;

	default:
		/* Неизвестное состояние, переходим в состояние ошибки */
		BootloaderData.status.State = BOOTLOADER_STATE_ERROR;
		BootloaderData.status.ErrorCode = BOOTLOADER_ERROR_UNKNOWN;
		Bootloader_SaveStatus(&BootloaderData);
		break;
	}
}

/**
 * @brief  Validates the application in internal flash.
 * @param  None
 * @retval 1 if valid, 0 if invalid
 */
static uint8_t Bootloader_ValidateApplication(void) {
	/* Проверяем, что область приложения не стёрта (первые байты не равны 0xFF) */
	uint32_t *appStart = (uint32_t*) MAIN_PROGRAM_START_ADDRESS;
	if (*appStart == 0xFFFFFFFF) {
		return 0; /* Область стёрта или пуста */
	}

	/* Check if the stack pointer points to valid RAM area */
	uint32_t sp = *(__IO uint32_t*) MAIN_PROGRAM_START_ADDRESS;

	/* Проверка корректности указателя стека */
	if (sp < 0x20000000 || sp > 0x20020000) {
		return 0; /* Недопустимый указатель стека */
	}

	/* Check if reset vector points to valid flash area */
	uint32_t resetVector = *(__IO uint32_t*) (MAIN_PROGRAM_START_ADDRESS + 4);

	if (resetVector < MAIN_PROGRAM_START_ADDRESS
			|| resetVector > (MAIN_PROGRAM_START_ADDRESS + MAIN_PROGRAM_SIZE)) {
		return 0;
	}

	/* Дополнительная проверка на наличие валидных векторов прерываний */
	for (int i = 0; i < 4; i++) {
		uint32_t vector = *(__IO uint32_t*) (MAIN_PROGRAM_START_ADDRESS
				+ (i * 4));
		if (vector == 0xFFFFFFFF) {
			return 0; /* Вектор не задан - неполная прошивка */
		}
	}

	return 1; /* Все проверки пройдены */
}

/**
 * @brief  Resets the application area in internal flash.
 * @param  None
 * @retval None
 */
static void Bootloader_ResetApplication(void) {
	FLASH_Utils_Unlock();

	/* Determine sectors to erase */
	uint32_t startSector = FLASH_Utils_GetSector(MAIN_PROGRAM_START_ADDRESS);
	uint32_t endSector = FLASH_Utils_GetSector(
			MAIN_PROGRAM_START_ADDRESS + MAIN_PROGRAM_SIZE - 1);

	/* Erase each sector */
	for (uint32_t sector = startSector; sector <= endSector; sector++) {
		FLASH_Utils_EraseSector(sector);
	}

	FLASH_Utils_Lock();
}

/**
 * @brief  Проверяет и инициализирует внешнюю SPI Flash, если необходимо.
 * @param  None
 * @retval 1 если успешно, 0 если ошибка
 */
static uint8_t Bootloader_CheckAndInitFlash(void) {
	/* Если SPI Flash еще не инициализирована, инициализируем её */

	return 1;
}

/**
 * @brief  Checks if backup firmware exists in SPI Flash.
 * @param  None
 * @retval 1 if valid backup firmware exists, 0 if not
 */
uint8_t Bootloader_CheckBackupFirmware(void) {
	/* Инициализируем SPI Flash, если необходимо */
	if (!Bootloader_CheckAndInitFlash())
		return 0;

	/* Чтение метаданных резервной прошивки */


	/* Проверка ограничений на размер прошивки */


	/* Проверка версии и контрольной суммы */


	/* Проверка адреса назначения */


	/* Проверка валидности данных */


	/* Чтение первых байтов прошивки для проверки */

	/* Проверка наличия валидного стека и вектора сброса (первые два слова) */

	/* Проверка, что стек указывает на область RAM */

	/* Проверка, что вектор сброса указывает на область Flash */

	return 1; /* Прошивка существует и, предположительно, валидна */
}

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
HAL_StatusTypeDef FLASH_Utils_Unlock(void) {
	return HAL_FLASH_Unlock();
}

/**
 * @brief   Locks the Flash after writing.
 * @param   None
 * @retval  HAL status
 */
HAL_StatusTypeDef FLASH_Utils_Lock(void) {
	return HAL_FLASH_Lock();
}

/**
 * @brief   Gets the sector number for a given address.
 * @param   address: Flash address
 * @retval  Sector number
 */
uint32_t FLASH_Utils_GetSector(uint32_t address) {
	uint32_t sector = 0;

	/* STM32F4xx has different sector sizes */
	if (address < 0x08003FFF) /* 16KB - Sector 0 */
		sector = FLASH_SECTOR_0;
	else if (address < 0x08007FFF) /* 16KB - Sector 1 */
		sector = FLASH_SECTOR_1;
	else if (address < 0x0800BFFF) /* 16KB - Sector 2 */
		sector = FLASH_SECTOR_2;
	else if (address < 0x0800FFFF) /* 16KB - Sector 3 */
		sector = FLASH_SECTOR_3;
	else if (address < 0x0801FFFF) /* 64KB - Sector 4 */
		sector = FLASH_SECTOR_4;
	else if (address < 0x0803FFFF) /* 128KB - Sector 5 */
		sector = FLASH_SECTOR_5;
	else if (address < 0x0805FFFF) /* 128KB - Sector 6 */
		sector = FLASH_SECTOR_6;
	else if (address < 0x0807FFFF) /* 128KB - Sector 7 */
		sector = FLASH_SECTOR_7;
	else if (address < 0x0809FFFF) /* 128KB - Sector 8 */
		sector = FLASH_SECTOR_8;
	else if (address < 0x080BFFFF) /* 128KB - Sector 9 */
		sector = FLASH_SECTOR_9;
	else if (address < 0x080DFFFF) /* 128KB - Sector 10 */
		sector = FLASH_SECTOR_10;
	else if (address < 0x080FFFFF) /* 128KB - Sector 11 */
		sector = FLASH_SECTOR_11;

	return sector;
}

/**
 * @brief   Erases the specified Flash sector.
 * @param   sectorNumber: Sector number to erase
 * @retval  HAL status
 */
HAL_StatusTypeDef FLASH_Utils_EraseSector(uint32_t sectorNumber) {
	FLASH_EraseInitTypeDef eraseInit;
	uint32_t sectorError = 0;
	HAL_StatusTypeDef status;

	/* Fill EraseInit structure */
	eraseInit.TypeErase = FLASH_TYPEERASE_SECTORS;
	eraseInit.VoltageRange = FLASH_VOLTAGE_RANGE_3;
	eraseInit.Sector = sectorNumber;
	eraseInit.NbSectors = 1;

	status = HAL_FLASHEx_Erase(&eraseInit, &sectorError);

	return status;
}

/**
 * @brief   Writes data to Flash.
 * @param   address: address to write to
 * @param   data: data to write
 * @param   dataSize: size of data in bytes (must be a multiple of 4)
 * @retval  HAL status
 */
HAL_StatusTypeDef FLASH_Utils_Write(uint32_t address, const uint8_t *data,
		uint32_t dataSize) {
	HAL_StatusTypeDef status = HAL_OK;

	/* Check if the data size is a multiple of 4 (word size) */
	if (dataSize % 4 != 0)
		return HAL_ERROR;

	/* Write data word by word */
	for (uint32_t i = 0; i < dataSize; i += 4) {
		/* Cast to ensure proper alignment */
		uint32_t word = *((uint32_t*) (data + i));

		status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address + i, word);

		if (status != HAL_OK)
			break;
	}

	return status;
}

/**
 * @brief   Reads data from Flash.
 * @param   address: address to read from
 * @param   data: buffer to store read data
 * @param   dataSize: size of data to read in bytes
 * @retval  HAL status
 */
HAL_StatusTypeDef FLASH_Utils_Read(uint32_t address, uint8_t *data,
		uint32_t dataSize) {
	/* No specific flash read function is needed, just memory copy */
	memcpy(data, (void*) address, dataSize);

	return HAL_OK;
}

/**
 * @brief   Validates the metadata structure CRC
 * @param   metadata: Pointer to metadata structure
 * @retval  1 if valid, 0 if invalid
 */
static uint8_t ValidateMetadataCRC(const boot_data_t *data) {

	// Размер структуры для расчета CRC (исключая само поле CRC и reserved)
	const uint32_t sizeForCRC = offsetof(boot_data_t, structCRC);

	// Расчет CRC структуры
	uint32_t calculatedCRC = CRC_Calculate((uint8_t*) data, sizeForCRC);

	// Сравнение с сохраненным CRC
	return (calculatedCRC == data->structCRC) ? 1 : 0;
}

/**
 * @brief   Checks if application is up to date based on metadata
 * @param   None
 * @retval  1 if up to date, 0 if not
 */
meta_valid_t FLASH_Utils_IsAppUpToDate(void) {
	boot_data_t data;

	/* Read the metadata */
	if (Bootloader_LoadStatus(&data) != HAL_OK)
		return not_read_meta;

	/* Validate metadata structure integrity */
	if (!ValidateMetadataCRC(&data))
		return not_valid_crc_meta;

	/* Check if metadata indicates valid app */
	if (data.metadata.update == 1)
		return need_update;

	/* Validate the actual firmware */
	if (!Bootloader_ValidateApplication())
		return not_valid_app;

	/* Validate firmware CRC */
	uint32_t calculatedCRC = CRC_Calculate( (uint8_t*) (MAIN_PROGRAM_START_ADDRESS), data.metadata.firmwareSize);
	if (calculatedCRC != data.metadata.firmwareCRC)
		return not_valid_crc_app;

	return 1;
}
