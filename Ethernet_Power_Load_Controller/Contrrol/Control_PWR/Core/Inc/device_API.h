/*
 * device_API.h
 *
 *  Created on: 3 июл. 2023 г.
 *      Author: Ierixon-HP
 */
#ifndef INC_DEVICE_API_H_
#define INC_DEVICE_API_H_

#include "main.h"
#include "cmsis_os.h"
#include <stddef.h>

/* Максимальный размер выходного буфера ответа */
#define DEVICE_API_RESP_SIZE  512U

/**
 * @brief Выполнить команду(ы) формата "C{cmd}A{addr}D{data}N{data1}x ..."
 *        и сформировать текстовый ответ в out_buf.
 */
void Command_execution(const char *in_str, char *out_buf, size_t out_size);

#endif /* INC_DEVICE_API_H_ */
