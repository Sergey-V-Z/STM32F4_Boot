#include "protocol.h"

extern uint8_t message_rx[message_RX_LENGTH];
// формат пакета [addres][cmd][size][data...][crc_lo][crc_hi]

// приватные функции
void serialize_dev_to_buff(uint8_t *buff, uint16_t *size, const DEV_t *dev);
void deserialize_buff_to_dev(uint8_t *buff, uint16_t size, DEV_t *dev);
HAL_StatusTypeDef send_pwm_ch_to_dev(const DEV_t *dev);

// Быстрая с таблицей
uint16_t crc16_fast(const uint8_t *data, uint16_t len) {
    uint16_t crc = 0xFFFF;
    while (len--) {
        crc = (crc << 8) ^ crc16_table[((crc >> 8) ^ *data++) & 0xFF];
    }
    return crc;
}

void deserialize_ch_data(uint8_t *buff, uint16_t size, pwm_ch_t *chanels)
{
  // Проверка на корректность входных параметров
  if (buff == NULL || chanels == NULL || size < 30)
  {
    return; // Минимальный размер: 6 байт (en1-en6) + 24 байта (PWM1-PWM6)
  }

  uint16_t offset = 0;

  // Десериализация флагов enable (6 байт)
  chanels->en1 = buff[offset++];
  chanels->en2 = buff[offset++];
  chanels->en3 = buff[offset++];
  chanels->en4 = buff[offset++];
  chanels->en5 = buff[offset++];
  chanels->en6 = buff[offset++];

  // Десериализация PWM значений (24 байта)
  // Предполагается little-endian порядок байтов
  chanels->PWM1 = (uint32_t)buff[offset] |
                  ((uint32_t)buff[offset + 1] << 8) |
                  ((uint32_t)buff[offset + 2] << 16) |
                  ((uint32_t)buff[offset + 3] << 24);
  offset += 4;

  chanels->PWM2 = (uint32_t)buff[offset] |
                  ((uint32_t)buff[offset + 1] << 8) |
                  ((uint32_t)buff[offset + 2] << 16) |
                  ((uint32_t)buff[offset + 3] << 24);
  offset += 4;

  chanels->PWM3 = (uint32_t)buff[offset] |
                  ((uint32_t)buff[offset + 1] << 8) |
                  ((uint32_t)buff[offset + 2] << 16) |
                  ((uint32_t)buff[offset + 3] << 24);
  offset += 4;

  chanels->PWM4 = (uint32_t)buff[offset] |
                  ((uint32_t)buff[offset + 1] << 8) |
                  ((uint32_t)buff[offset + 2] << 16) |
                  ((uint32_t)buff[offset + 3] << 24);
  offset += 4;

  chanels->PWM5 = (uint32_t)buff[offset] |
                  ((uint32_t)buff[offset + 1] << 8) |
                  ((uint32_t)buff[offset + 2] << 16) |
                  ((uint32_t)buff[offset + 3] << 24);
  offset += 4;

  chanels->PWM6 = (uint32_t)buff[offset] |
                  ((uint32_t)buff[offset + 1] << 8) |
                  ((uint32_t)buff[offset + 2] << 16) |
                  ((uint32_t)buff[offset + 3] << 24);
}

void serialize_ch_data(uint8_t *buff, uint16_t *size, pwm_ch_t *chanels)
{
  // Проверка на корректность входных параметров
  if (buff == NULL || size == NULL || chanels == NULL)
  {
    if (size != NULL)
      *size = 0;
    return;
  }

  uint16_t offset = 0;

  // Сериализация флагов enable (6 байт)
  buff[offset++] = chanels->en1;
  buff[offset++] = chanels->en2;
  buff[offset++] = chanels->en3;
  buff[offset++] = chanels->en4;
  buff[offset++] = chanels->en5;
  buff[offset++] = chanels->en6;

  // Сериализация PWM значений (24 байта)
  // Little-endian порядок байтов
  buff[offset++] = (uint8_t)(chanels->PWM1 & 0xFF);
  buff[offset++] = (uint8_t)((chanels->PWM1 >> 8) & 0xFF);
  buff[offset++] = (uint8_t)((chanels->PWM1 >> 16) & 0xFF);
  buff[offset++] = (uint8_t)((chanels->PWM1 >> 24) & 0xFF);

  buff[offset++] = (uint8_t)(chanels->PWM2 & 0xFF);
  buff[offset++] = (uint8_t)((chanels->PWM2 >> 8) & 0xFF);
  buff[offset++] = (uint8_t)((chanels->PWM2 >> 16) & 0xFF);
  buff[offset++] = (uint8_t)((chanels->PWM2 >> 24) & 0xFF);

  buff[offset++] = (uint8_t)(chanels->PWM3 & 0xFF);
  buff[offset++] = (uint8_t)((chanels->PWM3 >> 8) & 0xFF);
  buff[offset++] = (uint8_t)((chanels->PWM3 >> 16) & 0xFF);
  buff[offset++] = (uint8_t)((chanels->PWM3 >> 24) & 0xFF);

  buff[offset++] = (uint8_t)(chanels->PWM4 & 0xFF);
  buff[offset++] = (uint8_t)((chanels->PWM4 >> 8) & 0xFF);
  buff[offset++] = (uint8_t)((chanels->PWM4 >> 16) & 0xFF);
  buff[offset++] = (uint8_t)((chanels->PWM4 >> 24) & 0xFF);

  buff[offset++] = (uint8_t)(chanels->PWM5 & 0xFF);
  buff[offset++] = (uint8_t)((chanels->PWM5 >> 8) & 0xFF);
  buff[offset++] = (uint8_t)((chanels->PWM5 >> 16) & 0xFF);
  buff[offset++] = (uint8_t)((chanels->PWM5 >> 24) & 0xFF);

  buff[offset++] = (uint8_t)(chanels->PWM6 & 0xFF);
  buff[offset++] = (uint8_t)((chanels->PWM6 >> 8) & 0xFF);
  buff[offset++] = (uint8_t)((chanels->PWM6 >> 16) & 0xFF);
  buff[offset++] = (uint8_t)((chanels->PWM6 >> 24) & 0xFF);

  *size = offset; // Возвращаем размер сериализованных данных (30 байт)
}

void serialize_ret_pwm_data(uint8_t *buff, uint16_t *size, ret_pwm_ch_t *data)
{
  // Проверка на корректность входных параметров
  if (buff == NULL || size == NULL || data == NULL)
  {
    if (size != NULL)
      *size = 0;
    return;
  }

  uint16_t offset = 0;

  // Сериализация флагов enable (6 байт)
  buff[offset++] = data->en1;
  buff[offset++] = data->en2;
  buff[offset++] = data->en3;
  buff[offset++] = data->en4;
  buff[offset++] = data->en5;
  buff[offset++] = data->en6;

  // Сериализация ADC значений (14 байт - 7 значений uint16_t)
  // Little-endian порядок байтов
  buff[offset++] = (uint8_t)(data->ADC_CH1 & 0xFF);
  buff[offset++] = (uint8_t)((data->ADC_CH1 >> 8) & 0xFF);

  buff[offset++] = (uint8_t)(data->ADC_CH2 & 0xFF);
  buff[offset++] = (uint8_t)((data->ADC_CH2 >> 8) & 0xFF);

  buff[offset++] = (uint8_t)(data->ADC_CH3 & 0xFF);
  buff[offset++] = (uint8_t)((data->ADC_CH3 >> 8) & 0xFF);

  buff[offset++] = (uint8_t)(data->ADC_CH4 & 0xFF);
  buff[offset++] = (uint8_t)((data->ADC_CH4 >> 8) & 0xFF);

  buff[offset++] = (uint8_t)(data->ADC_CH5 & 0xFF);
  buff[offset++] = (uint8_t)((data->ADC_CH5 >> 8) & 0xFF);

  buff[offset++] = (uint8_t)(data->ADC_CH6 & 0xFF);
  buff[offset++] = (uint8_t)((data->ADC_CH6 >> 8) & 0xFF);

  buff[offset++] = (uint8_t)(data->ADC_Termo & 0xFF);
  buff[offset++] = (uint8_t)((data->ADC_Termo >> 8) & 0xFF);

  // Сериализация PWM значений (24 байта)
  // Little-endian порядок байтов
  buff[offset++] = (uint8_t)(data->PWM1 & 0xFF);
  buff[offset++] = (uint8_t)((data->PWM1 >> 8) & 0xFF);
  buff[offset++] = (uint8_t)((data->PWM1 >> 16) & 0xFF);
  buff[offset++] = (uint8_t)((data->PWM1 >> 24) & 0xFF);

  buff[offset++] = (uint8_t)(data->PWM2 & 0xFF);
  buff[offset++] = (uint8_t)((data->PWM2 >> 8) & 0xFF);
  buff[offset++] = (uint8_t)((data->PWM2 >> 16) & 0xFF);
  buff[offset++] = (uint8_t)((data->PWM2 >> 24) & 0xFF);

  buff[offset++] = (uint8_t)(data->PWM3 & 0xFF);
  buff[offset++] = (uint8_t)((data->PWM3 >> 8) & 0xFF);
  buff[offset++] = (uint8_t)((data->PWM3 >> 16) & 0xFF);
  buff[offset++] = (uint8_t)((data->PWM3 >> 24) & 0xFF);

  buff[offset++] = (uint8_t)(data->PWM4 & 0xFF);
  buff[offset++] = (uint8_t)((data->PWM4 >> 8) & 0xFF);
  buff[offset++] = (uint8_t)((data->PWM4 >> 16) & 0xFF);
  buff[offset++] = (uint8_t)((data->PWM4 >> 24) & 0xFF);

  buff[offset++] = (uint8_t)(data->PWM5 & 0xFF);
  buff[offset++] = (uint8_t)((data->PWM5 >> 8) & 0xFF);
  buff[offset++] = (uint8_t)((data->PWM5 >> 16) & 0xFF);
  buff[offset++] = (uint8_t)((data->PWM5 >> 24) & 0xFF);

  buff[offset++] = (uint8_t)(data->PWM6 & 0xFF);
  buff[offset++] = (uint8_t)((data->PWM6 >> 8) & 0xFF);
  buff[offset++] = (uint8_t)((data->PWM6 >> 16) & 0xFF);
  buff[offset++] = (uint8_t)((data->PWM6 >> 24) & 0xFF);

  *size = offset; // Возвращаем размер сериализованных данных (44 байта)
}

// Функция десериализации
void deserialize_ret_pwm_data(uint8_t *buff, uint16_t size, ret_pwm_ch_t *data)
{
  // Проверка на корректность входных параметров
  if (buff == NULL || data == NULL || size < 44)
  {
    return; // Минимальный размер: 6 + 14 + 24 = 44 байта
  }

  uint16_t offset = 0;

  // Десериализация флагов enable (6 байт)
  data->en1 = buff[offset++];
  data->en2 = buff[offset++];
  data->en3 = buff[offset++];
  data->en4 = buff[offset++];
  data->en5 = buff[offset++];
  data->en6 = buff[offset++];

  // Десериализация ADC значений (14 байт)
  // Little-endian порядок байтов
  data->ADC_CH1 = (uint16_t)buff[offset] | ((uint16_t)buff[offset + 1] << 8);
  offset += 2;

  data->ADC_CH2 = (uint16_t)buff[offset] | ((uint16_t)buff[offset + 1] << 8);
  offset += 2;

  data->ADC_CH3 = (uint16_t)buff[offset] | ((uint16_t)buff[offset + 1] << 8);
  offset += 2;

  data->ADC_CH4 = (uint16_t)buff[offset] | ((uint16_t)buff[offset + 1] << 8);
  offset += 2;

  data->ADC_CH5 = (uint16_t)buff[offset] | ((uint16_t)buff[offset + 1] << 8);
  offset += 2;

  data->ADC_CH6 = (uint16_t)buff[offset] | ((uint16_t)buff[offset + 1] << 8);
  offset += 2;

  data->ADC_Termo = (uint16_t)buff[offset] | ((uint16_t)buff[offset + 1] << 8);
  offset += 2;

  // Десериализация PWM значений (24 байта)
  // Little-endian порядок байтов
  data->PWM1 = (uint32_t)buff[offset] |
               ((uint32_t)buff[offset + 1] << 8) |
               ((uint32_t)buff[offset + 2] << 16) |
               ((uint32_t)buff[offset + 3] << 24);
  offset += 4;

  data->PWM2 = (uint32_t)buff[offset] |
               ((uint32_t)buff[offset + 1] << 8) |
               ((uint32_t)buff[offset + 2] << 16) |
               ((uint32_t)buff[offset + 3] << 24);
  offset += 4;

  data->PWM3 = (uint32_t)buff[offset] |
               ((uint32_t)buff[offset + 1] << 8) |
               ((uint32_t)buff[offset + 2] << 16) |
               ((uint32_t)buff[offset + 3] << 24);
  offset += 4;

  data->PWM4 = (uint32_t)buff[offset] |
               ((uint32_t)buff[offset + 1] << 8) |
               ((uint32_t)buff[offset + 2] << 16) |
               ((uint32_t)buff[offset + 3] << 24);
  offset += 4;

  data->PWM5 = (uint32_t)buff[offset] |
               ((uint32_t)buff[offset + 1] << 8) |
               ((uint32_t)buff[offset + 2] << 16) |
               ((uint32_t)buff[offset + 3] << 24);
  offset += 4;

  data->PWM6 = (uint32_t)buff[offset] |
               ((uint32_t)buff[offset + 1] << 8) |
               ((uint32_t)buff[offset + 2] << 16) |
               ((uint32_t)buff[offset + 3] << 24);
}

void clear_ret_pwm_data(ret_pwm_ch_t *data)
{
  if (data == NULL)
  {
    return;
  }

  // Обнуление флагов enable
  data->en1 = 0;
  data->en2 = 0;
  data->en3 = 0;
  data->en4 = 0;
  data->en5 = 0;
  data->en6 = 0;

  // Обнуление ADC значений
  data->ADC_CH1 = 0;
  data->ADC_CH2 = 0;
  data->ADC_CH3 = 0;
  data->ADC_CH4 = 0;
  data->ADC_CH5 = 0;
  data->ADC_CH6 = 0;
  data->ADC_Termo = 0;

  // Обнуление PWM значений
  data->PWM1 = 0;
  data->PWM2 = 0;
  data->PWM3 = 0;
  data->PWM4 = 0;
  data->PWM5 = 0;
  data->PWM6 = 0;
}

void clear_pwm_data(pwm_ch_t *data)
{
  if (data == NULL)
  {
    return;
  }

  // Обнуление флагов enable
  data->en1 = 0;
  data->en2 = 0;
  data->en3 = 0;
  data->en4 = 0;
  data->en5 = 0;
  data->en6 = 0;

  // Обнуление PWM значений
  data->PWM1 = 0;
  data->PWM2 = 0;
  data->PWM3 = 0;
  data->PWM4 = 0;
  data->PWM5 = 0;
  data->PWM6 = 0;
}

// Отправка пакета по UART (блокирующая)
HAL_StatusTypeDef uart_send_packet(UART_HandleTypeDef *huart, uint8_t addr, uint8_t cmd, const uint8_t *data, uint16_t data_len)
{
    if (data_len + 5 > UART_TX_LENGTH) // addr + cmd + size + data + crc_lo + crc_hi
        return HAL_ERROR;

    uint16_t size = data_len + 5; // полный размер пакета

    UART_tx[0] = addr;
    UART_tx[1] = cmd;
    UART_tx[2] = size;
    if (data_len > 0 && data != NULL)
        memcpy(&UART_tx[3], data, data_len);

    // CRC по всему пакету кроме CRC
    uint16_t crc = crc16_fast(UART_tx, size - 2);
    UART_tx[size - 2] = crc & 0xFF;
    UART_tx[size - 1] = (crc >> 8) & 0xFF;

    /* DMA TX: аппаратура сама подаёт байты без участия CPU — никаких пробелов
     * между байтами даже при вытеснении задачи планировщиком.
     * Ждём TC через опрос gState (volatile поле HAL) с осовобождением CPU. */
    HAL_StatusTypeDef ret = HAL_UART_Transmit_DMA(huart, UART_tx, size);
    if (ret != HAL_OK)
        return ret;

    /* Ждём завершения TX по gState: HAL_UART_GetState() не подходит —
     * она ORит gState|RxState, и бит 0x20 есть и в READY и в BUSY_RX,
     * что даёт ложное срабатывание маски BUSY_TX при работающем RX DMA. */
    uint32_t tickstart = HAL_GetTick();
    while (huart->gState != HAL_UART_STATE_READY) {
        if (HAL_GetTick() - tickstart >= 100U)
            return HAL_TIMEOUT;
        osDelay(1);
    }
    return HAL_OK;
}

// Получение пакета из буфера (возвращает длину полезных данных, либо 0 если ошибка CRC/размера)
uint16_t uart_parse_packet(uint8_t *buf, uint16_t buf_len, Header_t *header, uint8_t **data, uint16_t *data_len)
{
  if (buf_len < 5) // минимум: адрес(1)+cmd(1)+size(1)+crc(2)
    return 0;

  uint8_t packet_addr = buf[0];
  uint8_t packet_cmd = buf[1];
  uint8_t data_size = buf[2];

  if (data_size > buf_len || data_size < 5)
    return 0;

  uint16_t crc_calc = crc16_fast(buf, data_size - 2);
  uint16_t crc_recv = buf[data_size - 2] | (buf[data_size - 1] << 8);

  if (crc_calc != crc_recv)
    return 0;

  if (header)
  {
    header->address = packet_addr;
    header->cmd = packet_cmd;
    header->size = data_size;
  }

  if (data && data_len)
  {
    *data_len = data_size - 5; // payload only: total - addr(1) - cmd(1) - size(1) - crc(2)
    *data = &buf[3];
  }
  return data_size;
}

// Функция отправки пакета данных ret_pwm_ch_t по UART
void send_ret_pwm_packet(UART_HandleTypeDef *huart, uint8_t addr, ret_pwm_ch_t *ret_data)
{
    if (ret_data == NULL) return;

    uint8_t buf[44];
    uint16_t data_size = 0;

    serialize_ret_pwm_data(buf, &data_size, ret_data);

    uart_send_packet(huart, addr, data, buf, data_size); // команда "data" должна быть определена как enum или define
}


// мастер *****************************************************************************************************************************
// принимает массив девайсов для заполнения и его размер
uint32_t auto_search_dev(DEV_t *dev, uint8_t size_dev)
{
  if (dev == nullptr || size_dev == 0) {
    STM_LOG(LOG_ERR "Invalid device array or size");
    return 0;
  }

  uint16_t data_len = 0;
  uint8_t *rx_data = nullptr;

  uint16_t found_count = 0;

  if (deviceMutexHandle == nullptr)
  {
    STM_LOG(LOG_ERR "Device mutex handle is null");
    return 0;
  }

  osStatus status = osMutexWait(deviceMutexHandle, 5000);
  if (status != osOK) {
    STM_LOG(LOG_ERR "Failed to acquire device mutex");
    return 0;
  }

  STM_LOG("Scanning %d addresses starting from %d", size_dev, (int)START_ADR_DEV);

  // сканируем
  for (uint8_t i = 0; i < size_dev; ++i) {
    uint8_t addr = START_ADR_DEV + i;

    STM_LOG("  [%d] Sending status request to addr %d", i, addr);

    // Сброс очереди перед отправкой — убираем остатки предыдущего ответа
    { osEvent stale; do { stale = osMessageGet(rxDataUART1Handle, 0); } while (stale.status == osEventMessage); }

    HAL_StatusTypeDef send_result = uart_send_packet(&huart1, addr, (uint8_t)cmd_t::status, nullptr, 0);
    if (send_result != HAL_OK) {
      STM_LOG(LOG_ERR "  [%d] uart_send_packet failed (HAL error %d)", i, (int)send_result);
      continue;
    }

    osEvent evt = osMessageGet(rxDataUART1Handle, 50);
    if (evt.status != osEventMessage) {
      STM_LOG("  [%d] No response from addr %d (evt.status=0x%02X)", i, addr, (unsigned)evt.status);
      continue;
    }

    uint16_t rx_size = (uint16_t)evt.value.v;
    STM_LOG("  [%d] Got %d bytes from addr %d", i, rx_size, addr);

    Header_t header;
    if (!uart_parse_packet(message_rx, rx_size, &header, &rx_data, &data_len)) {
      STM_LOG(LOG_ERR "  [%d] Parse failed: rx[0..4]=%02X %02X %02X %02X %02X",
              i, message_rx[0], message_rx[1], message_rx[2], message_rx[3], message_rx[4]);
      continue;
    }

    STM_LOG("  [%d] Parsed: addr=%d cmd=%d size=%d data_len=%d",
            i, header.address, header.cmd, header.size, data_len);

    if (header.address != addr) {
      STM_LOG(LOG_ERR "  [%d] Address mismatch: expected %d, got %d", i, addr, header.address);
      continue;
    }

    if (header.cmd != (uint8_t)cmd_t::status) {
      STM_LOG(LOG_ERR "  [%d] Unexpected cmd=%d (expected %d)", i, header.cmd, (uint8_t)cmd_t::status);
      continue;
    }

    if (rx_data == nullptr || data_len < sizeof(secondary_status_t))
    {
      STM_LOG(LOG_ERR "  [%d] Payload too small: data_len=%d, need=%d",
              i, data_len, (int)sizeof(secondary_status_t));
    	continue;
    }

    secondary_status_t sec_status;
    memcpy(&sec_status, rx_data, sizeof(secondary_status_t));

    STM_LOG("  [%d] Status: key=0x%08X type_pcb=%lu count_ch=%lu mode=0x%08X",
            i, (unsigned)sec_status.key, sec_status.type_pcb, sec_status.count_ch, (unsigned)sec_status.mode);

    if (sec_status.mode != MODE_APP) {
      STM_LOG(LOG_WARN "  [%d] Addr %d is in bootloader mode, skipping", i, addr);
      continue;
    }

    dev[found_count].Addr = addr;
    dev[found_count].AddrFromDev = header.address;
    dev[found_count].TypePCB = (PCBType)sec_status.type_pcb;
    dev[found_count].ERR_counter = 0;
    dev[found_count].last_ERR = 0;
    for (int ci = 0; ci < 6; ci++) {
        dev[found_count].ch[ci].PWM_out = 0;
        dev[found_count].ch[ci].PWM     = 0;
        dev[found_count].ch[ci].On_off  = 0;
        dev[found_count].ch[ci].IsOn    = 0;
        dev[found_count].ch[ci].Current = 0;
    }

    if (sec_status.count_ch == 6) {
      dev[found_count].ch[0].used = true;
      dev[found_count].ch[1].used = true;
      dev[found_count].ch[2].used = true;
      dev[found_count].ch[3].used = true;
      dev[found_count].ch[4].used = true;
      dev[found_count].ch[5].used = true;
    } else {
      dev[found_count].ch[0].used = true;
      dev[found_count].ch[1].used = true;
      dev[found_count].ch[2].used = true;
      dev[found_count].ch[3].used = false;
      dev[found_count].ch[4].used = false;
      dev[found_count].ch[5].used = false;
    }

    STM_LOG("  [%d] Device FOUND: addr=%d TypePCB=%d count_ch=%lu",
            i, addr, (int)dev[found_count].TypePCB, sec_status.count_ch);
    found_count++;
  }
  
  osMutexRelease(deviceMutexHandle);

  return found_count;
}

// функция сериализации данных 
void serialize_dev_to_buff(uint8_t *buff, uint16_t *size, const DEV_t *dev) {
    if (buff == nullptr || size == nullptr || dev == nullptr) {
        if (size) *size = 0;
        return;
    }

    pwm_ch_t pwm_data;
    clear_pwm_data(&pwm_data);

    // Заполняем только те поля, которые есть в pwm_ch_t (en1-en6, PWM1-PWM6)
    pwm_data.en1 = dev->ch[0].On_off;
    pwm_data.en2 = dev->ch[1].On_off;
    pwm_data.en3 = dev->ch[2].On_off;
    pwm_data.en4 = dev->ch[3].On_off;
    pwm_data.en5 = dev->ch[4].On_off;
    pwm_data.en6 = dev->ch[5].On_off;

    pwm_data.PWM1 = dev->ch[0].PWM_out;
    pwm_data.PWM2 = dev->ch[1].PWM_out;
    pwm_data.PWM3 = dev->ch[2].PWM_out;
    pwm_data.PWM4 = dev->ch[3].PWM_out;
    pwm_data.PWM5 = dev->ch[4].PWM_out;
    pwm_data.PWM6 = dev->ch[5].PWM_out;

    serialize_ch_data(buff, size, &pwm_data);
}

// функция десериализации данных из буфера в структуру DEV_t
void deserialize_buff_to_dev(uint8_t *buff, uint16_t size, DEV_t *dev) {
    if (buff == nullptr || dev == nullptr) {
        return; // Минимальный размер: 6 байт (en1-en6) + 24 байта (PWM1-PWM6)
    }

    ret_pwm_ch_t pwm_data;
    clear_ret_pwm_data(&pwm_data);
    // Десериализуем данные из буфера
    deserialize_ret_pwm_data((uint8_t*)buff, size, &pwm_data);

    // Заполняем только те поля, которые есть в DEV_t
    dev->ch[0].On_off = pwm_data.en1;
    dev->ch[1].On_off = pwm_data.en2;
    dev->ch[2].On_off = pwm_data.en3;
    dev->ch[3].On_off = pwm_data.en4;
    dev->ch[4].On_off = pwm_data.en5;
    dev->ch[5].On_off = pwm_data.en6;

    dev->ch[0].PWM = pwm_data.PWM1;
    dev->ch[1].PWM = pwm_data.PWM2;
    dev->ch[2].PWM = pwm_data.PWM3;
    dev->ch[3].PWM = pwm_data.PWM4;
    dev->ch[4].PWM = pwm_data.PWM5;
    dev->ch[5].PWM = pwm_data.PWM6;

    // добавить обработку ADC значений, в DEV_t это Current
    dev->ch[0].Current = pwm_data.ADC_CH1;
    dev->ch[1].Current = pwm_data.ADC_CH2;  
    dev->ch[2].Current = pwm_data.ADC_CH3;
    dev->ch[3].Current = pwm_data.ADC_CH4;
    dev->ch[4].Current = pwm_data.ADC_CH5;
    dev->ch[5].Current = pwm_data.ADC_CH6;

}

// функция отправки данный pwm_ch_t на устройство, заполнение для pwm_ch_t взять из DEV_t
// Функция отправки данных pwm_ch_t на устройство по адресу, заполнение из DEV_t
HAL_StatusTypeDef send_pwm_ch_to_dev(const DEV_t *dev)
{
  if (dev == nullptr) {
      STM_LOG(LOG_ERR "DEV_t pointer is null");
      return HAL_ERROR;
  }

  pwm_ch_t pwm_data;
  clear_pwm_data(&pwm_data);

  // Заполняем pwm_ch_t из DEV_t
  pwm_data.en1 = dev->ch[0].On_off;
  pwm_data.en2 = dev->ch[1].On_off;
  pwm_data.en3 = dev->ch[2].On_off;
  pwm_data.en4 = dev->ch[3].On_off;
  pwm_data.en5 = dev->ch[4].On_off;
  pwm_data.en6 = dev->ch[5].On_off;

  pwm_data.PWM1 = dev->ch[0].PWM_out;
  pwm_data.PWM2 = dev->ch[1].PWM_out;
  pwm_data.PWM3 = dev->ch[2].PWM_out;
  pwm_data.PWM4 = dev->ch[3].PWM_out;
  pwm_data.PWM5 = dev->ch[4].PWM_out;
  pwm_data.PWM6 = dev->ch[5].PWM_out;

  uint8_t buff[64] = {0};
  uint16_t size = 0;
  serialize_ch_data(buff, &size, &pwm_data);

  // Отправляем пакет на устройство
  return uart_send_packet(&huart1, dev->Addr, cmd_t::data, buff, size);
}

// обмен данными каналов с устройством
uint8_t exchange_channel_data_with_device(DEV_t *dev)
{
  if (dev == nullptr) {
    STM_LOG(LOG_ERR "DEV_t pointer is null");
    return 0;
  }

  uint16_t rx_data_size = 0;

  // мютекс для защиты доступа к устройствам
  if (deviceMutexHandle == nullptr)
  {
    STM_LOG(LOG_ERR "Device mutex handle is null");
    return 0;
  }
  
  osStatus ret = osMutexWait(deviceMutexHandle, 5000);
  if (ret != osOK)
  {
    STM_LOG(LOG_ERR "Failed to acquire device mutex");
    return 0;
  }

  // Сброс очереди перед отправкой — убираем остатки предыдущего обмена
  { osEvent stale; do { stale = osMessageGet(rxDataUART1Handle, 0); } while (stale.status == osEventMessage); }

  //отправить данные devices[var]
  if(send_pwm_ch_to_dev(dev) != HAL_OK)
  {
    osMutexRelease(deviceMutexHandle);
    return 0;
  }

  // ожидаем ответ
  osEvent evt = osMessageGet(rxDataUART1Handle, 100); // ждем ответ

  if (evt.status == osEventMessage){
    uint32_t size = evt.value.v;

    uint8_t *rx_data = nullptr;
    rx_data_size = 0;
    Header_t header;

    uart_parse_packet(message_rx, size, &header, &rx_data, &rx_data_size); // парсим пакет

    if (header.cmd != cmd_t::data)
    {
      osMutexRelease(deviceMutexHandle);
      return 0; // неверная команда
    }

    if(rx_data != nullptr){
      // разборка данных
      deserialize_buff_to_dev(rx_data, rx_data_size, dev);
    }else{
      osMutexRelease(deviceMutexHandle);
      return 0;
    }

  }else if (evt.status == osEventTimeout) {
    // В случае тайм-аута, просто выводим информационное сообщение и продолжаем
    STM_LOG(LOG_WARN "Timeout waiting for response");
    osMutexRelease(deviceMutexHandle);
    return 0;
  }

  osMutexRelease(deviceMutexHandle);
  return 1;
}

// прочитать метаданные
void GetSecondaryBoardMeta(DEV_t *dev, meta_t* metadata)
{
    if (dev == nullptr || metadata == nullptr) {
        return;
    }

    // Захватываем тот же мьютекс, что использует exchange_channel_data_with_device,
    // чтобы исключить гонку на очереди rxDataUART1Handle
    if (deviceMutexHandle == nullptr || osMutexWait(deviceMutexHandle, 1000) != osOK) {
        return;
    }

    // Отправляем команду на вторичную плату для получения метаданных
    cmd_t cmd = cmd_t::metadata_current;
    uart_send_packet(&huart1, dev->Addr, cmd, nullptr, 0);

    // Ожидаем ответ
    osEvent evt = osMessageGet(rxDataUART1Handle, 100); // ждем ответ

    osMutexRelease(deviceMutexHandle);

    if (evt.status == osEventMessage) {
        uint32_t size = evt.value.v;

        uint8_t *rx_data = nullptr;
        uint16_t rx_data_size = 0;
        Header_t header;

        uart_parse_packet(message_rx, size, &header, &rx_data, &rx_data_size); // парсим пакет

        if (header.cmd != cmd_t::metadata_current) {
            return; // неверная команда (ожидаем metadata_current)
        }

        if (rx_data != nullptr) {
            // разборка данных
            if (rx_data_size >= sizeof(meta_t)) {
                memcpy(metadata, rx_data, sizeof(meta_t));
            }
        }
    }
}

// получение статуса с ожиданием и таймаутом
HAL_StatusTypeDef GetSecondaryBoardStatus(DEV_t *dev, secondary_status_t* status, uint32_t timeout_ms)
{
    if (dev == nullptr || status == nullptr) {
        return HAL_ERROR;
    }

    // Отправляем команду на вторичную плату для получения статуса
    cmd_t cmd = cmd_t::status;
    uart_send_packet(&huart1, dev->Addr, cmd, nullptr, 0);

    // Ожидаем ответ
    osEvent evt = osMessageGet(rxDataUART1Handle, timeout_ms); // ждем ответ

    if (evt.status == osEventMessage) {
        uint32_t size = evt.value.v;

        uint8_t *rx_data = nullptr;
        uint16_t rx_data_size = 0;
        Header_t header;

        uart_parse_packet(message_rx, size, &header, &rx_data, &rx_data_size); // парсим пакет

        if (header.cmd != cmd) {
            return HAL_ERROR; // неверная команда
        }

        if (rx_data != nullptr) {
            // разборка данных
            if (rx_data_size >= sizeof(secondary_status_t)) {
                memcpy(status, rx_data, sizeof(secondary_status_t));
                return HAL_OK;
            } else {
                return HAL_ERROR; // недостаточно данных
            }
        } else {
            return HAL_ERROR; // нет данных
        }
    } else if (evt.status == osEventTimeout) {
        // В случае тайм-аута
        STM_LOG(LOG_WARN "Timeout waiting for response");
        return HAL_TIMEOUT;
    }

    return HAL_ERROR; // неизвестная ошибка
}

//отправка команды на вторичную плату с ожиданием ответа и таймаутом
HAL_StatusTypeDef send_command_with_response(DEV_t *dev, cmd_t command, uint8_t *response_buffer, uint16_t *response_size, uint32_t timeout_ms) {
    if (dev == nullptr || response_buffer == nullptr || response_size == nullptr) {
        return HAL_ERROR;
    }

    // Отправляем команду на вторичную плату
    uart_send_packet(&huart1, dev->Addr, command, nullptr, 0);

    // Ожидаем ответ
    osEvent evt = osMessageGet(rxDataUART1Handle, timeout_ms); // ждем ответ

    if (evt.status == osEventMessage) {
        uint32_t size = evt.value.v;

        uint8_t *rx_data = nullptr;
        uint16_t rx_data_size = 0;
        Header_t header;

        uart_parse_packet(message_rx, size, &header, &rx_data, &rx_data_size); // парсим пакет

        if (header.cmd != command) {
            return HAL_ERROR; // неверная команда
        }

        if (rx_data != nullptr && rx_data_size > 0) {
            // Копируем данные в буфер ответа
            uint16_t copy_size = (rx_data_size < *response_size) ? rx_data_size : *response_size;
            memcpy(response_buffer, rx_data, copy_size);
            *response_size = copy_size;
            return HAL_OK;
        } else {
            *response_size = 0;
            return HAL_ERROR; // нет данных
        }
    } else if (evt.status == osEventTimeout) {
        // В случае тайм-аута
        STM_LOG(LOG_WARN "Timeout waiting for response");
        *response_size = 0;
        return HAL_TIMEOUT;
    }

    return HAL_ERROR; // неизвестная ошибка
}

//отправка команды c данными на вторичную плату с ожиданием ответа и таймаутом
HAL_StatusTypeDef send_cmd_data_with_response(DEV_t *dev, cmd_t command, uint8_t *send_data, uint16_t send_size, uint8_t *response_buffer, uint16_t *response_size, uint32_t timeout_ms) {
  if (dev == nullptr || response_buffer == nullptr || response_size == nullptr) {
    return HAL_ERROR;
  }

  // Отправляем команду с данными на вторичную плату
  uart_send_packet(&huart1, dev->Addr, command, send_data, send_size);

  // Ожидаем ответ
  osEvent evt = osMessageGet(rxDataUART1Handle, timeout_ms);

  if (evt.status == osEventMessage) {
    uint32_t size = evt.value.v;

    uint8_t *rx_data = nullptr;
    uint16_t rx_data_size = 0;
    Header_t header;

    uart_parse_packet(message_rx, size, &header, &rx_data, &rx_data_size);

    if (header.cmd != command) {
      return HAL_ERROR; // неверная команда
    }

    if (rx_data != nullptr && rx_data_size > 0) {
      // Копируем данные в буфер ответа
      uint16_t copy_size = (rx_data_size < *response_size) ? rx_data_size : *response_size;
      memcpy(response_buffer, rx_data, copy_size);
      *response_size = copy_size;
      return HAL_OK;
    } else {
      *response_size = 0;
      return HAL_ERROR; // нет данных
    }
  } else if (evt.status == osEventTimeout) {
    // В случае тайм-аута
    STM_LOG(LOG_WARN "Timeout waiting for response");
    *response_size = 0;
    return HAL_TIMEOUT;
  }

  return HAL_ERROR; // неизвестная ошибка
}

uint8_t SetSecondaryBoardToBootloader(DEV_t *dev){
  // читаем текущее состояние
  if (dev == nullptr) {
      STM_LOG(LOG_ERR "DEV_t pointer is null");
      return 1;
  }
  
  secondary_status_t status;
  
  // Получаем текущий статус
  if (GetSecondaryBoardStatus(dev, &status, 1000) != HAL_OK) {
      STM_LOG(LOG_WARN "Failed to get initial status");
      return 1;
  }

  if(status.mode == 1){
    return 0; // уже в загрузчике
  }else if(status.mode == 2){
    // Отправляем команду на вторичную плату для перехода в режим загрузчика
    cmd_t cmd = cmd_t::enter_boot_mode;
    uart_send_packet(&huart1, dev->Addr, cmd, nullptr, 0);
  }
  
  // пауза
  osDelay(100);

  // проверка статуса в цикле с ожиданием ответа
  for(uint8_t i = 0; i < 5; i++){
    if (GetSecondaryBoardStatus(dev, &status, 100) == HAL_OK) {
        if(status.mode == 1){
          return 0; // успешно перешли в загрузчик
        }
    } else {
        STM_LOG(LOG_WARN "Failed to get status on attempt %d", i+1);
    }
    osDelay(100);
  }

  return 1; // не удалось перейти в загрузчик
}
