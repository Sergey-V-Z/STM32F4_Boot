#include "main.h"
#include "log_buffer.h"

// Проверка вызова из прерывания
static uint8_t isInInterrupt(void) {
    return (SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk) != 0;
}

// Получить свободный слот из пула
static int getFreeMessageSlot(void) {
    for(int i = 0; i < QUEUE_SIZE; i++) {
        if(messagePoolUsed[i] == 0) {
            messagePoolUsed[i] = 1;
            return i;
        }
    }
    return -1;
}

// Атомарное освобождение слота
void freeSlotAtomic(uint8_t slot) {
    if(slot >= QUEUE_SIZE) return;

    uint32_t primask_bit = __get_PRIMASK();
    __disable_irq();
    messagePoolUsed[slot] = 0;
    if (!primask_bit) {
        __enable_irq();
    }
}

// Атомарный захват слота для прерываний
int getFreeSlotAtomic(void) {
    for(int i = 0; i < QUEUE_SIZE; i++) {
        uint32_t primask_bit = __get_PRIMASK();
        __disable_irq();
        if(messagePoolUsed[i] == 0) {
            messagePoolUsed[i] = 1;
            if (!primask_bit) __enable_irq();
            return i;
        }
        if (!primask_bit) __enable_irq();
    }
    return -1;
}

// Инициализация логгера (без изменений, но добавляем инициализацию нового поля)
void Logger_Init(UART_HandleTypeDef* huart) {
    logger.huart = huart;
    logger.isTransmitting = 0;
    logger.txBuffer = txBuffer;
    logger.started = 1;
    logger.currentMsgIndex = 0xFF;  // ← ДОБАВЛЕНО: невалидный индекс

    LogBuffer_Init();

    memset(messagePoolUsed, 0, sizeof(messagePoolUsed));

    osMutexDef(poolMutex);
    poolMutexHandle = osMutexCreate(osMutex(poolMutex));

    osMessageQDef(logQueue, QUEUE_SIZE, uint32_t);
    logger.messageQueue = osMessageCreate(osMessageQ(logQueue), NULL);
}

// ✅ ИСПРАВЛЕННАЯ функция обработки сообщений
void Logger_Process(void) {
    if (!logger.isTransmitting) {
        osEvent event = osMessageGet(logger.messageQueue, 0);

        if (event.status == osEventMessage) {
            uint32_t msgIndex = event.value.v;
            if(msgIndex < QUEUE_SIZE && messagePoolUsed[msgIndex] == 1) {  // ← ДОБАВЛЕНА проверка статуса
                LogMessage_t* msg = &messagePool[msgIndex];

                if(msg->length > 0 && msg->length < MAX_MESSAGE_SIZE) {
                    // Копируем сообщение в буфер отправки
                    memcpy(logger.txBuffer, msg->data, msg->length);

                    // ✅ ИСПРАВЛЕНО: НЕ освобождаем слот здесь!
                    // Сохраняем индекс для освобождения после завершения передачи
                    logger.currentMsgIndex = msgIndex;

                    // Начинаем передачу
                    logger.isTransmitting = 1;

                    HAL_StatusTypeDef status = HAL_ERROR;

                    // Проверяем состояние DMA перед отправкой
                    if(logger.huart->hdmatx && logger.huart->hdmatx->State == HAL_DMA_STATE_READY) {
                        status = HAL_UART_Transmit_DMA(logger.huart, (uint8_t*)logger.txBuffer, msg->length);
                    }

                    if(status != HAL_OK) {
                        // ✅ ИСПРАВЛЕНО: Если DMA не запустился - освобождаем слот и используем блокирующую передачу
                        HAL_UART_Transmit(logger.huart, (uint8_t*)logger.txBuffer, msg->length, 100);
                        freeSlotAtomic(msgIndex);  // Освобождаем слот
                        logger.isTransmitting = 0;
                        logger.currentMsgIndex = 0xFF;
                    }
                } else {
                    // ✅ ИСПРАВЛЕНО: Некорректный размер сообщения - освобождаем слот
                    freeSlotAtomic(msgIndex);
                }
            } else {
                // ✅ ДОБАВЛЕНО: Некорректный индекс или уже освобожденный слот - игнорируем
                // Это может произойти при race condition или двойной обработке
            }
        }
    }
}

// ✅ ИСПРАВЛЕННЫЙ callback завершения передачи
void Logger_TxCpltCallback(void) {
    // ✅ ИСПРАВЛЕНО: Освобождаем слот только после завершения передачи
    if(logger.currentMsgIndex < QUEUE_SIZE) {
        freeSlotAtomic(logger.currentMsgIndex);
        logger.currentMsgIndex = 0xFF;  // Сбрасываем в невалидное значение
    }
    logger.isTransmitting = 0;
}

// ✅ ИСПРАВЛЕННАЯ функция логирования
void Logger_Log(const char* format, ...) {
    if(!logger.started || !format) return;

    int slot = -1;

    // Безопасное получение слота с учетом контекста
    if(isInInterrupt()) {
        slot = getFreeSlotAtomic();
    } else {
        // В обычном коде используем мьютекс
        if(osMutexWait(poolMutexHandle, 10) == osOK) {  // ← ДОБАВЛЕН таймаут
            slot = getFreeMessageSlot();
            osMutexRelease(poolMutexHandle);
        }
    }

    if(slot < 0) return; // Нет свободных слотов

    LogMessage_t* msg = &messagePool[slot];
    va_list args;

    va_start(args, format);
    int length = vsnprintf(msg->data, MAX_MESSAGE_SIZE-3, format, args);
    va_end(args);

    if (length <= 0) {
        freeSlotAtomic(slot);
        return;
    }

    // Добавляем завершающие символы
    msg->data[length] = '\r';
    msg->data[length + 1] = '\x03';
    msg->data[length + 2] = '\x04';
    msg->length = length + 3;

    // Дублируем в RAM-кольцевой буфер до постановки в очередь — так копия
    // не теряется, даже если очередь/пул UART в этот момент переполнены.
    LogBuffer_Append(msg->data, msg->length);

    // Помещаем индекс сообщения в очередь
    osStatus status = osMessagePut(logger.messageQueue, slot, 10);
    if(status != osOK) {
        // ✅ ИСПРАВЛЕНО: При ошибке очереди освобождаем слот
        freeSlotAtomic(slot);
    }
}

// ✅ ИСПРАВЛЕННАЯ функция логирования без завершающих символов
void Logger_Log_xx(const char* format, ...) {
    if(!logger.started || !format) return;

    int slot = -1;

    if(isInInterrupt()) {
        slot = getFreeSlotAtomic();
    } else {
        if(osMutexWait(poolMutexHandle, 10) == osOK) {
            slot = getFreeMessageSlot();
            osMutexRelease(poolMutexHandle);
        }
    }

    if(slot < 0) return;

    LogMessage_t* msg = &messagePool[slot];
    va_list args;

    va_start(args, format);
    int length = vsnprintf(msg->data, MAX_MESSAGE_SIZE, format, args);
    va_end(args);

    if (length <= 0) {
        freeSlotAtomic(slot);
        return;
    }

    msg->length = length;

    // Дублируем в RAM-кольцевой буфер до постановки в очередь — см. Logger_Log.
    LogBuffer_Append(msg->data, msg->length);

    osStatus status = osMessagePut(logger.messageQueue, slot, 10);
    if(status != osOK) {
        freeSlotAtomic(slot);
    }
}

// ✅ ДОБАВЛЕНА функция для безопасной остановки логгера
void Logger_Stop(void) {
    logger.started = 0;

    // Ждем завершения текущей передачи
    while(logger.isTransmitting) {
        osDelay(1);
    }

    // Очищаем очередь
    osEvent evt;
    do {
        evt = osMessageGet(logger.messageQueue, 0);
        if(evt.status == osEventMessage && evt.value.v < QUEUE_SIZE) {
            freeSlotAtomic((uint8_t)evt.value.v);
        }
    } while(evt.status == osEventMessage);
}

// ✅ ДОБАВЛЕНА функция получения статистики
void Logger_GetStats(LoggerStats_t* stats) {
    if(!stats) return;

    stats->totalSlots = QUEUE_SIZE;
    stats->usedSlots = 0;
    stats->isTransmitting = logger.isTransmitting;

    // Подсчитываем занятые слоты
    uint32_t primask_bit = __get_PRIMASK();
    __disable_irq();
    for(int i = 0; i < QUEUE_SIZE; i++) {
        if(messagePoolUsed[i]) {
            stats->usedSlots++;
        }
    }
    if (!primask_bit) {
        __enable_irq();
    }

    stats->freeSlots = stats->totalSlots - stats->usedSlots;

    // Примерная длина очереди (может быть неточной из-за race conditions)
    stats->queueLength = stats->usedSlots;
}
