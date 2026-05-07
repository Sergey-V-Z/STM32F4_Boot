/**
 * @file protocol_9bit.c
 * @brief Поддержка 9-битного UART протокола для chanels_pwr (MOSFET_3CH, STM32F030)
 *
 * chanels_pwr использует HAL_MultiProcessor_Init + UART_WAKEUPMETHOD_ADDRESSMARK.
 * Мастер (Control_PWR) переключает UART1 в 9-бит, шлёт адресный байт, затем 16 байт
 * данных с TB8=0. Слэйв отвечает сразу 24 байтами.
 *
 * Важно: UART1 обычно работает в 8-бит + DMA (HAL_UARTEx_ReceiveToIdle_DMA).
 * Перед переключением в 9-bit мы останавливаем DMA-приём, после — восстанавливаем.
 * Всё это происходит под deviceMutexHandle.
 */

#include "protocol_9bit.h"
#include "protocol.h"
#include "cmsis_os.h"
#include <string.h>
#include <stdbool.h>

/* Внешние зависимости из freertos.cpp */
extern UART_HandleTypeDef huart1;
extern DMA_HandleTypeDef hdma_usart1_rx;
extern uint8_t UART_rx[];
extern osMutexId deviceMutexHandle;
extern osMessageQId rxDataUART1Handle;

/* Внутренний RX буфер для 9-bit ответа.
 * В 9-bit режиме STM32F4 HAL записывает по 2 байта (uint16_t) на каждый
 * принятый фрейм, поэтому буфер должен быть uint16_t[N], а не uint8_t[N]. */
static uint16_t s_9bit_rx_buf[CHANELS_PWR_RX_SIZE];

/* -------------------------------------------------------------------------
 * Вспомогательные функции
 * ------------------------------------------------------------------------- */

/**
 * @brief Отправить один байт с установленным битом TB8 (адресный маркер).
 *        STM32F4 USART: бит TB8 задаётся записью 0x100 | addr в DR.
 *        Функция работает в текущем режиме UART (9-бит или 8-бит).
 */
static HAL_StatusTypeDef uart1_send_address_byte(uint8_t addr)
{
    uint32_t timeout = 5U;  /* мс */
    uint32_t tickstart = HAL_GetTick();

    /* Ждём TXE */
    while (!(huart1.Instance->SR & USART_SR_TXE)) {
        if ((HAL_GetTick() - tickstart) > timeout) {
            return HAL_TIMEOUT;
        }
    }
    /* Записать 9-й бит = 1 (адресный маркер) + адрес (низкие 8 бит) */
    huart1.Instance->DR = (uint16_t)(0x100U | (addr & 0xFFU));

    /* Ждём TC (transmission complete) */
    tickstart = HAL_GetTick();
    while (!(huart1.Instance->SR & USART_SR_TC)) {
        if ((HAL_GetTick() - tickstart) > timeout) {
            return HAL_TIMEOUT;
        }
    }
    return HAL_OK;
}

/* -------------------------------------------------------------------------
 * Публичные функции
 * ------------------------------------------------------------------------- */

HAL_StatusTypeDef uart1_switch_to_9bit(void)
{
    /* Остановить текущий DMA-приём */
    HAL_UART_AbortReceive(&huart1);

    /* Деинициализировать */
    HAL_UART_DeInit(&huart1);

    /* Настроить как MultiProcessor 9-bit (как в chanels_pwr) */
    huart1.Instance                    = USART1;
    huart1.Init.BaudRate               = 115200;
    huart1.Init.WordLength             = UART_WORDLENGTH_9B;
    huart1.Init.StopBits               = UART_STOPBITS_1;
    huart1.Init.Parity                 = UART_PARITY_NONE;
    huart1.Init.Mode                   = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl              = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling           = UART_OVERSAMPLING_16;

    return HAL_UART_Init(&huart1);
}

HAL_StatusTypeDef uart1_switch_to_8bit(void)
{
    /* Деинициализировать */
    HAL_UART_DeInit(&huart1);

    /* Восстановить оригинальную конфигурацию 8-bit */
    huart1.Instance                    = USART1;
    huart1.Init.BaudRate               = 115200;
    huart1.Init.WordLength             = UART_WORDLENGTH_8B;
    huart1.Init.StopBits               = UART_STOPBITS_1;
    huart1.Init.Parity                 = UART_PARITY_NONE;
    huart1.Init.Mode                   = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl              = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling           = UART_OVERSAMPLING_16;

    HAL_StatusTypeDef ret = HAL_UART_Init(&huart1);
    if (ret != HAL_OK) {
        return ret;
    }

    /* Перезапустить DMA-приём (как в uart_task при старте) */
    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, UART_rx, UART_RX_LENGTH);
    __HAL_DMA_DISABLE_IT(&hdma_usart1_rx, DMA_IT_HT);

    return HAL_OK;
}

HAL_StatusTypeDef chanels_pwr_exchange(UART_HandleTypeDef *huart, uint8_t addr,
                                       const chanels_pwr_cmd_t *cmd,
                                       chanels_pwr_resp_t *resp)
{
    if (huart == NULL || cmd == NULL || resp == NULL || addr == 0) {
        return HAL_ERROR;
    }

    /* Сборка TX пакета (16 байт) согласно формату aRxBuffer chanels_pwr:
     * [0]    = addr (9-bit address marker, TB8=1 — шлём отдельно)
     * [1..4] = PWM1 (little-endian)
     * [5]    = en1
     * [6..9] = PWM2
     * [10]   = en2
     * [11..14]= PWM3
     * [15]   = en3
     *
     * В chanels_pwr: aRxBuffer[0] = адрес (9-bit маркер),
     *                aRxBuffer[1..4] = PWM1, [5]=en1, [6..9]=PWM2, [10]=en2,
     *                [11..14]=PWM3, [15]=en3
     *
     * В 9-bit режиме HAL читает по 2 байта (uint16_t) на элемент.
     * Поэтому tx_buf должен быть uint16_t[15], где каждый элемент — один байт
     * данных (значение 0-255). chanels_pwr ожидает addr + 15 фреймов = 16 итого.
     */
    uint16_t tx_buf[15];
    tx_buf[0]  = (uint8_t)(cmd->PWM1 & 0xFF);
    tx_buf[1]  = (uint8_t)((cmd->PWM1 >> 8)  & 0xFF);
    tx_buf[2]  = (uint8_t)((cmd->PWM1 >> 16) & 0xFF);
    tx_buf[3]  = (uint8_t)((cmd->PWM1 >> 24) & 0xFF);
    tx_buf[4]  = cmd->en1;
    tx_buf[5]  = (uint8_t)(cmd->PWM2 & 0xFF);
    tx_buf[6]  = (uint8_t)((cmd->PWM2 >> 8)  & 0xFF);
    tx_buf[7]  = (uint8_t)((cmd->PWM2 >> 16) & 0xFF);
    tx_buf[8]  = (uint8_t)((cmd->PWM2 >> 24) & 0xFF);
    tx_buf[9]  = cmd->en2;
    tx_buf[10] = (uint8_t)(cmd->PWM3 & 0xFF);
    tx_buf[11] = (uint8_t)((cmd->PWM3 >> 8)  & 0xFF);
    tx_buf[12] = (uint8_t)((cmd->PWM3 >> 16) & 0xFF);
    tx_buf[13] = (uint8_t)((cmd->PWM3 >> 24) & 0xFF);
    tx_buf[14] = cmd->en3;

    /* Сначала шлём адресный байт с TB8=1 */
    HAL_StatusTypeDef ret = uart1_send_address_byte(addr);
    if (ret != HAL_OK) {
        return ret;
    }

    /* Затем 15 байт данных (9-bit фреймов, TB8=0): chanels_pwr ждёт 16 фреймов
     * всего (addr + 15 data), что соответствует его приёму 16 фреймов.
     * HAL_UART_Transmit в 9-bit режиме требует выравнивания буфера.
     * tx_buf уже uint16_t[15], поэтому передаём 15 элементов. */
    ret = HAL_UART_Transmit(huart, (uint8_t*)tx_buf, 15, 10);
    if (ret != HAL_OK) {
        return ret;
    }

    /* Ожидаем 24-байтный ответ от слэйва через блокирующий полинг.
     * HAL_UARTEx_ReceiveToIdle в режиме 9-bit использует блокирующий опрос RXNE.
     * s_9bit_rx_buf должен быть uint16_t[] для корректного разбора 9-бит данных. */
    uint16_t rx_len = 0;
    ret = HAL_UARTEx_ReceiveToIdle(huart, (uint8_t*)s_9bit_rx_buf, CHANELS_PWR_RX_SIZE,
                                   &rx_len, CHANELS_PWR_TIMEOUT_MS);
    if (ret != HAL_OK) {
        return ret;
    }

    if (rx_len < CHANELS_PWR_RX_SIZE) {
        return HAL_ERROR;
    }

    /* Разбираем ответ согласно структуре aTxBuffer chanels_pwr */
    resp->PWM1     = (uint32_t)s_9bit_rx_buf[0]  |
                     ((uint32_t)s_9bit_rx_buf[1]  << 8)  |
                     ((uint32_t)s_9bit_rx_buf[2]  << 16) |
                     ((uint32_t)s_9bit_rx_buf[3]  << 24);
    resp->reserved1 = s_9bit_rx_buf[4];
    resp->reserved2 = s_9bit_rx_buf[5];
    resp->en1       = s_9bit_rx_buf[6];
    resp->PWM2     = (uint32_t)s_9bit_rx_buf[7]  |
                     ((uint32_t)s_9bit_rx_buf[8]  << 8)  |
                     ((uint32_t)s_9bit_rx_buf[9]  << 16) |
                     ((uint32_t)s_9bit_rx_buf[10] << 24);
    resp->ADC_ch1   = (uint16_t)s_9bit_rx_buf[11] |
                      ((uint16_t)s_9bit_rx_buf[12] << 8);
    resp->en2       = s_9bit_rx_buf[13];
    resp->PWM3     = (uint32_t)s_9bit_rx_buf[14] |
                     ((uint32_t)s_9bit_rx_buf[15] << 8)  |
                     ((uint32_t)s_9bit_rx_buf[16] << 16) |
                     ((uint32_t)s_9bit_rx_buf[17] << 24);
    resp->ADC_ch2   = (uint16_t)s_9bit_rx_buf[18] |
                      ((uint16_t)s_9bit_rx_buf[19] << 8);
    resp->en3       = s_9bit_rx_buf[20];
    resp->addr      = s_9bit_rx_buf[21];
    resp->type      = s_9bit_rx_buf[22];
    resp->pad       = s_9bit_rx_buf[23];

    return HAL_OK;
}

uint32_t auto_search_9bit(DEV_t *dev, uint8_t start_idx, uint8_t max_dev)
{
    if (dev == NULL || start_idx >= max_dev) {
        return 0;
    }

    uint32_t found_count = 0;

    if (uart1_switch_to_9bit() != HAL_OK) {
        STM_LOG(LOG_ERR "auto_search_9bit: failed to switch to 9-bit mode");
        uart1_switch_to_8bit();
        return 0;
    }

    /* Нулевая команда: все en=0, PWM=0 */
    chanels_pwr_cmd_t cmd = {0};
    chanels_pwr_resp_t resp;

    for (uint8_t addr = START_ADR_DEV; addr < START_ADR_DEV + (max_dev - start_idx); addr++) {
        /* Проверяем, не занят ли уже этот адрес (8-bit устройством) */
        int already_found = 0;
        for (uint8_t k = 0; k < start_idx; k++) {
            if (dev[k].Addr == addr) {
                already_found = 1;
                break;
            }
        }
        if (already_found) {
            continue;
        }

        memset(&resp, 0, sizeof(resp));

        HAL_StatusTypeDef ret = chanels_pwr_exchange(&huart1, addr, &cmd, &resp);

        STM_LOG("9bit: addr=%d, ret=%d, resp.type=%d, resp.addr=%d", addr, ret, resp.type, resp.addr);

        if (ret == HAL_OK && resp.type == CHANELS_PWR_TYPE_ID && resp.addr == addr) {
            /* Нашли chanels_pwr устройство */
            uint8_t slot = start_idx + found_count;
            if (slot >= max_dev) {
                break;
            }

            dev[slot].Addr       = addr;
            dev[slot].AddrFromDev = resp.addr;
            dev[slot].TypePCB    = MOSFET_3CH;
            dev[slot].ch[0].used = true;
            dev[slot].ch[1].used = true;
            dev[slot].ch[2].used = true;
            dev[slot].ch[3].used = false;
            dev[slot].ch[4].used = false;
            dev[slot].ch[5].used = false;

            STM_LOG("Found MOSFET_3CH at addr %d", addr);
            found_count++;
        }
    }

    uart1_switch_to_8bit();

    return found_count;
}

uint8_t exchange_9bit_device(DEV_t *dev)
{
    if (dev == NULL || dev->Addr == 0 || dev->TypePCB != MOSFET_3CH) {
        return 1;
    }

    /* Формируем команду из текущих данных устройства */
    chanels_pwr_cmd_t cmd;
    cmd.PWM1 = dev->ch[0].PWM;
    cmd.en1  = dev->ch[0].On_off;
    cmd.PWM2 = dev->ch[1].PWM;
    cmd.en2  = dev->ch[1].On_off;
    cmd.PWM3 = dev->ch[2].PWM;
    cmd.en3  = dev->ch[2].On_off;

    if (uart1_switch_to_9bit() != HAL_OK) {
        STM_LOG(LOG_ERR "exchange_9bit_device: failed to switch to 9-bit");
        uart1_switch_to_8bit();
        return 1;
    }

    chanels_pwr_resp_t resp;
    memset(&resp, 0, sizeof(resp));

    HAL_StatusTypeDef ret = chanels_pwr_exchange(&huart1, dev->Addr, &cmd, &resp);

    uart1_switch_to_8bit();

    if (ret != HAL_OK) {
        dev->ERR_counter++;
        dev->last_ERR = ret;
        return 1;
    }

    /* Обновляем обратные данные (CCR из ответа) */
    dev->ch[0].PWM_out = resp.PWM1;
    dev->ch[0].IsOn    = resp.en1;
    dev->ch[1].PWM_out = resp.PWM2;
    dev->ch[1].IsOn    = resp.en2;
    dev->ch[2].PWM_out = resp.PWM3;
    dev->ch[2].IsOn    = resp.en3;

    /* ADC данные (ток/напряжение) */
    dev->ch[0].Current = (uint16_t)resp.ADC_ch1;
    dev->ch[1].Current = (uint16_t)resp.ADC_ch2;

    dev->ERR_counter = 0;

    return 0;
}
