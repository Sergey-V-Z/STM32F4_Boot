/*
 * ПРИМЕР КОДА ДЛЯ ДОБАВЛЕНИЯ В ПРОШИВКУ 6ch_LED_Driver
 * Обработчик JSON команды калибровки тока ACS712
 * 
 * Этот код нужно добавить в UART обработчик прошивки
 */

// В файле где обрабатываются JSON команды через UART
// (возможно в freertos.cpp в задаче uart_Task или подобной)

/*
 Пример обработчика JSON команды:
 
 Входящая команда:
 {
   "cmd": "calibrate_current",
   "mode": 0
 }
 
 mode:
   0 - выполнить калибровку нуля
   1 - прочитать текущее смещение  
   2 - сбросить калибровку
*/

// Добавить в обработчик JSON команд:
void handle_calibrate_current_command(cJSON *json)
{
    cJSON *mode_json = cJSON_GetObjectItem(json, "mode");
    int mode = (mode_json != NULL) ? mode_json->valueint : 0;
    
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "cmd", "calibrate_current");
    
    if (mode == 0)
    {
        // Выполнить калибровку нуля
        ACS712_CalibrateZero();
        
        // Вернуть новое смещение в миллиамперах
        int32_t offset_ma = (int32_t)(g_current_zero_offset * 1000.0f);
        cJSON_AddStringToObject(response, "status", "OK");
        cJSON_AddNumberToObject(response, "offset_ma", offset_ma);
    }
    else if (mode == 1)
    {
        // Прочитать текущее смещение
        int32_t offset_ma = (int32_t)(g_current_zero_offset * 1000.0f);
        cJSON_AddStringToObject(response, "status", "OK");
        cJSON_AddNumberToObject(response, "offset_ma", offset_ma);
    }
    else if (mode == 2)
    {
        // Сбросить калибровку
        g_current_zero_offset = 0.0f;
        Calibration_Save();  // Сохранить сброшенное значение во flash
        cJSON_AddStringToObject(response, "status", "OK");
        cJSON_AddNumberToObject(response, "offset_ma", 0);
    }
    else
    {
        cJSON_AddStringToObject(response, "status", "ERROR");
        cJSON_AddStringToObject(response, "message", "Invalid mode");
    }
    
    // Отправить ответ через UART
    char *json_string = cJSON_PrintUnformatted(response);
    if (json_string != NULL)
    {
        // Здесь отправить json_string через UART
        // Например: HAL_UART_Transmit(&huart2, (uint8_t*)json_string, strlen(json_string), 1000);
        
        cJSON_free(json_string);
    }
    
    cJSON_Delete(response);
}

/*
 В основном обработчике JSON команд добавить:
 
 cJSON *json = cJSON_Parse(received_json_string);
 if (json != NULL)
 {
     cJSON *cmd = cJSON_GetObjectItem(json, "cmd");
     if (cmd != NULL && cmd->valuestring != NULL)
     {
         if (strcmp(cmd->valuestring, "calibrate_current") == 0)
         {
             handle_calibrate_current_command(json);
         }
         // ... другие команды ...
     }
     cJSON_Delete(json);
 }
*/

/*
 Необходимые объявления глобальных переменных (должны быть доступны):
 
 extern float g_current_amperes;  // Текущий ток в амперах
 extern float g_current_zero_offset;  // Калибровочное смещение нуля
 extern void ACS712_CalibrateZero(void);  // Функция калибровки
 extern void Calibration_Save(void);  // Функция сохранения калибровки во flash
*/
