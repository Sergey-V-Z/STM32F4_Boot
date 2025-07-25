/*
 * protocol.h
 *
 *  Created on: Jul 25, 2025
 *      Author: SERGEI
*/

// формат пакета [addres][cmd][size][data...][crc_lo][crc_hi]

#ifndef INC_PROTOCOL_H_
#define INC_PROTOCOL_H_

#include "main.h"

typedef struct {
    uint8_t address;   // Адрес устройства
    uint8_t cmd;       // Команда
    uint8_t size;      // Размер данных
} Header_t;


void deserialize_ch_data(uint8_t *buff, uint16_t size, pwm_ch_t *chanels);
void serialize_ch_data(uint8_t *buff, uint16_t *size, pwm_ch_t *chanels);

void serialize_ret_pwm_data(uint8_t *buff, uint16_t *size, ret_pwm_ch_t *data);
void deserialize_ret_pwm_data(uint8_t *buff, ret_pwm_ch_t *data);

void clear_ret_pwm_data(ret_pwm_ch_t *data);
void clear_pwm_data(pwm_ch_t *data);

//
HAL_StatusTypeDef uart_send_packet(UART_HandleTypeDef *huart, uint8_t addr, uint8_t cmd, const uint8_t *data, uint16_t data_len);
uint16_t uart_parse_packet(uint8_t *buf, uint16_t buf_len, Header_t *header, uint8_t *data, uint16_t *data_len);
void send_status_packet(UART_HandleTypeDef *huart, uint32_t own_addr, uint8_t tupe_pcb, uint8_t stat_flash);
void send_ret_pwm_packet(UART_HandleTypeDef *huart, uint8_t addr, ret_pwm_ch_t *ret_data);

uint32_t auto_search_dev(DEV_t *dev, uint8_t size_dev);
void serialize_dev_to_buff(uint8_t *buff, uint16_t *size, const DEV_t *dev);
void deserialize_buff_to_dev(uint8_t *buff, DEV_t *dev);
HAL_StatusTypeDef send_pwm_ch_to_dev(const DEV_t *dev);

#endif /* INC_PROTOCOL_H_ */
