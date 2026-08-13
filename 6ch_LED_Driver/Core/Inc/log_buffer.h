#ifndef INC_LOG_BUFFER_H_
#define INC_LOG_BUFFER_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------------------------
// Кольцевой RAM-буфер, дублирующий байт-в-байт всё, что STM_LOG/STM_LOG_xx
// (Logger_Log/Logger_Log_xx, см. logger.cpp) отправляют в отладочный UART.
// Позволяет забрать недавнюю историю лога по TCP (см. cmd 28 в device_API.cpp)
// без физического доступа к UART. Чисто RAM/volatile — во flash не пишется.
// ---------------------------------------------------------------------------
#define LOG_BUFFER_SIZE 4096u

// Сбросить head/total (сам массив уже обнулён загрузчиком, вызов для
// явности). Вызывается один раз из Logger_Init().
void LogBuffer_Init(void);

// Добавить сырые байты (те же, что уходят в UART) с перезаписью старых
// данных при переполнении. ISR-safe: короткая критическая секция
// (PRIMASK save/disable/restore, как freeSlotAtomic в logger.cpp) — вызывается
// из Logger_Log/Logger_Log_xx как из задач, так и из прерываний.
void LogBuffer_Append(const char *data, uint16_t length);

// Неразрушающий снимок буфера в линейном порядке (от старых данных к
// новым) в out_buf. Только task context. Возвращает реальное число
// скопированных байт (<= out_buf_size и <= LOG_BUFFER_SIZE).
uint32_t LogBuffer_Snapshot(char *out_buf, uint32_t out_buf_size);

#ifdef __cplusplus
}
#endif

#endif /* INC_LOG_BUFFER_H_ */
