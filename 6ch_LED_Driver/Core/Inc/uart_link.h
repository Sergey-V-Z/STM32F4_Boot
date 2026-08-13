#ifndef INC_UART_LINK_H_
#define INC_UART_LINK_H_

#include "stm32f4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Обмен данными с компьютером по UART6 (USART6, см. DBG_PORT в main.h).
 *
 * По этому физическому интерфейсу передаются два типа данных:
 *
 *  1) Команды/настройки в формате JSON: {"id":.., "type_data":N, "obj":{...}}
 *     - принимаются от ПК, разбираются в UART_Link_Process() и передаются в
 *       action_ip()/action_commands()/action_settings_data() (freertos.cpp) —
 *       те же обработчики, что используются и для TCP (eth_Task);
 *     - ответ на команду отправляется обратно тоже в виде JSON-строки через
 *       UART_Link_SendResponse().
 *
 *  2) Лог-сообщения прошивки (STM_LOG/STM_LOG_xx, см. logger.cpp) — обычный
 *     текст для отладки.
 *
 * Оба типа сообщений идут через один и тот же передающий буфер логгера
 * (Logger_Log_xx), поэтому на стороне ПК их нужно различать по содержимому:
 *   - строка, начинающаяся с '{' — это JSON-ответ на команду;
 *   - всё остальное — служебный лог.
 * Терминатор лог-сообщения: "\r\x03\x04" (см. Logger_Log).
 * Терминатор JSON-ответа:   "\x03\x04" (без \r, см. UART_Link_SendResponse).
 * ------------------------------------------------------------------------- */

// Запускает приём данных по UART6 (DMA, ReceiveToIdle).
// Вызывается один раз перед циклом uart_Task().
void UART_Link_Init(void);

// Если от ПК пришло новое сообщение — разбирает его как JSON и вызывает
// action_ip()/action_commands()/action_settings_data() в зависимости от
// поля "type_data". Блокируется до прихода сообщения (см. реализацию).
// Вызывается в цикле uart_Task().
void UART_Link_Process(void);

// Отправляет ответ на команду обратно на ПК через UART6 (формат см. выше).
// Используется из tcp_or_uart_send() (freertos.cpp) при tcp_conn == nullptr.
void UART_Link_SendResponse(const char *s);

#ifdef __cplusplus
}
#endif

#endif /* INC_UART_LINK_H_ */
