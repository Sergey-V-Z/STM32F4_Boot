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
#define FLASH_COPY_BUFFER_SIZE   1024  /* Размер буфера для копирования прошивки */

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
boot_data_t BootloaderData; // Глобальная переменная для доступа из других модулей
static Recovery_Reason_t RecoveryReason = RECOVERY_REASON_NONE;
static uint8_t FirmwareBuffer[FLASH_COPY_BUFFER_SIZE];
static W25QXX_Device_t *pSpiFlash = NULL; // Глобальная ссылка на устройство SPI Flash

/* Private function prototypes -----------------------------------------------*/
static void Bootloader_InitStatus(boot_data_t *boot_data);
static void Bootloader_EnterRecoveryMode(Recovery_Reason_t reason);
static int Bootloader_ProcessState(void);
static void Bootloader_ResetApplication(void);
static uint8_t Bootloader_ValidateApplication(void);
static uint8_t Bootloader_CheckAndInitFlash(void);
static HAL_StatusTypeDef Bootloader_CopyFirmwareFromSPI( uint32_t externalFlashAddress, uint32_t internalFlashAddress, uint32_t firmwareSize);
static uint8_t Bootloader_VerifyApplication(uint32_t firmwareSize, uint32_t expectedCRC);
static void Bootloader_UpdateProgress(uint32_t progress, uint32_t total);
static void Bootloader_IndicateError(Bootloader_Error_t error);

/* Functions -----------------------------------------------------------------*/

/**
 * @brief  Initializes the bootloader.
 * @param  None
 * @retval None
 */
void Bootloader_Init(void) {
	/* Initialize status to default values */
	//Bootloader_InitStatus(&BootloaderData);

	/* Initialize Flash utilities */
	if (FLASH_Utils_Unlock() != HAL_OK) {
		BootloaderData.status.State = BOOTLOADER_STATE_ERROR;
		BootloaderData.status.ErrorCode = BOOTLOADER_ERROR_FLASH_INIT_FAILED;
		return;
	}
	FLASH_Utils_Lock();

	/* Загрузка статуса загрузчика из области BOOT_DATA */
	boot_data_valid_t ret = Bootloader_LoadStatus(&BootloaderData);
	if ((ret == ERR_read_boot_data) || (ret == ERR_nuul_ptr)){
		HAL_Delay(3000);
		// перезагрузится
		NVIC_SystemReset();
	}else if((ret == ERR_crc_boot_data) || (ret == EPMTY_boot_data)){
		Bootloader_InitStatus(&BootloaderData); // Инициализируем значениями по умолчанию
		//Bootloader_SaveStatus(&BootloaderData);
	}
	else // прочитали успешно
	{
		/* Validate the actual firmware */
		if (Bootloader_ValidateApplication()) {
			/*
			 * //Validate firmware CRC
			uint32_t calculatedCRC = CRC_Calculate( (uint8_t*) (MAIN_PROGRAM_START_ADDRESS), BootloaderData.metadata.firmwareSize);
			if (calculatedCRC != BootloaderData.metadata.firmwareCRC)
			{
				// перейти в режим восстановления
				Bootloader_EnterRecoveryMode(RECOVERY_REASON_INVALID_APP);
				return;
			}*/
			// с прошивкой все ок

			/* Check if metadata indicates valid app */
			if (BootloaderData.metadata.update == 1){
				// перейти в режим обновления
				Bootloader_EnterRecoveryMode(RECOVERY_REASON_APP_UPDATE);
				return;
			}
			else {
				// проверить количество ошибок если превысили то перейти в режим восстановления
				if (BootloaderData.reset_counter >= MAX_RESET_COUNT) {
					Bootloader_EnterRecoveryMode(RECOVERY_REASON_RESET_COUNT);
					return;
				}

				// перейти в приложение
				Bootloader_JumpToApplication();
			}
		}
		else {
			// перейти в режим восстановления
			Bootloader_EnterRecoveryMode(RECOVERY_REASON_NO_APP);
			return;
		}
	}



	/* Сохраняем обновленный статус */
	//Bootloader_SaveStatus(&BootloaderData);
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
 * @brief  Initializes bootloader status to default values and calculates CRC.
 * @param  boot_data: Pointer to boot_data_t structure to initialize
 * @retval None
 */
static void Bootloader_InitStatus(boot_data_t *boot_data) {
    if (boot_data == NULL) {
        return;
    }

    // Инициализация структуры статуса загрузчика
    boot_data->status.State = BOOTLOADER_STATE_INIT;
    boot_data->status.ErrorCode = BOOTLOADER_ERROR_NONE;
    boot_data->status.ResetCount = 0;
    boot_data->status.IsAppValid = 0;
    boot_data->status.IsRecovery = 0;
    boot_data->status.LastFlashAddress = 0;
    boot_data->status.BytesFlashed = 0;

    // Инициализация метаданных приложения
    boot_data->metadata.firmwareCRC = 0;
    boot_data->metadata.firmwareSize = 0;
    boot_data->metadata.firmwareVersion = 0;
    boot_data->metadata.update = 0;
    boot_data->metadata.reserved = 0;

    // Инициализация SPI Flash memory layout
    boot_data->SPI_Flash_Layout.ConfigAreaAddress = SPI_FLASH_CONFIG_ADDRESS;
    boot_data->SPI_Flash_Layout.ConfigAreaSize = SPI_FLASH_CONFIG_SIZE;
    boot_data->SPI_Flash_Layout.MainFirmwareAddress = SPI_FLASH_MAIN_FW_ADDRESS;
    boot_data->SPI_Flash_Layout.MainFirmwareSize = SPI_FLASH_MAIN_FW_SIZE;
    boot_data->SPI_Flash_Layout.BackupFirmwareAddress = SPI_FLASH_BACKUP_FW_ADDRESS;
    boot_data->SPI_Flash_Layout.BackupFirmwareSize = SPI_FLASH_BACKUP_FW_SIZE;
    boot_data->SPI_Flash_Layout.AppDataAddress = SPI_FLASH_APP_DATA_ADDRESS;
    boot_data->SPI_Flash_Layout.AppDataSize = SPI_FLASH_APP_DATA_SIZE;

    // Инициализация firmware metadata
    boot_data->SPI_Flash_Firmware_Metadata.FirmwareVersion = 0;
    boot_data->SPI_Flash_Firmware_Metadata.FirmwareSize = 0;
    boot_data->SPI_Flash_Firmware_Metadata.FirmwareCRC = 0;
    boot_data->SPI_Flash_Firmware_Metadata.TargetAddress = MAIN_PROGRAM_START_ADDRESS;

    // Инициализация зарезервированных полей
    for (int i = 0; i < 4; i++) {
        boot_data->SPI_Flash_Firmware_Metadata.Reserved[i] = 0;
    }

    // Инициализация других полей
    boot_data->flashInitialized = 0;
    boot_data->reset_counter = 0;

    // Временно обнуляем CRC перед его вычислением
    boot_data->structCRC = 0;

    // Расчет CRC структуры (исключая само поле CRC)
    const uint32_t sizeForCRC = offsetof(boot_data_t, structCRC);
    boot_data->structCRC = CRC_Calculate((uint8_t*)boot_data, sizeForCRC);
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

	// Вычисляем CRC для структуры (без самого поля CRC)
	boot_data_t tempData = *data;
	tempData.structCRC = 0;

	uint32_t calculatedCRC = CRC_Calculate((uint8_t*) &tempData,
			offsetof(boot_data_t, structCRC));
	tempData.structCRC = calculatedCRC;

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
	status = FLASH_Utils_Write(BOOT_DATA_ADDRESS, (const uint8_t*) &tempData,
			sizeof(boot_data_t));

	FLASH_Utils_Lock();

	return status;
}

/**
 * @brief  Loads the bootloader status from BOOT_DATA area.
 * @param  None
 * @retval HAL status
 */
boot_data_valid_t Bootloader_LoadStatus(boot_data_t *boot_data) {
	if(boot_data == NULL){
		return ERR_nuul_ptr;
	}

	boot_data_valid_t result = OK_boot_data;

	uint32_t statusValue = *((uint32_t*) BOOT_DATA_ADDRESS);

	/* Проверяем, что область не пустая (не стертая) */
	if (statusValue == 0xFFFFFFFF) {
		/* Область пустая, используем значения по умолчанию */
		// записать по умолчанию
		return EPMTY_boot_data;
	}

	/* Загружаем статус из флеш-памяти */
	if(FLASH_Utils_Read(BOOT_DATA_ADDRESS, (uint8_t*) boot_data, sizeof(boot_data_t))) {
		return ERR_read_boot_data;
	}

	// Проверка валидности загруженной структуры по CRC
	uint32_t storedCRC = boot_data->structCRC;
	boot_data->structCRC = 0;

	uint32_t calculatedCRC = CRC_Calculate((uint8_t*) boot_data, offsetof(boot_data_t, structCRC));

	// Восстанавливаем значение CRC
	boot_data->structCRC = storedCRC;

	if (calculatedCRC != storedCRC) {
		// CRC не совпадает, данные не валидны
		//Bootloader_InitStatus(); // Инициализируем значениями по умолчанию
		return ERR_crc_boot_data;
	}

	return result;
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
	//Bootloader_SaveStatus(&BootloaderData);
}

/**
 * @brief  Processes the current bootloader state.
 * @param  None
 * @retval None
 */
static int Bootloader_ProcessState(void) {

	/* Инициализируем периферию, если не инициализирована */
	MX_GPIO_Init();
	MX_SPI3_Init();

	/* Индикация режима восстановления */
	HAL_GPIO_WritePin(G_GPIO_Port, G_Pin, GPIO_PIN_RESET); // Зеленый индикатор
	HAL_GPIO_WritePin(R_GPIO_Port, R_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(B_GPIO_Port, B_Pin, GPIO_PIN_SET);

	/* Инициализация SPI Flash */
	if (!Bootloader_CheckAndInitFlash()) {
		BootloaderData.status.State = BOOTLOADER_STATE_ERROR;
		BootloaderData.status.ErrorCode = BOOTLOADER_ERROR_FLASH_INIT_FAILED;
		Bootloader_SaveStatus(&BootloaderData);
		return -1;
	}

	/* Индикация начала процесса обновления */
	HAL_GPIO_WritePin(B_GPIO_Port, B_Pin, GPIO_PIN_RESET); // Синий индикатор

	/* Проверяем причину входа в режим восстановления */
	if (RecoveryReason == RECOVERY_REASON_APP_UPDATE) {
		/* Обновление запрошено приложением, используем метаданные из boot_data_t */

		/* Проверка размера прошивки */
		if (BootloaderData.metadata.firmwareSize == 0 || BootloaderData.metadata.firmwareSize > MAIN_PROGRAM_SIZE) {
			BootloaderData.status.State = BOOTLOADER_STATE_ERROR;
			BootloaderData.status.ErrorCode = BOOTLOADER_ERROR_APP_INVALID;
			Bootloader_SaveStatus(&BootloaderData);
			return -1;
		}

		/* Стираем область приложения */
		HAL_GPIO_WritePin(R_GPIO_Port, R_Pin, GPIO_PIN_RESET); // Красный индикатор - стирание
		HAL_GPIO_WritePin(G_GPIO_Port, G_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(B_GPIO_Port, B_Pin, GPIO_PIN_SET);

		Bootloader_ResetApplication();

		/* Индикация процесса копирования */
		HAL_GPIO_WritePin(R_GPIO_Port, R_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(G_GPIO_Port, G_Pin, GPIO_PIN_RESET); // Зеленый индикатор - копирование
		HAL_GPIO_WritePin(B_GPIO_Port, B_Pin, GPIO_PIN_SET);

		/* Копируем прошивку из внешней флеш во внутреннюю */
		if (Bootloader_CopyFirmwareFromSPI(SPI_FLASH_MAIN_FW_ADDRESS, MAIN_PROGRAM_START_ADDRESS, BootloaderData.metadata.firmwareSize) != HAL_OK) {
			BootloaderData.status.State = BOOTLOADER_STATE_ERROR;
			BootloaderData.status.ErrorCode =
					BOOTLOADER_ERROR_FLASH_WRITE_FAILED;
			Bootloader_SaveStatus(&BootloaderData);
			return -1;
		}

		/* Верификация скопированной прошивки */
		HAL_GPIO_WritePin(R_GPIO_Port, R_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(G_GPIO_Port, G_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(B_GPIO_Port, B_Pin, GPIO_PIN_RESET); // Синий индикатор - верификация

		if (!Bootloader_VerifyApplication(BootloaderData.metadata.firmwareSize, BootloaderData.metadata.firmwareCRC)) {
			BootloaderData.status.State = BOOTLOADER_STATE_ERROR;
			BootloaderData.status.ErrorCode = BOOTLOADER_ERROR_CRC_FAILED;
			Bootloader_SaveStatus(&BootloaderData);
			return -1;
		}

		/* Индикация успешного обновления */
		HAL_GPIO_WritePin(R_GPIO_Port, R_Pin, GPIO_PIN_RESET); // Все индикаторы
		HAL_GPIO_WritePin(G_GPIO_Port, G_Pin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(B_GPIO_Port, B_Pin, GPIO_PIN_RESET);
		HAL_Delay(1000);

		/* Обновление успешно, сбрасываем флаг обновления */
		BootloaderData.metadata.update = 0;
		BootloaderData.status.IsAppValid = 1;
		//BootloaderData.status.State = BOOTLOADER_STATE_JUMP_TO_APP;
		Bootloader_SaveStatus(&BootloaderData);

		/* Перезагрузка для запуска обновленного приложения */
		NVIC_SystemReset();
	} else if (Bootloader_CheckBackupFirmware()) {
		/* Резервная прошивка найдена, используем её для восстановления */

		/* Стираем область приложения */
		HAL_GPIO_WritePin(R_GPIO_Port, R_Pin, GPIO_PIN_RESET); // Красный индикатор - стирание
		HAL_GPIO_WritePin(G_GPIO_Port, G_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(B_GPIO_Port, B_Pin, GPIO_PIN_SET);

		Bootloader_ResetApplication();

		/* Индикация процесса копирования */
		HAL_GPIO_WritePin(R_GPIO_Port, R_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(G_GPIO_Port, G_Pin, GPIO_PIN_RESET); // Зеленый индикатор - копирование
		HAL_GPIO_WritePin(B_GPIO_Port, B_Pin, GPIO_PIN_SET);

		/* Копируем резервную прошивку из внешней флеш во внутреннюю */
		/* Для простоты предполагаем стандартный размер прошивки */
		uint32_t backup_size = 0x40000; // 256KB - пример

		if (Bootloader_CopyFirmwareFromSPI(SPI_FLASH_BACKUP_FW_ADDRESS, MAIN_PROGRAM_START_ADDRESS, backup_size) != HAL_OK) {
			BootloaderData.status.State = BOOTLOADER_STATE_ERROR;
			BootloaderData.status.ErrorCode =
					BOOTLOADER_ERROR_FLASH_WRITE_FAILED;
			Bootloader_SaveStatus(&BootloaderData);
			return -1;
		}

		/* Проверка валидности восстановленной прошивки */
		if (!Bootloader_ValidateApplication()) {
			BootloaderData.status.State = BOOTLOADER_STATE_ERROR;
			BootloaderData.status.ErrorCode = BOOTLOADER_ERROR_APP_INVALID;
			Bootloader_SaveStatus(&BootloaderData);
			return -1;
		}

		/* Индикация успешного восстановления */
		HAL_GPIO_WritePin(R_GPIO_Port, R_Pin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(G_GPIO_Port, G_Pin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(B_GPIO_Port, B_Pin, GPIO_PIN_RESET);
		HAL_Delay(1000);

		/* Восстановление успешно, обновляем статус */
		BootloaderData.status.IsAppValid = 1;
		//BootloaderData.status.State = BOOTLOADER_STATE_JUMP_TO_APP;
		Bootloader_SaveStatus(&BootloaderData);
	} else {
		/* Резервной прошивки нет, проверяем текущую */
		if (Bootloader_ValidateApplication()) {
			/* Существующая прошивка во внутренней флеш-памяти валидна */
			BootloaderData.status.IsAppValid = 1;
			//BootloaderData.status.State = BOOTLOADER_STATE_JUMP_TO_APP;
			Bootloader_SaveStatus(&BootloaderData);
		} else {
			/* Нет валидной прошивки нигде */
			BootloaderData.status.State = BOOTLOADER_STATE_ERROR;
			BootloaderData.status.ErrorCode =
					BOOTLOADER_ERROR_RECOVERY_FAILED;

			/* Индикация ошибки - отсутствие прошивки */
			HAL_GPIO_WritePin(R_GPIO_Port, R_Pin, GPIO_PIN_RESET);
			HAL_GPIO_WritePin(G_GPIO_Port, G_Pin, GPIO_PIN_SET);
			HAL_GPIO_WritePin(B_GPIO_Port, B_Pin, GPIO_PIN_SET);
			Bootloader_SaveStatus(&BootloaderData);
		}
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
 * @brief  Verify application using CRC
 * @param  firmwareSize: Size of firmware
 * @param  expectedCRC: Expected CRC value
 * @retval 1 if valid, 0 if invalid
 */
static uint8_t Bootloader_VerifyApplication(uint32_t firmwareSize,
		uint32_t expectedCRC) {
	/* Проверка через CRC */
	uint32_t calculatedCRC = CRC_Calculate(
			(uint8_t*) MAIN_PROGRAM_START_ADDRESS, firmwareSize);

	/* Сравнение с ожидаемым CRC */
	if (calculatedCRC != expectedCRC) {
		return 0; /* CRC не совпадает */
	}

	/* Дополнительная проверка валидности приложения */
	if (!Bootloader_ValidateApplication()) {
		return 0; /* Базовая проверка не пройдена */
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
	if (pSpiFlash == NULL) {
		pSpiFlash = &w25qxx_dev;

		/* Настройка пинов для SPI Flash */
		pins_spi_t csPin = { SPI3_CS_GPIO_Port, SPI3_CS_Pin };
		pins_spi_t wpPin = { WP_GPIO_Port, WP_Pin };
		pins_spi_t holdPin = { HOLD_GPIO_Port, HOLD_Pin };

		/* Инициализация устройства SPI Flash */
		W25QXX_InitDevice(pSpiFlash);

		/* Инициализация SPI Flash с настройками */
		if (!W25QXX_Init(pSpiFlash, &hspi3, 0, csPin, wpPin, holdPin, 1)) {
			pSpiFlash = NULL;
			return 0; /* Ошибка инициализации */
		}
	}

	return 1; /* Успешная инициализация или уже инициализирована */
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

	/* Чтение первых байтов прошивки для проверки */
	uint32_t buffer[4];
	W25QXX_ReadBytes(pSpiFlash, (uint8_t*) buffer, SPI_FLASH_BACKUP_FW_ADDRESS,
			sizeof(buffer));

	/* Проверка валидности данных (проверяем указатель стека и вектор сброса) */
	uint32_t sp = buffer[0];
	uint32_t resetVector = buffer[1];

	/* Проверка, что стек указывает на область RAM */
	if (sp < 0x20000000 || sp > 0x20020000) {
		return 0; /* Недопустимый указатель стека */
	}

	/* Проверка, что вектор сброса указывает на область Flash */
	if (resetVector < MAIN_PROGRAM_START_ADDRESS
			|| resetVector > (MAIN_PROGRAM_START_ADDRESS + MAIN_PROGRAM_SIZE)) {
		return 0; /* Недопустимый вектор сброса */
	}

	/* Проверка, что нет "пустой" прошивки (все 0xFF) */
	if (buffer[0] == 0xFFFFFFFF && buffer[1] == 0xFFFFFFFF) {
		return 0; /* Прошивка пуста или стёрта */
	}

	/* Дополнительная проверка на наличие валидных векторов прерываний */
	for (int i = 0; i < 4; i++) {
		if (buffer[i] == 0xFFFFFFFF) {
			return 0; /* Вектор не задан - неполная прошивка */
		}
	}

	return 1; /* Прошивка существует и, предположительно, валидна */
}

/**
 * @brief  Копирует прошивку из внешней SPI Flash во внутреннюю Flash
 * @param  externalFlashAddress: Адрес начала прошивки во внешней Flash
 * @param  internalFlashAddress: Адрес назначения во внутренней Flash
 * @param  firmwareSize: Размер прошивки для копирования
 * @retval HAL_StatusTypeDef: Статус операции
 */
static HAL_StatusTypeDef Bootloader_CopyFirmwareFromSPI(
		uint32_t externalFlashAddress, uint32_t internalFlashAddress,
		uint32_t firmwareSize) {
	HAL_StatusTypeDef status = HAL_OK;
	uint32_t bytesLeft = firmwareSize;
	uint32_t currentExternalAddress = externalFlashAddress;
	uint32_t currentInternalAddress = internalFlashAddress;
	uint32_t blockSize;

	/* Проверяем инициализацию SPI Flash */
	if (!Bootloader_CheckAndInitFlash()) {
		return HAL_ERROR;
	}

	/* Разблокировка Flash для записи */
	FLASH_Utils_Unlock();

	/* Копирование блоками */
	while (bytesLeft > 0) {
		/* Определяем размер текущего блока */
		blockSize = (bytesLeft > FLASH_COPY_BUFFER_SIZE) ?
		FLASH_COPY_BUFFER_SIZE :
															bytesLeft;

		/* Читаем блок из внешней Flash */
		W25QXX_ReadBytes(pSpiFlash, FirmwareBuffer, currentExternalAddress,
				blockSize);

		/* Записываем блок во внутреннюю Flash */
		status = FLASH_Utils_Write(currentInternalAddress, FirmwareBuffer,
				blockSize);
		if (status != HAL_OK) {
			FLASH_Utils_Lock();
			return status;
		}

		/* Обновляем адреса и количество оставшихся байт */
		currentExternalAddress += blockSize;
		currentInternalAddress += blockSize;
		bytesLeft -= blockSize;

		/* Обновляем индикацию прогресса */
		Bootloader_UpdateProgress(firmwareSize - bytesLeft, firmwareSize);
	}

	/* Блокируем Flash */
	FLASH_Utils_Lock();

	return status;
}

/**
 * @brief  Обновляет индикацию прогресса операции
 * @param  progress: Текущий прогресс
 * @param  total: Общее количество
 * @retval None
 */
static void Bootloader_UpdateProgress(uint32_t progress, uint32_t total) {
	static uint32_t lastPercent = 0;
	uint32_t percent = (progress * 100) / total;

	/* Обновляем индикацию только при изменении процента */
	if (percent != lastPercent) {
		lastPercent = percent;

		/* Мигаем зеленым светодиодом для отображения прогресса */
		if ((percent % 10) == 0) {
			HAL_GPIO_TogglePin(G_GPIO_Port, G_Pin);
			HAL_Delay(50);
			HAL_GPIO_TogglePin(G_GPIO_Port, G_Pin);
		}
	}
}

/**
 * @brief  Индикация ошибки через светодиоды
 * @param  error: Код ошибки
 * @retval None
 */
static void Bootloader_IndicateError(Bootloader_Error_t error) {
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

	/* Дополнительно индицируем код ошибки количеством вспышек */
	for (int i = 0; i < error + 1; i++) {
		HAL_GPIO_WritePin(R_GPIO_Port, R_Pin, GPIO_PIN_RESET);
		HAL_Delay(300);
		HAL_GPIO_WritePin(R_GPIO_Port, R_Pin, GPIO_PIN_SET);
		HAL_Delay(300);
	}
	HAL_Delay(2000);
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
	uint32_t calculatedCRC = CRC_Calculate(
			(uint8_t*) (MAIN_PROGRAM_START_ADDRESS),
			data.metadata.firmwareSize);
	if (calculatedCRC != data.metadata.firmwareCRC)
		return not_valid_crc_app;

	return valid_app;
}
