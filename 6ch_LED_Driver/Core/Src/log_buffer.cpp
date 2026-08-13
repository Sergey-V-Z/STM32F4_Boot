#include "log_buffer.h"
#include "main.h"
#include <string.h>

static char s_logBuf[LOG_BUFFER_SIZE];
static volatile uint32_t s_writeHead    = 0U; // позиция следующей записи
static volatile uint32_t s_totalWritten = 0U; // сколько байт всего записано с момента старта

void LogBuffer_Init(void)
{
    s_writeHead    = 0U;
    s_totalWritten = 0U;
}

void LogBuffer_Append(const char *data, uint16_t length)
{
    if (!data || length == 0U) return;
    if (length > LOG_BUFFER_SIZE) length = LOG_BUFFER_SIZE; // защита от неправдоподобной длины

    // Та же схема короткой ISR-safe критической секции, что в freeSlotAtomic
    // (logger.cpp) — Logger_Log/Logger_Log_xx вызываются и из задач, и из
    // прерываний, вся операция (включая memcpy) должна быть атомарной.
    uint32_t primask_bit = __get_PRIMASK();
    __disable_irq();

    uint32_t head = s_writeHead;
    uint32_t tail_space = LOG_BUFFER_SIZE - head;

    if (length <= tail_space)
    {
        memcpy(&s_logBuf[head], data, length);
    }
    else
    {
        memcpy(&s_logBuf[head], data, tail_space);
        memcpy(&s_logBuf[0], data + tail_space, length - tail_space);
    }

    s_writeHead = (head + length) % LOG_BUFFER_SIZE;
    s_totalWritten += length;

    if (!primask_bit) __enable_irq();
}

uint32_t LogBuffer_Snapshot(char *out_buf, uint32_t out_buf_size)
{
    if (!out_buf || out_buf_size == 0U) return 0U;

    // Снимаем head/total коротким атомарным чтением; сам копирующий memcpy —
    // уже вне критической секции (не держим IRQ выключенными на весь объём).
    uint32_t primask_bit = __get_PRIMASK();
    __disable_irq();
    uint32_t head  = s_writeHead;
    uint32_t total = s_totalWritten;
    if (!primask_bit) __enable_irq();

    uint32_t valid = (total < LOG_BUFFER_SIZE) ? total : LOG_BUFFER_SIZE;
    // Пока буфер ни разу не переполнялся, самые старые данные лежат с
    // начала массива; после переполнения head одновременно "следующая
    // позиция записи" и "позиция самого старого валидного байта".
    uint32_t start = (total < LOG_BUFFER_SIZE) ? 0U : head;

    if (valid > out_buf_size)
    {
        // Буфер вызывающего меньше доступных данных — оставляем самые
        // свежие байты, отбрасывая лишние старые.
        uint32_t skip = valid - out_buf_size;
        start = (start + skip) % LOG_BUFFER_SIZE;
        valid = out_buf_size;
    }

    uint32_t first_chunk = LOG_BUFFER_SIZE - start;
    if (first_chunk >= valid)
    {
        memcpy(out_buf, &s_logBuf[start], valid);
    }
    else
    {
        memcpy(out_buf, &s_logBuf[start], first_chunk);
        memcpy(out_buf + first_chunk, &s_logBuf[0], valid - first_chunk);
    }

    return valid;
}
