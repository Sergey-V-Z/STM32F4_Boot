/*
 * device_API.cpp
 *
 *  Created on: 3 июл. 2023 г.
 *      Author: Ierixon-HP
 */

#include "flash_spi.h"
#include "LED.h"
#include "lwip.h"
using namespace std;
#include <string>
#include "api.h"
#include <iostream>
#include <vector>
#include "device_API.h"
#include "main.h"
#include "protocol.h"
#include "firmware_update.h"
#include "pwm_controller.h"
#include "tca9548a.h"
#include "dac_settings.h"
#include "log_buffer.h"

/*variables ---------------------------------------------------------*/
extern settings_t settings;
extern flash mem_spi;
extern float g_current_amperes;  // Ток с датчика ACS712 в амперах
extern float g_current_zero_offset;  // Калибровочное смещение нуля

// Функции для работы с ACS712 (определены в freertos.cpp)
void ACS712_CalibrateZero(void);

/* Typedef -----------------------------------------------------------*/
struct mesage_t
{
	uint32_t cmd;
	uint32_t addres_var;
	uint32_t data_in;
	uint32_t data_in1;
	bool need_resp = false;
	bool data_in_is;
	uint32_t data_out;
	int32_t data_out_signed;  // Для знаковых значений (например, ток)
	bool use_signed = false;  // Флаг использования знакового значения
	string err;			 // сообщение клиенту об ошибке в сообщении
	bool f_bool = false; // наличие ошибки в сообшении
	bool has_raw_payload = false; // true только для cmd 28 (дамп лог-буфера)
	string raw_payload;           // сырые байты дампа для has_raw_payload
};

// Флаги для разбора сообщения
string f_cmd("C");
string f_addr("A");
string f_datd("D");
string f_datd1("N");
string delim("x");

string Сommand_execution(string in_str)
{
	// Парсинг
	vector<string> arr_msg;
	vector<mesage_t> arr_cmd;
	size_t prev = 0;
	size_t next;
	size_t delta = delim.length();
	bool errMSG = false;

	// разбить на сообщения
	while ((next = in_str.find(delim, prev)) != string::npos)
	{
		arr_msg.push_back(in_str.substr(prev, (next + 1) - prev));
		prev = next + delta;
	}
	// arr_msg.push_back( in_str.substr( prev ) );
	if (arr_msg.size() != 0)
	{
		// занести сообщения в структуру
		int count_msg = arr_msg.size();
		for (int i = 0; i < count_msg; ++i)
		{
			prev = 0;
			next = 0;
			size_t posC = 0;
			size_t posA = 0;
			size_t posD = 0;
			size_t posN = 0;
			size_t posx = 0;
			mesage_t temp_msg;

			// выделение комманды
			delta = f_cmd.length();
			next = arr_msg[i].find(f_cmd);
			posC = next;
			if (next == string::npos)
			{
				// Ошибка
				temp_msg.err = "wrong format in C flag";
				errMSG = true;
				arr_cmd.push_back(temp_msg);
				continue;
			}
			prev = next + delta;

			// выделение адреса
			delta = f_addr.length();
			next = arr_msg[i].find(f_addr, prev);
			posA = next;
			if (next == string::npos)
			{
				// Ошибка
				temp_msg.err = "wrong format in A flag";
				errMSG = true;
				arr_cmd.push_back(temp_msg);
				continue;
			}
			prev = next + delta;

			// выделение данных
			delta = f_datd.length();
			next = arr_msg[i].find(f_datd, prev);
			posD = next;
			if (next == string::npos)
			{
				// Ошибка
				temp_msg.err = "wrong format in D flag";
				errMSG = true;
				arr_cmd.push_back(temp_msg);
				continue;
			}
			prev = next + delta;

			// выделение данных 1
			delta = f_datd1.length();
			next = arr_msg[i].find(f_datd1, prev);
			posN = next;
			if (next == string::npos)
			{
				// Ошибка
				temp_msg.err = "wrong format in N flag";
				errMSG = true;
				arr_cmd.push_back(temp_msg);
				continue;
			}
			prev = next + delta;

			// выделение данных
			delta = delim.length();
			next = arr_msg[i].find(delim, prev);
			posx = next;
			if (next == string::npos)
			{
				// Ошибка
				temp_msg.err = "wrong format in x flag";
				errMSG = true;
				arr_cmd.push_back(temp_msg);
				continue;
			}

			bool isNum;
			// from flag "C" to flag "A" is a number ?
			isNum = isNumeric(arr_msg[i].substr(posC + 1, (posA - 1) - posC));

			// chain, check parameters
			if (isNum)
			{
				temp_msg.cmd = (uint32_t)stoi(arr_msg[i].substr(posC + 1, (posA - 1) - posC)); // get number from string
				// from flag "A" to flag "D" is a number ?
				isNum = isNumeric(arr_msg[i].substr(posA + 1, (posD - 1) - posA));

				if (isNum)
				{
					temp_msg.addres_var = (uint32_t)stoi(arr_msg[i].substr(posA + 1, (posD - 1) - posA)); // get number from string
					// from flag "D" to flag "N" is a number ?
					isNum = isNumeric(arr_msg[i].substr(posD + 1, (posN - 1) - posD));

					if (isNum)
					{
						temp_msg.data_in = (uint32_t)stoi(arr_msg[i].substr(posD + 1, (posN - 1) - posD)); // get number from string
						// from flag "N" to flag "x" is a number ?
						isNum = isNumeric(arr_msg[i].substr(posN + 1, (posx - 1) - posN));

						if (isNum)
						{
							temp_msg.data_in1 = (uint32_t)stoi(arr_msg[i].substr(posN + 1, (posx - 1) - posN)); // get number from string
						}
						else
						{
							temp_msg.err = "err after N is not number";
							errMSG = true;
						}
					}
					else
					{
						temp_msg.err = "err after D is not number";
						errMSG = true;
					}
				}
				else
				{
					temp_msg.err = "err after A is not number";
					errMSG = true;
				}
			}
			else
			{
				temp_msg.err = "err after C is not number";
				errMSG = true;
			}

			arr_cmd.push_back(temp_msg);
		}
	}
	else
	{
		mesage_t temp_msg;
		temp_msg.err = "err format message";
		arr_cmd.push_back(temp_msg);
		errMSG = true;
	}
	// Закончили парсинг

	/*-----------------------------------------------------------------------------------------------------------------------------*/
	// Выполнение комманд
	int count_cmd = arr_cmd.size();
	int ret = 0;
	if (!errMSG)
	{
		for (int i = 0; i < count_cmd; ++i)
		{

			switch (arr_cmd[i].cmd)
			{
			// Delet dev
			case 1:
			{
				if (arr_cmd[i].data_in1 == 0)
				{

					// сигнал на повторный поиск плат

					arr_cmd[i].err = "OK";
				}
				else if (arr_cmd[i].data_in1 == 1)
				{
					arr_cmd[i].err = "not available";
				}
				else if (arr_cmd[i].data_in1 == 2)
				{
					// cleanAll_i2c_dev();
					arr_cmd[i].err = "OK";
				}
				else
				{
					arr_cmd[i].err = "bead param";
				}

				break;
			}
			// Add dev in cell
			case 2:
			{

				// ret = set_i2c_dev(arr_cmd[i].addres_var, arr_cmd[i].data_in, arr_cmd[i].data_in1);
				switch (ret)
				{
				case 1:
					arr_cmd[i].err = "err not valid chanel data";
					arr_cmd[i].f_bool = true;
					break;
				case 2:
					arr_cmd[i].err = "err not empty cell";
					arr_cmd[i].f_bool = true;
					break;
				case 3:
					arr_cmd[i].err = "err not valid addres";
					arr_cmd[i].f_bool = true;
					break;
				default:
					arr_cmd[i].err = "OK";
					break;
				}

				break;
			}
			// Delet dev
			case 3:
			{
				// del_Name_dev(arr_cmd[i].data_in1);
				arr_cmd[i].err = "OK";
				break;
			}
			// Chanel on/off/fade-on/fade-off
			// D0 = instant off, D1 = instant on, D3 = fade on, D4 = fade off
			case 4:
			{
				uint32_t mode = arr_cmd[i].data_in;
				// mode set all channels (addres_var >= 1)
				if (arr_cmd[i].addres_var >= 1)
				{
					HAL_StatusTypeDef st;
					if (mode == 1) {
						st = DAC_ChannelOnAll();
					} else if (mode == 0) {
						st = DAC_ChannelOffAll();
					} else if (mode == 3) {
						st = DAC_ChannelFadeOnAll();
					} else if (mode == 4) {
						st = DAC_ChannelFadeOffAll();
					} else {
						arr_cmd[i].err = "Invalid mode";
						arr_cmd[i].f_bool = true;
						break;
					}
					// Мгновенно (mode 0/1) статус подтверждён обратным чтением ЦАП;
					// для fade (mode 3/4) — только то, что переход успешно запущен.
					arr_cmd[i].err = (st == HAL_OK) ? "OK" : "FAIL";
				}
				else
				{
					// mode set one channel
					uint32_t ch = arr_cmd[i].data_in1;
					if (ch < PWM_CH_COUNT)
					{
						HAL_StatusTypeDef st;
						if (mode == 1) {
							st = DAC_ChannelOn((uint8_t)ch);
						} else if (mode == 0) {
							st = DAC_ChannelOff((uint8_t)ch);
						} else if (mode == 3) {
							st = DAC_ChannelFadeOn((uint8_t)ch);
						} else if (mode == 4) {
							st = DAC_ChannelFadeOff((uint8_t)ch);
						} else {
							arr_cmd[i].err = "Invalid mode";
							arr_cmd[i].f_bool = true;
							break;
						}
						arr_cmd[i].err = (st == HAL_OK) ? "OK" : "FAIL";
					}
					else
					{
						arr_cmd[i].err = "Invalid channel";
					}
				}
				break;
			}
			// PWM set to Channel
			case 5:
			{
				// mode set all channel
				if (arr_cmd[i].addres_var >= 1)
				{
					PWM_SetAllChannels((uint16_t)arr_cmd[i].data_in);
					arr_cmd[i].err = "OK";
				}
				else
				{
					// mode set one channel
					uint32_t ch = arr_cmd[i].data_in1;
					if (ch < PWM_CH_COUNT)
					{
						PWM_SetValue((PWM_Channel_t)ch, (uint16_t)arr_cmd[i].data_in);
						arr_cmd[i].err = "OK";
					}
					else
					{
						arr_cmd[i].err = "Invalid channel";
					}
				}
				break;
			}
			// PWM read from channel
			case 6:
			{
				uint32_t ch = arr_cmd[i].data_in1;
				if (ch < PWM_CH_COUNT)
				{
					arr_cmd[i].data_out = PWM_GetValue((PWM_Channel_t)ch);
					arr_cmd[i].need_resp = true;
					arr_cmd[i].err = "OK";
				}
				else
				{
					arr_cmd[i].err = "Invalid channel";
				}
				break;
			}
			// Fade duration set/get
			// data_in1=0: set duration (data_in = ms, 50-10000)
			// data_in1=1: get current duration
			case 7:
			{
				if (arr_cmd[i].data_in1 == 1)
				{
					arr_cmd[i].data_out = DAC_GetFadeDuration();
					arr_cmd[i].need_resp = true;
					arr_cmd[i].err = "OK";
				}
				else
				{
					uint32_t ms = arr_cmd[i].data_in;
					if (ms >= 50 && ms <= 10000)
					{
						DAC_SetFadeDuration((uint16_t)ms);
						arr_cmd[i].err = "OK";
					}
					else
					{
						arr_cmd[i].err = "Invalid range (50-10000 ms)";
					}
				}
				break;
			}
			// is on
			case 8:
			{
				uint32_t ch = arr_cmd[i].data_in1;
				if (ch < PWM_CH_COUNT)
				{
					arr_cmd[i].data_out = DAC_ChannelIsOn((uint8_t)ch) ? 1 : 0;
					arr_cmd[i].need_resp = true;
					arr_cmd[i].err = "OK";
				}
				else
				{
					arr_cmd[i].err = "Invalid channel";
				}
				break;
			}
			// save
			case 9:
			{
				mem_spi.W25qxx_EraseSector(SPI_FLASH_CONFIG_ADDRESS);
				osDelay(5);
				mem_spi.Write(settings);
				arr_cmd[i].err = "OK";
				break;
			}
			// load from flash
			case 10:
			{
				settings.IP_end_from_settings = (uint8_t)arr_cmd[i].data_in;
				arr_cmd[i].err = "OK";
				break;
			}
			// Reboot
			case 11:
			{
				if (arr_cmd[i].data_in)
				{
					NVIC_SystemReset();
				}
				arr_cmd[i].err = "OK";
				break;
			}
			// DHCP
			case 12:
			{
				settings.DHCPset = (uint8_t)arr_cmd[i].data_in;
				arr_cmd[i].err = "OK";
				break;
			}
			// IP
			case 13:
			{
				settings.saveIP.ip[arr_cmd[i].addres_var] = arr_cmd[i].data_in;
				arr_cmd[i].err = "OK";
				break;
			}
			// MASK
			case 14:
			{
				settings.saveIP.mask[arr_cmd[i].addres_var] = arr_cmd[i].data_in;
				arr_cmd[i].err = "OK";
				break;
			}
			// GW
			case 15:
			{
				settings.saveIP.gateway[arr_cmd[i].addres_var] = arr_cmd[i].data_in;
				arr_cmd[i].err = "OK";
				break;
			}
			// MAC
			case 16:
			{
				settings.MAC[arr_cmd[i].addres_var] = arr_cmd[i].data_in;
				arr_cmd[i].err = "OK";
				break;
			}
			// errors - not applicable for direct PWM control
			case 17:
			{
				arr_cmd[i].data_out = 0;
				arr_cmd[i].need_resp = true;
				arr_cmd[i].err = "Not available";
				break;
			}
			// bridge_settings - removed
			case 18:
			{
				arr_cmd[i].err = "Not available";
				break;
			}
			// Read current from ACS712 sensor
			case 19:
			{
				// Преобразуем float в int32_t (умножаем на 1000 для передачи с точностью до миллиампер)
				// Например: 5.234A -> 5234, -0.234A -> -234
				int32_t current_ma = (int32_t)(g_current_amperes * 1000.0f);
				arr_cmd[i].data_out_signed = current_ma;
				arr_cmd[i].use_signed = true;
				arr_cmd[i].need_resp = true;
				arr_cmd[i].err = "OK";
				break;
			}
			// Calibrate current sensor zero point
			case 20:
			{
				if (arr_cmd[i].addres_var == 0)
				{
					// A=0: Выполнить калибровку нуля
					ACS712_CalibrateZero();
					// Возвращаем смещение в мА
					int32_t offset_ma = (int32_t)(g_current_zero_offset * 1000.0f);
					arr_cmd[i].data_out_signed = offset_ma;
					arr_cmd[i].use_signed = true;
					arr_cmd[i].need_resp = true;
					arr_cmd[i].err = "OK";
				}
				else if (arr_cmd[i].addres_var == 1)
				{
					// A=1: Чтение текущего смещения
					int32_t offset_ma = (int32_t)(g_current_zero_offset * 1000.0f);
					arr_cmd[i].data_out_signed = offset_ma;
					arr_cmd[i].use_signed = true;
					arr_cmd[i].need_resp = true;
					arr_cmd[i].err = "OK";
				}
				else if (arr_cmd[i].addres_var == 2)
				{
					// A=2: Сброс калибровки
					g_current_zero_offset = 0.0f;
					arr_cmd[i].need_resp = false;
					arr_cmd[i].err = "OK";
				}
				else
				{
					arr_cmd[i].err = "Invalid address";
					arr_cmd[i].f_bool = true;
				}
				break;
			}
			// Set percent (0-1000) for channel
			// C21A<ch>D<pct>N0x  — one channel; A>=6: all channels
			case 21:
			{
				uint32_t pct = arr_cmd[i].data_in;
				if (pct > DAC_PERCENT_MAX) pct = DAC_PERCENT_MAX;
				if (arr_cmd[i].addres_var >= DAC_CH_COUNT)
				{
					HAL_StatusTypeDef st = HAL_OK;
					for (uint8_t c = 0; c < DAC_CH_COUNT; c++)
						if (DAC_SetPercent(c, (uint16_t)pct) != HAL_OK) st = HAL_ERROR;
					arr_cmd[i].err = (st == HAL_OK) ? "OK" : "FAIL";
				}
				else
				{
					uint8_t ch = (uint8_t)arr_cmd[i].addres_var;
					DAC_SetPercent(ch, (uint16_t)pct);
					arr_cmd[i].data_out = g_dac_setpoints[ch];
					arr_cmd[i].need_resp = true;
					arr_cmd[i].err = "OK";
				}
				break;
			}
			// Read percent setpoint for channel
			// C22A<ch>D0N0x  → D<percent 0-1000>
			case 22:
			{
				uint8_t ch = (uint8_t)arr_cmd[i].addres_var;
				if (ch < DAC_CH_COUNT)
				{
					arr_cmd[i].data_out = g_percent_setpoints[ch];
					arr_cmd[i].need_resp = true;
					arr_cmd[i].err = "OK";
				}
				else
				{
					arr_cmd[i].err = "Invalid channel";
					arr_cmd[i].f_bool = true;
				}
				break;
			}
			// Set raw DAC count (0-4095) — debug
			// C23A<ch>D<dac>N0x  → D<dac>
			case 23:
			{
				uint8_t ch = (uint8_t)arr_cmd[i].addres_var;
				uint32_t val = arr_cmd[i].data_in;
				if (ch < DAC_CH_COUNT)
				{
					if (val > MCP4725_DAC_MAX) val = MCP4725_DAC_MAX;
					PWM_SetValue((PWM_Channel_t)ch, PWM_MAX_VALUE);
					DAC_SetRaw(ch, (uint16_t)val);
					arr_cmd[i].data_out = val;
					arr_cmd[i].need_resp = true;
					arr_cmd[i].err = "OK";
				}
				else
				{
					arr_cmd[i].err = "Invalid channel";
					arr_cmd[i].f_bool = true;
				}
				break;
			}
			// Read raw DAC count for channel
			// C24A<ch>D0N0x  → D<dac>
			case 24:
			{
				uint8_t ch = (uint8_t)arr_cmd[i].addres_var;
				if (ch < DAC_CH_COUNT)
				{
					arr_cmd[i].data_out = g_dac_setpoints[ch];
					arr_cmd[i].need_resp = true;
					arr_cmd[i].err = "OK";
				}
				else
				{
					arr_cmd[i].err = "Invalid channel";
					arr_cmd[i].f_bool = true;
				}
				break;
			}
			// Disable channel(s): DAC=0, PWM=0
			// C25A<ch>D0N0x  — one channel; A>=6: all channels
			case 25:
			{
				HAL_StatusTypeDef st;
				if (arr_cmd[i].addres_var >= DAC_CH_COUNT)
				{
					st = DAC_DisableAll();
				}
				else
				{
					st = DAC_DisableChannel((uint8_t)arr_cmd[i].addres_var);
				}
				arr_cmd[i].err = (st == HAL_OK) ? "OK" : "FAIL";
				break;
			}
			// Read MCP4725 scan result for channel: D = (found<<8 | addr)
			// C26A<ch>D0N0x  → D<found_addr>
			case 26:
			{
				uint8_t ch = (uint8_t)arr_cmd[i].addres_var;
				if (ch < DAC_CH_COUNT)
				{
					arr_cmd[i].data_out = ((uint32_t)g_mcp4725_found[ch] << 8) | g_mcp4725_addr[ch];
					arr_cmd[i].need_resp = true;
					arr_cmd[i].err = "OK";
				}
				else
				{
					arr_cmd[i].err = "Invalid channel";
					arr_cmd[i].f_bool = true;
				}
				break;
			}
			// Сохранить уставки тока (percent) всех каналов во flash.
			// Бит включения/выключения (cmd 4) не сохраняется — см. dac_settings.h
			// C27A0D0N0x
			case 27:
			{
				DAC_Settings_Save();
				arr_cmd[i].err = "OK";
				break;
			}
			// Дамп RAM-буфера логов (дублирует debug UART, см. log_buffer.h).
			// Неразрушающее чтение — буфер продолжает накапливаться/перезаписываться.
			// A/D/N игнорируются (зарезервированы). C28A0D0N0x -> C28D<N>:<N raw bytes>x
			case 28:
			{
				static char snapBuf[LOG_BUFFER_SIZE]; // не на стеке — стек eth_Task всего 3072 байта
				uint32_t n = LogBuffer_Snapshot(snapBuf, sizeof(snapBuf));
				arr_cmd[i].raw_payload.assign(snapBuf, n);
				arr_cmd[i].has_raw_payload = true;
				break;
			}
			default:
			{
				arr_cmd[i].err = "Command does not exist";
				arr_cmd[i].f_bool = true;
				break;
			}
			}
		}
	}
	/*-----------------------------------------------------------------------------------------------------------------------------*/
	// Формируем ответ
	string resp;
	if (!errMSG)
	{
		for (int i = 0; i < count_cmd; ++i)
		{

			if (arr_cmd[i].has_raw_payload)
			{
				// Нестандартный, длина-префиксный формат ответа: сырые байты
				// дампа лога могут содержать что угодно (включая 'x','C','D'),
				// поэтому длина указывается явно перед ':' — см. cmd 28.
				resp.append(f_cmd + to_string(arr_cmd[i].cmd));
				resp.append(f_datd + to_string(arr_cmd[i].raw_payload.size()) + ":");
				resp.append(arr_cmd[i].raw_payload);
				resp.append(delim);
			}
			else if (arr_cmd[i].f_bool == false)
			{
				resp.append(f_cmd + to_string(arr_cmd[i].cmd));
				if (arr_cmd[i].need_resp)
				{
					// Используем знаковое или беззнаковое значение в зависимости от флага
					if (arr_cmd[i].use_signed)
					{
						resp.append(f_datd + to_string(arr_cmd[i].data_out_signed));
					}
					else
					{
						resp.append(f_datd + to_string(arr_cmd[i].data_out));
					}
				}
				else
				{
					resp.append(" " + arr_cmd[i].err);
				}
				resp.append(delim);
			}
			else
			{
				resp.append(arr_cmd[i].err);
			}
		}
	}
	else
	{
		resp.append(arr_cmd[0].err);
	}

	return resp;
}

bool isNumeric(std::string const &str)
{
	char *p;
	strtol(str.c_str(), &p, 10);
	return *p == 0;
}
