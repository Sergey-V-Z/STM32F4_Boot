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

/*variables ---------------------------------------------------------*/
extern settings_t settings;
extern flash mem_spi;

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
	string err;			 // сообщение клиенту об ошибке в сообщении
	bool f_bool = false; // наличие ошибки в сообшении
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
			// Chanel on/off
			case 4:
			{
				// mode set all channel
				if (arr_cmd[i].addres_var >= 1)
				{
					if (arr_cmd[i].data_in) {
						PWM_EnableAll();
					} else {
						PWM_DisableAll();
					}
					arr_cmd[i].err = "OK";
				}
				else
				{
					// mode set one channel
					uint32_t ch = arr_cmd[i].data_in1;
					if (ch < PWM_CH_COUNT)
					{
						if (arr_cmd[i].data_in) {
							PWM_Enable((PWM_Channel_t)ch);
						} else {
							PWM_Disable((PWM_Channel_t)ch);
						}
						arr_cmd[i].err = "OK";
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
			// Current read from channel (not available for direct PWM control)
			case 7:
			{
				arr_cmd[i].data_out = 0;
				arr_cmd[i].need_resp = true;
				arr_cmd[i].err = "Not available";
				break;
			}
			// is on
			case 8:
			{
				uint32_t ch = arr_cmd[i].data_in1;
				if (ch < PWM_CH_COUNT)
				{
					arr_cmd[i].data_out = PWM_IsEnabled((PWM_Channel_t)ch) ? 1 : 0;
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

			if (arr_cmd[i].f_bool == false)
			{
				resp.append(f_cmd + to_string(arr_cmd[i].cmd));
				if (arr_cmd[i].need_resp)
				{
					resp.append(f_datd + to_string(arr_cmd[i].data_out));
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
