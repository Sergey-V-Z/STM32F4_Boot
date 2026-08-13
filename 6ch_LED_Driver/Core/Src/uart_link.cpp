#include "uart_link.h"
#include "main.h"
#include "cmsis_os.h"
#include "cJSON.h"
#include <cstring>

// Общие обработчики JSON-команд (определены в freertos.cpp).
// Используются как для TCP (eth_Task), так и для UART (см. ниже) —
// при tcp_conn == nullptr ответ уходит обратно через UART_Link_SendResponse().
void action_ip(cJSON *obj, bool save);
void action_commands(cJSON *obj, struct netconn *tcp_conn = nullptr);
void action_settings_data(cJSON *obj, struct netconn *tcp_conn = nullptr);

// Очередь "пришло новое сообщение по UART6" (создаётся в MX_FREERTOS_Init, freertos.cpp)
extern osMessageQId rxDataUART6Handle;

/* ---------------------------------------------------------------------------
 * Буферы и состояние приёмника UART6.
 * Используются только в этом файле — наружу ничего не отдаётся.
 * ------------------------------------------------------------------------- */

// Буфер DMA: сюда HAL_UARTEx_ReceiveToIdle_DMA непрерывно пишет принятые байты
static uint8_t UART_debug_rx[UART6_RX_LENGTH];

// Буфер собранного сообщения (одна команда от ПК, JSON, завершается '\r' или 0)
static uint8_t message_rx[message_RX_LENGTH];

static uint16_t indx_message_rx = 0; // позиция записи в message_rx (для "склейки" по частям)
static uint16_t Start_index      = 0; // смещение в UART_debug_rx, накопленное с прошлого события DMA

/* ---------------------------------------------------------------------------
 * UART_Link_Init — запуск приёма по UART6
 * ------------------------------------------------------------------------- */
void UART_Link_Init(void)
{
    HAL_UARTEx_ReceiveToIdle_DMA(&huart6, UART_debug_rx, UART6_RX_LENGTH);
    __HAL_DMA_DISABLE_IT(&hdma_usart6_rx, DMA_IT_HT);
}

/* ---------------------------------------------------------------------------
 * HAL_UARTEx_RxEventCallback — кадрирование данных, принятых по UART6.
 *
 * HAL_UARTEx_ReceiveToIdle_DMA непрерывно принимает байты в UART_debug_rx.
 * Этот callback вызывается HAL'ом при двух событиях:
 *  - HAL_UART_RXEVENT_IDLE — на линии возникла пауза (конец посылки от ПК).
 *    Принятые с прошлого вызова байты копируются в message_rx. Если
 *    последний байт — '\r' (0x0D) или 0x00, кадр считается завершённым:
 *    message_rx нуль-терминируется и в очередь rxDataUART6Handle кладётся
 *    сигнал "есть новое сообщение" для UART_Link_Process(). Если терминатора
 *    нет, данные остаются в message_rx и ждут продолжения (TC).
 *  - HAL_UART_RXEVENT_TC — буфер UART_debug_rx заполнился целиком до паузы
 *    (длинное сообщение из нескольких частей). Данные дописываются в
 *    message_rx, кадр пока не завершён.
 *
 * После каждого события приём перезапускается (DMA не останавливается).
 * ------------------------------------------------------------------------- */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance != DBG_PORT_NAME) return;

    // Ожидаем завершения передачи DMA, прежде чем читать буфер
    while (__HAL_UART_GET_FLAG(huart, UART_FLAG_TC) != SET)
    {
    }

    uint16_t Size_Data = Size - Start_index;

    HAL_UART_RxEventTypeTypeDef rxEventType = HAL_UARTEx_GetRxEventType(huart);
    switch (rxEventType)
    {
    case HAL_UART_RXEVENT_IDLE:
        memcpy(&message_rx[indx_message_rx], &UART_debug_rx[Start_index], Size_Data);

        if ((message_rx[indx_message_rx + Size_Data - 1] == '\r') ||
            (message_rx[indx_message_rx + Size_Data - 1] == 0))
        {
            // Кадр завершён: терминируем строку и сигнализируем UART_Link_Process()
            message_rx[indx_message_rx + Size_Data] = 0;

            osStatus status = osMessagePut(rxDataUART6Handle, (uint32_t)indx_message_rx, 0);
            if (status != osOK)
            {
                // Очередь переполнена — освобождаем её и пробуем снова
                osEvent evt;
                do
                {
                    evt = osMessageGet(rxDataUART6Handle, 0);
                } while (evt.status == osEventMessage);

                status = osMessagePut(rxDataUART6Handle, (uint32_t)indx_message_rx, 0);
            }

            indx_message_rx = 0;
        }
        else
        {
            // Терминатора нет — продолжаем накопление при следующем событии
            indx_message_rx += Size_Data;
        }

        Start_index = Size;
        break;

    case HAL_UART_RXEVENT_TC:
        // Буфер заполнился до паузы на линии — дописываем и продолжаем
        memcpy(&message_rx[indx_message_rx], &UART_debug_rx[Start_index], Size_Data);
        indx_message_rx += Size_Data;
        Start_index = 0;
        break;

    default:
        STM_LOG("Неизвестный тип события UART: %d", rxEventType);
        break;
    }

    // Перезапускаем приём DMA
    HAL_UARTEx_ReceiveToIdle_DMA(huart, UART_debug_rx, UART6_RX_LENGTH);
    __HAL_DMA_DISABLE_IT(&hdma_usart6_rx, DMA_IT_HT);
}

/* ---------------------------------------------------------------------------
 * UART_Link_Process — обработка одного принятого от ПК сообщения.
 *
 * Блокируется на rxDataUART6Handle до прихода кадра от
 * HAL_UARTEx_RxEventCallback(), затем разбирает message_rx как JSON:
 *   {"id":<ID_CTRL|0>, "type_data":N, "save_settings":bool, "obj":{...}}
 * и вызывает соответствующий обработчик (общий с TCP, см. freertos.cpp):
 *   type_data=1 -> action_ip            (настройки сети)
 *   type_data=3 -> action_commands      (команды управления, ответ обратно
 *                                         уходит через UART_Link_SendResponse)
 *   type_data=4 -> action_settings_data (чтение настроек)
 * Сообщения с "id", отличным от ID_CTRL и 0 (broadcast), игнорируются.
 * ------------------------------------------------------------------------- */
void UART_Link_Process(void)
{
    // Ожидаем сообщение из очереди
    osMessageGet(rxDataUART6Handle, osWaitForever);

    cJSON *json = cJSON_Parse((char *)message_rx);
    if (json == NULL)
    {
        STM_LOG("Invalid JSON");
        return;
    }

    cJSON *id            = cJSON_GetObjectItemCaseSensitive(json, "id");
    cJSON *type_data     = cJSON_GetObjectItemCaseSensitive(json, "type_data");
    cJSON *save_settings = cJSON_GetObjectItemCaseSensitive(json, "save_settings");
    cJSON *obj           = cJSON_GetObjectItemCaseSensitive(json, "obj");

    if (!cJSON_IsNumber(id))
    {
        STM_LOG("id not valid");
        cJSON_Delete(json);
        return;
    }

    uint32_t received_id = (uint32_t)cJSON_GetNumberValue(id);

    // Принимаем сообщения для своего ID или широковещательные (id=0)
    if (received_id != ID_CTRL && received_id != 0)
    {
        STM_LOG("id not valid");
        cJSON_Delete(json);
        return;
    }

    bool save_set = cJSON_IsTrue(save_settings);

    if (!cJSON_IsNumber(type_data))
    {
        STM_LOG("Invalid type data");
        cJSON_Delete(json);
        return;
    }

    switch (type_data->valueint)
    {
    case 1: // настройки IP
        action_ip(obj, save_set);
        break;
    case 3: // команды управления
        action_commands(obj);
        break;
    case 4: // чтение настроек
        action_settings_data(obj);
        break;
    default:
        STM_LOG("data type not registered");
        break;
    }

    cJSON_Delete(json);
}

/* ---------------------------------------------------------------------------
 * UART_Link_SendResponse — отправка JSON-ответа на команду обратно на ПК.
 *
 * Используется из tcp_or_uart_send() (freertos.cpp) при tcp_conn == nullptr,
 * т.е. когда команда пришла по UART, а не по TCP. Ответ отправляется через
 * очередь логгера (Logger_Log_xx), как и обычные логи, но с терминатором
 * "\x03\x04" без ведущего "\r" — это позволяет ПК отличить JSON-ответ
 * (строка начинается с '{') от строки лога.
 * ------------------------------------------------------------------------- */
void UART_Link_SendResponse(const char *s)
{
    STM_LOG_xx("%s", s);
    STM_LOG_xx("\x03\x04");
}
