/**
 * @file tcp_server.h
 * @brief Определения и интерфейсы для TCP-сервера обновления прошивки
 */

#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <stdint.h>
#include "lwip/api.h"
#include "firmware_update.h"

/**
 * @brief Порт TCP для сервера обновления прошивки
 */
#define FIRMWARE_UPDATE_PORT     8080

/**
 * @brief Максимальный размер приемного буфера для TCP-сервера
 */
#define TCP_RX_BUFFER_SIZE       1536

/**
 * @brief Максимальный размер передаваемого буфера для TCP-сервера
 */
#define TCP_TX_BUFFER_SIZE       1024

/**
 * @brief Тайм-аут для приема данных (в мс)
 */
#define TCP_RECEIVE_TIMEOUT      5000

/**
 * @brief Тайм-аут для отправки данных (в мс)
 */
#define TCP_SEND_TIMEOUT         5000

/**
 * @brief Максимальное количество одновременных подключений
 */
#define TCP_MAX_CONNECTIONS      1

/**
 * @brief Размер заголовка пакета
 */
#define PACKET_HEADER_SIZE       12 // command(4) + size(4) + block_number(4)

/**
 * @brief Размер ответного пакета
 */
#define RESPONSE_PACKET_SIZE sizeof(ResponsePacket)
#define METADATA_RESPONSE_SIZE sizeof(MetadataResponsePacket)
/**
 * @brief Структура состояния TCP-сервера
 */
typedef struct {
    uint8_t isRunning;               // Флаг работы сервера
    struct netconn *conn;            // Основное соединение сервера
    TaskHandle_t serverTaskHandle;   // Дескриптор задачи сервера
} TCPServerState;

/**
 * @brief Инициализирует TCP-сервер обновления прошивки
 *
 * @return uint8_t 0 при успехе, код ошибки при неудаче
 */
uint8_t FirmwareUpdateServer_Init(void);

/**
 * @brief Деинициализирует TCP-сервер
 */
void FirmwareUpdateServer_Deinit(void);

/**
 * @brief Запускает задачу TCP-сервера
 *
 * @return uint8_t 0 при успехе, код ошибки при неудаче
 */
uint8_t FirmwareUpdateServer_Start(void);

/**
 * @brief Останавливает задачу TCP-сервера
 */
void FirmwareUpdateServer_Stop(void);

/**
 * @brief Получает текущий статус сервера
 *
 * @return uint8_t 1 если сервер запущен, 0 иначе
 */
uint8_t FirmwareUpdateServer_IsRunning(void);

#endif // TCP_SERVER_H
