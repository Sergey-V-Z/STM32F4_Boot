/**
 * @file tcp_server.cpp
 * @brief Реализация TCP-сервера для обновления прошивки
 */

#include "tcp_server.h"
#include "firmware_update.h"
#include "crc.h"
#include <string.h>

/* Приватные переменные */
static TCPServerState g_serverState = {0};
static uint8_t g_rxBuffer[TCP_RX_BUFFER_SIZE];
static uint8_t g_txBuffer[TCP_TX_BUFFER_SIZE];

/* Прототипы приватных функций */
static void TCPServerTask(void *pvParameters);
static uint8_t ProcessClientRequest(struct netconn *client);
static void SendResponse(struct netconn *client, uint32_t status, uint32_t error);

/**
 * @brief Инициализирует TCP-сервер обновления прошивки
 *
 * @return uint8_t 0 при успехе, код ошибки при неудаче
 */
uint8_t FirmwareUpdateServer_Init(void) {
    // Инициализируем структуру состояния
    memset(&g_serverState, 0, sizeof(g_serverState));

    // Инициализируем буферы
    memset(g_rxBuffer, 0, TCP_RX_BUFFER_SIZE);
    memset(g_txBuffer, 0, TCP_TX_BUFFER_SIZE);

    STM_LOG("Firmware Update TCP Server initialized");
    return 0;
}

/**
 * @brief Деинициализирует TCP-сервер
 */
void FirmwareUpdateServer_Deinit(void) {
    FirmwareUpdateServer_Stop();
    STM_LOG("Firmware Update TCP Server deinitialized");
}

/**
 * @brief Запускает задачу TCP-сервера
 *
 * @return uint8_t 0 при успехе, код ошибки при неудаче
 */
uint8_t FirmwareUpdateServer_Start(void) {
    // Проверяем, что сервер еще не запущен
    if (g_serverState.isRunning) {
        return 1;
    }

    // Диагностика heap перед созданием задачи
    STM_LOG("Free heap before TCP task: %d bytes", (int)xPortGetFreeHeapSize());

    // Создаем задачу TCP-сервера
    BaseType_t result = xTaskCreate(
        TCPServerTask,           // Функция задачи
        "FWUpdateServer",        // Имя задачи
        512,                     // Размер стека в словах
        NULL,                    // Параметры задачи
        osPriorityNormal,        // Приоритет
        &g_serverState.serverTaskHandle  // Указатель на дескриптор задачи
    );

    if (result != pdPASS) {
        STM_LOG("Failed to create TCP server task (free heap: %d)", (int)xPortGetFreeHeapSize());
        return 2;
    }

    STM_LOG("Firmware Update TCP Server started");
    return 0;
}

/**
 * @brief Останавливает задачу TCP-сервера
 */
void FirmwareUpdateServer_Stop(void) {
    // Проверяем, что сервер запущен
    if (!g_serverState.isRunning) {
        return;
    }

    // Сбрасываем флаг работы, задача сама завершится
    g_serverState.isRunning = 0;

    // Ждем завершения задачи
    vTaskDelay(pdMS_TO_TICKS(100));

    // Если задача не завершилась, удаляем ее
    if (g_serverState.serverTaskHandle != NULL) {
        vTaskDelete(g_serverState.serverTaskHandle);
        g_serverState.serverTaskHandle = NULL;
    }

    // Закрываем соединение, если оно еще открыто
    if (g_serverState.conn != NULL) {
        netconn_close(g_serverState.conn);
        netconn_delete(g_serverState.conn);
        g_serverState.conn = NULL;
    }

    STM_LOG("Firmware Update TCP Server stopped");
}

/**
 * @brief Получает текущий статус сервера
 *
 * @return uint8_t 1 если сервер запущен, 0 иначе
 */
uint8_t FirmwareUpdateServer_IsRunning(void) {
    return g_serverState.isRunning;
}

/**
 * @brief Задача TCP-сервера
 *
 * @param pvParameters Параметры задачи (не используются)
 */
static void TCPServerTask(void *pvParameters) {
    err_t err;
    struct netconn *client;

    // Устанавливаем флаг работы сервера
    g_serverState.isRunning = 1;

    // Создаем новое TCP-соединение
    g_serverState.conn = netconn_new(NETCONN_TCP);
    if (g_serverState.conn == NULL) {
        STM_LOG("Failed to create new TCP connection");
        g_serverState.isRunning = 0;
        vTaskDelete(NULL);
        return;
    }

    // Привязываем соединение к порту
    err = netconn_bind(g_serverState.conn, IP_ADDR_ANY, FIRMWARE_UPDATE_PORT);
    if (err != ERR_OK) {
        STM_LOG("Failed to bind TCP connection to port %d", FIRMWARE_UPDATE_PORT);
        netconn_delete(g_serverState.conn);
        g_serverState.conn = NULL;
        g_serverState.isRunning = 0;
        vTaskDelete(NULL);
        return;
    }

    // Устанавливаем соединение в режим прослушивания
    err = netconn_listen(g_serverState.conn);
    if (err != ERR_OK) {
        STM_LOG("Failed to set TCP connection to listen mode");
        netconn_close(g_serverState.conn);
        netconn_delete(g_serverState.conn);
        g_serverState.conn = NULL;
        g_serverState.isRunning = 0;
        vTaskDelete(NULL);
        return;
    }

    STM_LOG("TCP server listening on port %d", FIRMWARE_UPDATE_PORT);

    // Основной цикл обработки подключений
    while (g_serverState.isRunning) {
        // Ожидаем подключения клиента
        err = netconn_accept(g_serverState.conn, &client);

        if (err == ERR_OK) {
            STM_LOG("Client connected to firmware update server");

            // Обрабатываем запросы клиента до закрытия соединения
            while (ProcessClientRequest(client) == 0 && g_serverState.isRunning) {
                // Просто продолжаем цикл, пока клиент не отключится
                // или не произойдет ошибка
                vTaskDelay(pdMS_TO_TICKS(1)); // Небольшая задержка для освобождения процессора
            }

            // Закрываем и удаляем соединение с клиентом
            netconn_close(client);
            netconn_delete(client);
            STM_LOG("Client disconnected from firmware update server");
        } else {
            // Проверяем, что сервер все еще должен работать
            if (!g_serverState.isRunning) {
                break;
            }

            STM_LOG("Failed to accept client connection: %d", err);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    // Закрываем и удаляем основное соединение
    netconn_close(g_serverState.conn);
    netconn_delete(g_serverState.conn);
    g_serverState.conn = NULL;
    g_serverState.isRunning = 0;
    g_serverState.serverTaskHandle = NULL;

    STM_LOG("TCP server task terminated");
    vTaskDelete(NULL);
}

/**
 * @brief Обрабатывает запрос клиента
 *
 * @param client Указатель на соединение с клиентом
 * @return uint8_t 0 при успехе, код ошибки при неудаче
 */
static uint8_t ProcessClientRequest(struct netconn *client) {
    err_t err;
    struct netbuf *netbuf;
    void *data;
    uint16_t data_len;
    PacketHeader *header;
    uint8_t result = 0;
    uint8_t command;
    uint32_t block_number;
    uint32_t data_size;
    uint32_t crc;

    // Принимаем данные от клиента
    err = netconn_recv(client, &netbuf);
    if (err != ERR_OK) {
        if (err == ERR_TIMEOUT) {
            // Тайм-аут не считается ошибкой
            return 0;
        }
        STM_LOG("Failed to receive data from client: %d", err);
        return 1;
    }

    // Получаем указатель на данные и их размер
    netbuf_data(netbuf, &data, &data_len);

    // Минимальный размер пакета - размер заголовка
    if (data_len < PACKET_HEADER_SIZE) {
        STM_LOG("Received packet is too small: %u bytes", data_len);
        netbuf_delete(netbuf);
        return 2;
    }

    // Копируем данные в буфер для обработки
    if (data_len > TCP_RX_BUFFER_SIZE) {
        STM_LOG("Received packet is too large: %u bytes", data_len);
        netbuf_delete(netbuf);
        return 3;
    }
    memcpy(g_rxBuffer, data, data_len);

    // Освобождаем сетевой буфер, так как данные уже скопированы
    netbuf_delete(netbuf);

    // Разбираем заголовок
    header = (PacketHeader *)g_rxBuffer;
    command = header->command;
    block_number = header->block_number;
    data_size = header->size;

    // Проверяем валидность команды
    if (command < CMD_PING || command > CMD_REBOOT) {
        STM_LOG("Invalid command: %u", command);
        SendResponse(client, UPDATE_STATUS_ERROR, UPDATE_ERROR_INVALID_CMD);
        return 4;
    }

    //STM_LOG("Received command: %u, Block: %lu, Size: %lu", command, block_number, data_size);

    // Обрабатываем команду
    switch (command) {
        case CMD_PING:
            // Простая проверка связи
            SendResponse(client, UPDATE_STATUS_IDLE, 0);
            break;

        case CMD_START_UPDATE:
        case CMD_START_BACKUP_UPDATE:
        {
            // Извлекаем размер и версию прошивки из payload (новый протокол settings_tool)
            // Заголовок: command(4) + size(4) + block_number(4), size содержит размер payload
            uint32_t fw_size = 0;
            uint32_t fw_version = 0;
            uint32_t payload_size = data_size; // data_size = header.size = размер payload в новом протоколе

            if (data_len >= PACKET_HEADER_SIZE + sizeof(meta_t) + sizeof(FWUpdateParams)
                && payload_size >= sizeof(meta_t) + sizeof(FWUpdateParams))
            {
                // Новый протокол: meta_t + FWUpdateParams в payload
                const meta_t *meta = (const meta_t*)(g_rxBuffer + PACKET_HEADER_SIZE);
                const FWUpdateParams *params = (const FWUpdateParams*)(g_rxBuffer + PACKET_HEADER_SIZE + sizeof(meta_t));
                fw_size    = params->fw_size;
                fw_version = meta->version;
                STM_LOG("New protocol: fw_size=%lu, version=0x%08lX", fw_size, fw_version);
            }
            else
            {
                // Старый протокол: размер в header.size, версия в header.block_number
                fw_size    = data_size;
                fw_version = block_number;
                STM_LOG("Old protocol: fw_size=%lu, version=0x%08lX", fw_size, fw_version);
            }

            if (command == CMD_START_UPDATE) {
                result = FirmwareUpdate_StartUpdate(fw_size, fw_version);
            } else {
                result = FirmwareUpdate_StartBackupUpdate(fw_size, fw_version);
            }

            if (result == UPDATE_ERROR_NONE) {
                SendResponse(client, UPDATE_STATUS_IN_PROGRESS, UPDATE_ERROR_NONE);
            } else {
                SendResponse(client, UPDATE_STATUS_ERROR, result);
            }
            break;
        }

        case CMD_FIRMWARE_DATA:
            {
                // Определяем смещение до данных
                uint8_t *data_ptr = g_rxBuffer + PACKET_HEADER_SIZE;

                // Проверяем размер пакета
                if (data_len < PACKET_HEADER_SIZE + data_size) {
                    STM_LOG("Invalid FIRMWARE_DATA packet size: %u bytes, expected: %lu bytes",
                           data_len, PACKET_HEADER_SIZE + data_size);
                    SendResponse(client, UPDATE_STATUS_ERROR, UPDATE_ERROR_INVALID_SIZE);
                    result = 5;
                    break;
                }

                // Вычисляем и проверяем CRC блока
                crc = crc32_calculate(data_ptr, data_size, 0);

                // Обрабатываем блок данных прошивки
                result = FirmwareUpdate_ProcessDataBlock(
                    block_number,
                    data_ptr,
                    data_size,
                    crc
                );

                if (result == UPDATE_ERROR_NONE) {
                    SendResponse(client, UPDATE_STATUS_IN_PROGRESS, UPDATE_ERROR_NONE);
                } else {
                    SendResponse(client, UPDATE_STATUS_ERROR, result);
                }
            }
            break;

        case CMD_END_UPDATE:
            // Завершаем процесс обновления
            crc = block_number; // Используем поле block_number для передачи CRC
            result = FirmwareUpdate_EndUpdate(crc);
            if (result == UPDATE_ERROR_NONE) {
                SendResponse(client, UPDATE_STATUS_COMPLETE, UPDATE_ERROR_NONE);
            } else {
                SendResponse(client, UPDATE_STATUS_ERROR, result);
            }
            break;

        case CMD_GET_STATUS:
            {
                uint32_t error;
                uint32_t status = FirmwareUpdate_GetStatus(&error);
                SendResponse(client, status, error);
            }
            break;

        case CMD_ABORT_UPDATE:
            // Отменяем процесс обновления
            result = FirmwareUpdate_AbortUpdate();
            if (result == UPDATE_ERROR_NONE) {
                SendResponse(client, UPDATE_STATUS_IDLE, UPDATE_ERROR_NONE);
            } else {
                SendResponse(client, UPDATE_STATUS_ERROR, result);
            }
            break;

        case CMD_GET_METADATA:
            {
                // Читаем метаданные из текущей прошивки
                meta_t* metadata = (meta_t*)METADATA_ADDRESS;

                // Создаем расширенный ответ
                MetadataResponsePacket* response = (MetadataResponsePacket*)g_txBuffer;
                response->command = CMD_RESPONSE;
                response->status = UPDATE_STATUS_IDLE;
                response->error = UPDATE_ERROR_NONE;

                // Копируем метаданные, проверяя валидность
                if (metadata->key_start == METADATA_KEY) {
                    memcpy(&response->metadata, metadata, sizeof(meta_t));
                    STM_LOG("Metadata retrieved: %s v%lu",
                            response->metadata.name_proj,
                            response->metadata.version);
                } else {
                    // Если метаданные не найдены, заполняем пустыми значениями
                    memset(&response->metadata, 0, sizeof(meta_t));
                    STM_LOG("No valid metadata found, sending empty response");
                }

                // Отправляем расширенный ответ
                err_t err = netconn_write(client, g_txBuffer, METADATA_RESPONSE_SIZE, NETCONN_COPY);
                if (err != ERR_OK) {
                    STM_LOG("Failed to send metadata response: %d", err);
                }
            }
            break;

        case CMD_REBOOT:
        {
        	//SendResponse(client, UPDATE_STATUS_READY_REBOOT, UPDATE_ERROR_NONE);

            ResponsePacket *response = (ResponsePacket *)g_txBuffer;

            // Заполняем структуру ответа
            response->command = CMD_RESPONSE;
            response->status = UPDATE_STATUS_READY_REBOOT;
            response->error = UPDATE_ERROR_NONE;
            response->reserv = 0xAAAAAAAA; // Инициализируем резервное поле

            //STM_LOG("Sending response: Status=%u, Error=0x%08lX",status, error);

            // Отправляем ответ клиенту
            size_t bytes_written = 0;
            err_t err = netconn_write_partly(client, g_txBuffer, RESPONSE_PACKET_SIZE, NETCONN_COPY, &bytes_written);
            //err_t err = netconn_write(client, g_txBuffer, RESPONSE_PACKET_SIZE, NETCONN_COPY);
            if (err != ERR_OK) {
                STM_LOG("Failed to send response to client: %d", err);
            } else {
                STM_LOG("Response sent successfully");
            }

    		STM_LOG("Rebooting...");
    		osDelay(3000);
            // Закрываем и удаляем соединение с клиентом
            netconn_close(client);
            netconn_delete(client);

    		NVIC_SystemReset();

            break;
        }
        default:
            STM_LOG("Unknown command: 0x%02X", command);
            SendResponse(client, UPDATE_STATUS_ERROR, UPDATE_ERROR_INVALID_CMD);
            result = 7;
            break;
    }

    return result;
}

/**
 * @brief Отправляет ответ клиенту
 *
 * @param client Указатель на соединение с клиентом
 * @param status Статус операции
 * @param error Код ошибки (если есть)
 */
static void SendResponse(struct netconn *client, uint32_t status, uint32_t error) {
    ResponsePacket *response = (ResponsePacket *)g_txBuffer;

    // Заполняем структуру ответа
    response->command = CMD_RESPONSE;
    response->status = status;
    response->error = error;
    response->reserv = 0xAAAAAAAA; // Инициализируем резервное поле

    //STM_LOG("Sending response: Status=%u, Error=0x%08lX",status, error);

    // Отправляем ответ клиенту
    err_t err = netconn_write(client, g_txBuffer, RESPONSE_PACKET_SIZE, NETCONN_COPY);
    if (err != ERR_OK) {
        STM_LOG("Failed to send response to client: %d", err);
    } else {
        //STM_LOG("Response sent successfully");
    }
}
