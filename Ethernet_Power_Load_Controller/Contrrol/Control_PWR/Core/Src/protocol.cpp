#include "protocol.h"

extern uint8_t message_rx[message_RX_LENGTH];
// формат пакета [addres][cmd][size][data...][crc_lo][crc_hi]

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
void deserialize_ret_pwm_data(uint8_t *buff, ret_pwm_ch_t *data)
{
  // Проверка на корректность входных параметров
  if (buff == NULL || data == NULL)
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

    return HAL_UART_Transmit(huart, UART_tx, size, 100);
}

// Получение пакета из буфера (возвращает длину полезных данных, либо 0 если ошибка CRC/размера)
uint16_t uart_parse_packet(uint8_t *buf, uint16_t buf_len, Header_t *header, uint8_t *data, uint16_t *data_len)
{
  if (buf_len < 5) // минимум: адрес(1)+cmd(1)+size(1)+crc(2)
    return 0;

  uint8_t packet_addr = buf[0];
  uint8_t packet_cmd = buf[1];
  uint8_t packet_size = buf[2];

  if (packet_size > buf_len || packet_size < 5)
    return 0;

  uint16_t crc_calc = crc16_fast(buf, packet_size - 2);
  uint16_t crc_recv = buf[packet_size - 2] | (buf[packet_size - 1] << 8);

  if (crc_calc != crc_recv)
    return 0;

  if (header)
  {
    header->address = packet_addr;
    header->cmd = packet_cmd;
    header->size = packet_size;
  }

  if (data && data_len)
  {
    *data_len = packet_size - 5;
    data = &buf[3];
  }
  return packet_size;
}

// Отправка пакета статуса (r_status) из MainTask
void send_status_packet(UART_HandleTypeDef *huart, uint32_t own_addr, uint8_t tupe_pcb, uint8_t stat_flash)
{
  uint8_t data[6];
  data[0] = own_addr;
  data[1] = tupe_pcb;
  data[2] = stat_flash;
  data[3] = 0;
  data[4] = 0;
  data[5] = 0;
  uart_send_packet(huart, own_addr, r_status, data, 6);
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
uint32_t auto_search_dev(DEV_t *dev, uint8_t size_dev)
{

  uint16_t data_len = 0;
  uint8_t rx_data[10];

  uint16_t found_count = 0;
  for (uint8_t i = 0; i < size_dev; ++i) {
    uint8_t addr = START_ADR_I2C + i;

    uint8_t tx_buf[8] = {0};

    // Обновленный вызов uart_send_packet: теперь требуется указать UART_HandleTypeDef*
    HAL_StatusTypeDef status = uart_send_packet(&huart1, addr, r_status, tx_buf, sizeof(tx_buf));
    if (status != HAL_OK) {
      continue;
    }

    osEvent evt = osMessageGet(rxDataUART1Handle, 200);
    if (evt.status != osEventMessage) {
      continue;
    }

    uint16_t rx_size = evt.value.v;

    Header_t header;
    if (!uart_parse_packet(message_rx, rx_size, &header, rx_data, &data_len)) {
      continue;
    }

    // Проверка: адрес в пакете должен совпадать с ожидаемым адресом
    if (header.address != addr) {
      STM_LOG(LOG_ERR "Address mismatch: expected %d, got %d", addr, header.address);
      continue;
    }

    if (header.cmd != r_status) {
      STM_LOG(LOG_ERR "Unexpected command %d for address %d", header.cmd, addr);
      continue;
    }

    if (rx_size < data_len + 5) {
      STM_LOG(LOG_ERR "Invalid data size %d for address %d", rx_size, addr);
      continue;
    }

    dev[i].Addr = addr;
    dev[i].AddrFromDev = header.address;
    dev[i].TypePCB = (PCBType)rx_data[1];
    //dev[i].StatFlash = rx_data[2];

    if (rx_data[3] == 3) {
      dev[i].ch[0].used = true;
      dev[i].ch[1].used = true;
      dev[i].ch[2].used = true;
      dev[i].ch[3].used = false;
      dev[i].ch[4].used = false;
      dev[i].ch[5].used = false;
    } else if (rx_data[3] == 6) {
      dev[i].ch[0].used = true;
      dev[i].ch[1].used = true;
      dev[i].ch[2].used = true;
      dev[i].ch[3].used = true;
      dev[i].ch[4].used = true;
      dev[i].ch[5].used = true;
    }

    found_count++;
  }

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
void deserialize_buff_to_dev(uint8_t *buff, DEV_t *dev) {
    if (buff == nullptr || dev == nullptr) {
        return; // Минимальный размер: 6 байт (en1-en6) + 24 байта (PWM1-PWM6)
    }

    ret_pwm_ch_t pwm_data;
    clear_ret_pwm_data(&pwm_data);
    // Десериализуем данные из буфера
    deserialize_ret_pwm_data((uint8_t*)buff, &pwm_data);

    // Заполняем только те поля, которые есть в DEV_t
    dev->ch[0].On_off = pwm_data.en1;
    dev->ch[1].On_off = pwm_data.en2;
    dev->ch[2].On_off = pwm_data.en3;
    dev->ch[3].On_off = pwm_data.en4;
    dev->ch[4].On_off = pwm_data.en5;
    dev->ch[5].On_off = pwm_data.en6;

    dev->ch[0].PWM_out = pwm_data.PWM1;
    dev->ch[1].PWM_out = pwm_data.PWM2;
    dev->ch[2].PWM_out = pwm_data.PWM3;
    dev->ch[3].PWM_out = pwm_data.PWM4;
    dev->ch[4].PWM_out = pwm_data.PWM5;
    dev->ch[5].PWM_out = pwm_data.PWM6;

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
