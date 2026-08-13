#ifndef INC_I2C3_H_
#define INC_I2C3_H_

#include "stm32f4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

extern I2C_HandleTypeDef hi2c3;

void MX_I2C3_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_I2C3_H_ */
