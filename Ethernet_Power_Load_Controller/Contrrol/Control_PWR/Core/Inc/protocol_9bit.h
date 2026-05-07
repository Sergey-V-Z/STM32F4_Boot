/**
 * @file protocol_9bit.h
 * @brief Поддержка устаревшего 9-битного UART протокола для chanels_pwr (MOSFET_3CH)
 *
 * chanels_pwr (STM32F030) использует UART с address-mark wakeup (9-bit UART):
 * - Мастер шлёт address-байт с битом TB8=1 (9-й бит = 1) — все слэйвы слышат.
 * - Слэйв с нужным адресом выходит из mute-mode и принимает следующие данные (8 бит).
 * - Мастер отправляет 16 байт данных (TB8=0) после адреса.
 * - Слэйв отвечает 24 байтами без заголовка.
 *
 * Формат TX мастера → слэйв:
 *   [addr_9bit] [PWM1_lo][PWM1_hi][PWM1_b2][PWM1_b3]
 *               [en1]
 *               [PWM2_lo][PWM2_hi][PWM2_b2][PWM2_b3]
 *               [en2]
 *               [PWM3_lo][PWM3_hi][PWM3_b2][PWM3_b3]
 *               [en3]
 *   Итого: 16 байт данных (без адреса).
 *
 * Формат RX слэйв → мастер (24 байта):
 *   [PWM1_lo..b3][en1]  [ADC_ch1_lo][ADC_ch1_hi]
 *   [PWM2_lo..b3][en2]  [ADC_ch2_lo][ADC_ch2_hi]
 *   [PWM3_lo..b3][en3]
 *   [addr][type=30]
 *   (без дополнительного padding)
 *
 * Совместимость: не меняет прошивку chanels_pwr.
 */

#ifndef INC_PROTOCOL_9BIT_H_
#define INC_PROTOCOL_9BIT_H_

#include "main.h"
#include "stm32f4xx_hal.h"

/* Размеры пакетов chanels_pwr */
#define CHANELS_PWR_TX_SIZE    16U   /* байт данных от мастера к слэйву */
#define CHANELS_PWR_RX_SIZE    24U   /* байт ответа слэйва */

/* Тип платы в ответе chanels_pwr */
#define CHANELS_PWR_TYPE_ID    30U

/* Таймаут ожидания ответа от chanels_pwr в мс */
#define CHANELS_PWR_TIMEOUT_MS 50U

/**
 * @brief Структура команды мастера для chanels_pwr (16 байт TX)
 */
typedef struct __attribute__((packed)) {
    uint32_t PWM1;   /* ШИМ канала 1 */
    uint8_t  en1;    /* Включение канала 1 */
    uint32_t PWM2;   /* ШИМ канала 2 */
    uint8_t  en2;    /* Включение канала 2 */
    uint32_t PWM3;   /* ШИМ канала 3 */
    uint8_t  en3;    /* Включение канала 3 */
} chanels_pwr_cmd_t;  /* 3*5 = 15 байт + 1 выравнивания — но packed, итого 15 */

/**
 * @brief Структура ответа chanels_pwr (24 байта RX)
 *
 * Из анализа main.c chanels_pwr (aTxBuffer[24]):
 *  [0..3]  = CCR4 (PWM1)
 *  [4..5]  = зарезервировано (aTxBuffer[4]=0, [5]=0)
 *  [6]     = en1
 *  [7..10] = CCR2 (PWM2)
 *  [11..12]= ADC_ch1 (adc_buffer[1])
 *  [13]    = en2
 *  [14..17]= CCR1 (PWM3)
 *  [18..19]= ADC_ch2 (adc_buffer[0])
 *  [20]    = en3
 *  [21]    = OwnAddr
 *  [22]    = TypePCB (30 = MOSFET_3CH)
 *  [23]    = 0
 */
typedef struct __attribute__((packed)) {
    uint32_t PWM1;        /* CCR4 [0..3] */
    uint8_t  reserved1;  /* [4] */
    uint8_t  reserved2;  /* [5] */
    uint8_t  en1;         /* [6] */
    uint32_t PWM2;        /* [7..10] */
    uint16_t ADC_ch1;     /* [11..12] */
    uint8_t  en2;         /* [13] */
    uint32_t PWM3;        /* [14..17] */
    uint16_t ADC_ch2;     /* [18..19] */
    uint8_t  en3;         /* [20] */
    uint8_t  addr;        /* [21] = OwnAddr */
    uint8_t  type;        /* [22] = 30 */
    uint8_t  pad;         /* [23] = 0 */
} chanels_pwr_resp_t;  /* итого 24 байта */

/* -------------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------------- */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Переключить UART1 в 9-битный режим (для chanels_pwr).
 *        Вызывать перед обменом с MOSFET_3CH устройствами.
 *        Предварительно остановить DMA-приём через HAL_UART_AbortReceive().
 */
HAL_StatusTypeDef uart1_switch_to_9bit(void);

/**
 * @brief Переключить UART1 обратно в 8-битный режим (для нового протокола).
 *        Вызывать после завершения обмена с MOSFET_3CH устройствами.
 *        После вызова перезапустить DMA-приём (HAL_UARTEx_ReceiveToIdle_DMA).
 */
HAL_StatusTypeDef uart1_switch_to_8bit(void);

/**
 * @brief Отправить команду и получить ответ от chanels_pwr (MOSFET_3CH).
 *
 * @param huart   UART1 handle
 * @param addr    Адрес устройства (1..15)
 * @param cmd     Указатель на структуру команды (16 байт)
 * @param resp    Указатель для записи ответа (24 байта)
 * @return HAL_OK при успехе, HAL_TIMEOUT или HAL_ERROR при неудаче
 */
HAL_StatusTypeDef chanels_pwr_exchange(UART_HandleTypeDef *huart, uint8_t addr,
                                       const chanels_pwr_cmd_t *cmd,
                                       chanels_pwr_resp_t *resp);

/**
 * @brief Поиск chanels_pwr (MOSFET_3CH) устройств в диапазоне адресов.
 *        Переключает UART1 в 9-bit, сканирует, возвращает обратно в 8-bit.
 *        Заполняет найденные слоты в массиве dev[] начиная с offset.
 *
 * @param dev        Массив устройств
 * @param start_idx  Начальный индекс в массиве (после уже найденных 8-bit устройств)
 * @param max_dev    Размер массива dev
 * @return Количество найденных MOSFET_3CH устройств
 */
uint32_t auto_search_9bit(DEV_t *dev, uint8_t start_idx, uint8_t max_dev);

/**
 * @brief Обменяться данными с одним chanels_pwr устройством.
 *        Переключает UART1 в 9-bit, обменивается, возвращает в 8-bit.
 *        Обновляет поля ch[] в структуре dev.
 *
 * @param dev  Указатель на запись устройства
 * @return 0 при успехе, 1 при ошибке
 */
uint8_t exchange_9bit_device(DEV_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* INC_PROTOCOL_9BIT_H_ */
