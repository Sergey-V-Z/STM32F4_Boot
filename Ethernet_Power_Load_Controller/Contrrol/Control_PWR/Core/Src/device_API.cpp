/*
 * device_API.cpp
 *
 *  Created on: 3 июл. 2023 г.
 *      Author: Ierixon-HP
 */

#include "flash_spi.h"
#include "LED.h"
#include "lwip.h"
#include "api.h"
#include "device_API.h"
#include "main.h"
#include "protocol.h"
#include "firmware_update.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/*variables ---------------------------------------------------------*/
extern settings_t settings;
extern chName_t NameCH[MAX_CH_NAME];
extern DEV_t devices[MAX_ADR_DEV];
extern flash mem_spi;

/* -------------------------------------------------------------------------
 * Внутренние типы (без STL)
 * ------------------------------------------------------------------------- */
#define MAX_CMD_COUNT  16U
#define ERR_BUF_SIZE   48U
#define MSG_BUF_SIZE   80U

typedef struct {
    uint32_t cmd;
    uint32_t addres_var;
    uint32_t data_in;
    uint32_t data_in1;
    bool     need_resp;
    uint32_t data_out;
    char     err[ERR_BUF_SIZE];
    bool     f_bool;
} mesage_c_t;

/* Проверяет, состоит ли строка длиной len из цифр (допускается ведущий '-') */
static bool isNumeric_n(const char *s, size_t len)
{
    if (s == NULL || len == 0 || len >= 16U) return false;
    char tmp[16];
    memcpy(tmp, s, len);
    tmp[len] = '\0';
    char *p = NULL;
    strtol(tmp, &p, 10);
    return (p != NULL && *p == '\0');
}

/* Парсит число из строки длиной len */
static long parse_num_n(const char *s, size_t len)
{
    if (s == NULL || len == 0 || len >= 16U) return 0;
    char tmp[16];
    memcpy(tmp, s, len);
    tmp[len] = '\0';
    return strtol(tmp, NULL, 10);
}

/* Безопасный strncat с остатком */
static void safe_append(char *dst, const char *src, size_t dst_size)
{
    size_t used = strlen(dst);
    if (used < dst_size - 1U) {
        strncat(dst, src, dst_size - used - 1U);
    }
}

/* -------------------------------------------------------------------------
 * Публичная функция
 * ------------------------------------------------------------------------- */
void Command_execution(const char *in_str, char *out_buf, size_t out_size)
{
    if (in_str == NULL || out_buf == NULL || out_size == 0U) return;
    out_buf[0] = '\0';

    static char arr_msg[MAX_CMD_COUNT][MSG_BUF_SIZE];
    static mesage_c_t arr_cmd[MAX_CMD_COUNT];

    uint32_t msg_count = 0;
    bool errMSG = false;

    /* ---- Разбить на сообщения по разделителю 'x' ---- */
    const char *p = in_str;
    while (*p != '\0' && msg_count < MAX_CMD_COUNT) {
        const char *x = strchr(p, 'x');
        if (x == NULL) break;
        size_t len = (size_t)(x - p) + 1U; /* включая 'x' */
        if (len < MSG_BUF_SIZE) {
            memcpy(arr_msg[msg_count], p, len);
            arr_msg[msg_count][len] = '\0';
            msg_count++;
        }
        p = x + 1;
    }

    if (msg_count == 0U) {
        strncpy(out_buf, "err format message", out_size - 1U);
        out_buf[out_size - 1U] = '\0';
        return;
    }

    /* ---- Парсинг каждого сообщения ---- */
    uint32_t cmd_count = 0;
    for (uint32_t i = 0; i < msg_count && cmd_count < MAX_CMD_COUNT; i++) {
        mesage_c_t *m = &arr_cmd[cmd_count];
        memset(m, 0, sizeof(*m));
        strncpy(m->err, "OK", ERR_BUF_SIZE - 1U);

        const char *msg = arr_msg[i];
        const char *pC = strchr(msg, 'C');
        const char *pA = (pC != NULL) ? strchr(pC + 1, 'A') : NULL;
        const char *pD = (pA != NULL) ? strchr(pA + 1, 'D') : NULL;
        const char *pN = (pD != NULL) ? strchr(pD + 1, 'N') : NULL;
        const char *px = (pN != NULL) ? strchr(pN + 1, 'x') : NULL;

#define SET_ERR(m_, txt_) do { strncpy((m_)->err, (txt_), ERR_BUF_SIZE-1U); (m_)->f_bool = true; errMSG = true; } while(0)

        if (pC == NULL) { SET_ERR(m, "wrong format in C flag"); cmd_count++; continue; }
        if (pA == NULL) { SET_ERR(m, "wrong format in A flag"); cmd_count++; continue; }
        if (pD == NULL) { SET_ERR(m, "wrong format in D flag"); cmd_count++; continue; }
        if (pN == NULL) { SET_ERR(m, "wrong format in N flag"); cmd_count++; continue; }
        if (px == NULL) { SET_ERR(m, "wrong format in x flag"); cmd_count++; continue; }

        size_t lenCmd   = (size_t)(pA - pC - 1U);
        size_t lenAddr  = (size_t)(pD - pA - 1U);
        size_t lenData  = (size_t)(pN - pD - 1U);
        size_t lenData1 = (size_t)(px - pN - 1U);

        if (!isNumeric_n(pC + 1, lenCmd))   { SET_ERR(m, "err after C is not number"); cmd_count++; continue; }
        if (!isNumeric_n(pA + 1, lenAddr))  { SET_ERR(m, "err after A is not number"); cmd_count++; continue; }
        if (!isNumeric_n(pD + 1, lenData))  { SET_ERR(m, "err after D is not number"); cmd_count++; continue; }
        if (!isNumeric_n(pN + 1, lenData1)) { SET_ERR(m, "err after N is not number"); cmd_count++; continue; }

#undef SET_ERR

        m->cmd        = (uint32_t)parse_num_n(pC + 1, lenCmd);
        m->addres_var = (uint32_t)parse_num_n(pA + 1, lenAddr);
        m->data_in    = (uint32_t)parse_num_n(pD + 1, lenData);
        m->data_in1   = (uint32_t)parse_num_n(pN + 1, lenData1);
        cmd_count++;
    }

    /* ---- Выполнение команд ---- */
    if (!errMSG) {
        for (uint32_t i = 0; i < cmd_count; i++) {
            switch (arr_cmd[i].cmd) {
            /* Channel on/off */
            case 4: {
                osMutexWait(varMutexDevicesHandle, osWaitForever);
                if (arr_cmd[i].addres_var >= 1U) {
                    for (int name = 0; name < MAX_CH_NAME; name++) {
                        if (NameCH[name].dev != NULL) {
                            uint8_t c = NameCH[name].Channel_number;
                            NameCH[name].dev->ch[c].On_off = (uint8_t)arr_cmd[i].data_in;
                        }
                    }
                    strncpy(arr_cmd[i].err, "OK", ERR_BUF_SIZE-1U);
                } else {
                    if (arr_cmd[i].data_in1 < MAX_CH_NAME && NameCH[arr_cmd[i].data_in1].dev != NULL) {
                        uint8_t c = NameCH[arr_cmd[i].data_in1].Channel_number;
                        NameCH[arr_cmd[i].data_in1].dev->ch[c].On_off = (uint8_t)arr_cmd[i].data_in;
                        strncpy(arr_cmd[i].err, "OK", ERR_BUF_SIZE-1U);
                    } else {
                        strncpy(arr_cmd[i].err, "NULL ptr dev", ERR_BUF_SIZE-1U);
                    }
                }
                osMutexRelease(varMutexDevicesHandle);
                break;
            }
            /* PWM set to Channel */
            case 5: {
                osMutexWait(varMutexDevicesHandle, osWaitForever);
                if (arr_cmd[i].addres_var >= 1U) {
                    for (int name = 0; name < MAX_CH_NAME; name++) {
                        if (NameCH[name].dev != NULL) {
                            uint8_t c = NameCH[name].Channel_number;
                            NameCH[name].dev->ch[c].PWM_out = arr_cmd[i].data_in;
                        }
                    }
                    strncpy(arr_cmd[i].err, "OK", ERR_BUF_SIZE-1U);
                } else {
                    if (arr_cmd[i].data_in1 < MAX_CH_NAME && NameCH[arr_cmd[i].data_in1].dev != NULL) {
                        uint8_t c = NameCH[arr_cmd[i].data_in1].Channel_number;
                        NameCH[arr_cmd[i].data_in1].dev->ch[c].PWM_out = arr_cmd[i].data_in;
                        strncpy(arr_cmd[i].err, "OK", ERR_BUF_SIZE-1U);
                    } else {
                        strncpy(arr_cmd[i].err, "NULL ptr dev", ERR_BUF_SIZE-1U);
                    }
                }
                osMutexRelease(varMutexDevicesHandle);
                break;
            }
            /* PWM read from channel */
            case 6: {
                osMutexWait(varMutexDevicesHandle, osWaitForever);
                if (arr_cmd[i].data_in1 < MAX_CH_NAME && NameCH[arr_cmd[i].data_in1].dev != NULL) {
                    uint8_t c = NameCH[arr_cmd[i].data_in1].Channel_number;
                    arr_cmd[i].data_out = NameCH[arr_cmd[i].data_in1].dev->ch[c].PWM;
                    arr_cmd[i].need_resp = true;
                    strncpy(arr_cmd[i].err, "OK", ERR_BUF_SIZE-1U);
                } else {
                    strncpy(arr_cmd[i].err, "NULL ptr dev", ERR_BUF_SIZE-1U);
                }
                osMutexRelease(varMutexDevicesHandle);
                break;
            }
            /* Current read from channel */
            case 7: {
                osMutexWait(varMutexDevicesHandle, osWaitForever);
                if (arr_cmd[i].data_in1 < MAX_CH_NAME && NameCH[arr_cmd[i].data_in1].dev != NULL) {
                    uint8_t c = NameCH[arr_cmd[i].data_in1].Channel_number;
                    arr_cmd[i].data_out = NameCH[arr_cmd[i].data_in1].dev->ch[c].Current;
                    arr_cmd[i].need_resp = true;
                    strncpy(arr_cmd[i].err, "OK", ERR_BUF_SIZE-1U);
                } else {
                    strncpy(arr_cmd[i].err, "NULL ptr dev", ERR_BUF_SIZE-1U);
                }
                osMutexRelease(varMutexDevicesHandle);
                break;
            }
            /* IsOn */
            case 8: {
                osMutexWait(varMutexDevicesHandle, osWaitForever);
                if (arr_cmd[i].data_in1 < MAX_CH_NAME && NameCH[arr_cmd[i].data_in1].dev != NULL) {
                    uint8_t c = NameCH[arr_cmd[i].data_in1].Channel_number;
                    arr_cmd[i].data_out = NameCH[arr_cmd[i].data_in1].dev->ch[c].IsOn;
                    arr_cmd[i].need_resp = true;
                    strncpy(arr_cmd[i].err, "OK", ERR_BUF_SIZE-1U);
                } else {
                    strncpy(arr_cmd[i].err, "NULL ptr dev", ERR_BUF_SIZE-1U);
                }
                osMutexRelease(varMutexDevicesHandle);
                break;
            }
            /* Save to flash */
            case 9: {
                mem_spi.W25qxx_EraseSector(SPI_FLASH_CONFIG_ADDRESS);
                osDelay(5);
                mem_spi.Write(settings);
                strncpy(arr_cmd[i].err, "OK", ERR_BUF_SIZE-1U);
                break;
            }
            /* Set IP end from settings */
            case 10: {
                settings.IP_end_from_settings = (uint8_t)arr_cmd[i].data_in;
                strncpy(arr_cmd[i].err, "OK", ERR_BUF_SIZE-1U);
                break;
            }
            /* Reboot */
            case 11: {
                if (arr_cmd[i].data_in) {
                    NVIC_SystemReset();
                }
                strncpy(arr_cmd[i].err, "OK", ERR_BUF_SIZE-1U);
                break;
            }
            /* DHCP */
            case 12: {
                settings.DHCPset = (uint8_t)arr_cmd[i].data_in;
                strncpy(arr_cmd[i].err, "OK", ERR_BUF_SIZE-1U);
                break;
            }
            /* IP octet */
            case 13: {
                if (arr_cmd[i].addres_var < 4U) {
                    settings.saveIP.ip[arr_cmd[i].addres_var] = (uint8_t)arr_cmd[i].data_in;
                }
                strncpy(arr_cmd[i].err, "OK", ERR_BUF_SIZE-1U);
                break;
            }
            /* MASK octet */
            case 14: {
                if (arr_cmd[i].addres_var < 4U) {
                    settings.saveIP.mask[arr_cmd[i].addres_var] = (uint8_t)arr_cmd[i].data_in;
                }
                strncpy(arr_cmd[i].err, "OK", ERR_BUF_SIZE-1U);
                break;
            }
            /* GW octet */
            case 15: {
                if (arr_cmd[i].addres_var < 4U) {
                    settings.saveIP.gateway[arr_cmd[i].addres_var] = (uint8_t)arr_cmd[i].data_in;
                }
                strncpy(arr_cmd[i].err, "OK", ERR_BUF_SIZE-1U);
                break;
            }
            /* MAC octet */
            case 16: {
                if (arr_cmd[i].addres_var < 6U) {
                    settings.MAC[arr_cmd[i].addres_var] = (uint8_t)arr_cmd[i].data_in;
                }
                strncpy(arr_cmd[i].err, "OK", ERR_BUF_SIZE-1U);
                break;
            }
            /* Errors */
            case 17: {
                osMutexWait(varMutexDevicesHandle, osWaitForever);
                if (arr_cmd[i].data_in1 < MAX_CH_NAME && NameCH[arr_cmd[i].data_in1].dev != NULL) {
                    if (arr_cmd[i].addres_var) {
                        arr_cmd[i].data_out = NameCH[arr_cmd[i].data_in1].dev->ERR_counter;
                        NameCH[arr_cmd[i].data_in1].dev->ERR_counter = 0;
                    } else {
                        arr_cmd[i].data_out = NameCH[arr_cmd[i].data_in1].dev->last_ERR;
                        NameCH[arr_cmd[i].data_in1].dev->last_ERR = 0;
                    }
                    arr_cmd[i].need_resp = true;
                    strncpy(arr_cmd[i].err, "OK", ERR_BUF_SIZE-1U);
                } else {
                    strncpy(arr_cmd[i].err, "NULL ptr dev", ERR_BUF_SIZE-1U);
                }
                osMutexRelease(varMutexDevicesHandle);
                break;
            }
            /* Bridge settings */
            case 18: {
                if (arr_cmd[i].addres_var) {
                    if (arr_cmd[i].data_in) {
                        mem_spi.Write(settings);
                    }
                } else {
                    arr_cmd[i].data_out = (uint32_t)settings.bridge_sett.mode_rs485;
                    arr_cmd[i].need_resp = true;
                }
                strncpy(arr_cmd[i].err, "OK", ERR_BUF_SIZE-1U);
                break;
            }
            default: {
                strncpy(arr_cmd[i].err, "Command does not exist", ERR_BUF_SIZE-1U);
                arr_cmd[i].f_bool = true;
                break;
            }
            }
        }
    }

    /* ---- Формируем ответ ---- */
    if (!errMSG) {
        char tmp[32];
        for (uint32_t i = 0; i < cmd_count; i++) {
            if (!arr_cmd[i].f_bool) {
                snprintf(tmp, sizeof(tmp), "C%lu", arr_cmd[i].cmd);
                safe_append(out_buf, tmp, out_size);
                if (arr_cmd[i].need_resp) {
                    snprintf(tmp, sizeof(tmp), "D%lu", arr_cmd[i].data_out);
                    safe_append(out_buf, tmp, out_size);
                } else {
                    safe_append(out_buf, " ", out_size);
                    safe_append(out_buf, arr_cmd[i].err, out_size);
                }
                safe_append(out_buf, "x", out_size);
            } else {
                safe_append(out_buf, arr_cmd[i].err, out_size);
            }
        }
    } else {
        strncpy(out_buf, arr_cmd[0].err, out_size - 1U);
        out_buf[out_size - 1U] = '\0';
    }
}
