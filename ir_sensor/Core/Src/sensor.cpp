#include "sensor.h"
#include "utils.h"
//extern osSemaphoreId ADC_endHandle;
//extern ADC_HandleTypeDef hadc2;
//extern ADC_HandleTypeDef hadc1;
//extern uint16_t adc_buffer[1024];
extern osSemaphoreId setMutexHandle;

void sensor :: Init(settings_t *set, osSemaphoreId *ADC_endHandle, ADC_HandleTypeDef *hadc, uint16_t *adc_buffer, uint16_t depth)
{
	// проверка параметров
	if(set == nullptr) Error_Handler();
	if(ADC_endHandle == nullptr) Error_Handler();
	if(hadc == nullptr) Error_Handler();
	if(adc_buffer == nullptr) Error_Handler();

	p_settings = set;
	p_settings->sensorSett.sensorType = Optic;
	this->ADC_endHandle = ADC_endHandle;
	this->hadc = hadc;
	this->adc_buffer = adc_buffer;
	this->Depth = depth;

	// установка коэфицентов фильтра
	p_settings->sensorSett.k_H = 0.55f;
	p_settings->sensorSett.k_L = 0.15f;


}

// обробатываем накопленные данные
uint32_t sensor :: DataProcessing(uint16_t *data){

	if (!data || !p_settings) return 0;  // Проверка указателей

	float Output = 0;
	/* Sum */
	for (int var = 0; var < Depth; ++var) {
		//Output += *data;
		if((*data) >= 100){ //на входе фильтра отсекаем маленькие значенияя
			Output = ExpRunningAvgAdaptive(*data); // фильтруем
			*data = 0; // обнуляем входные данные
		}
		data++; // идем дальше
	}

	/**/
	result = (uint32_t) (Output);

	return result;
}

/*
	бегущее среднее с адаптивным коэффициентом
	Порог переключения (1.5) можете увеличить до 2-3 при высоком уровне шума

	k_H (0.1-0.9):
	Увеличить → быстрее отслеживание полезного сигнала, но больше пропускается шума
	Уменьшить → медленнее реакция на изменения, но лучше подавление шума

	k_L (0.01-0.3):
	Увеличить → меньше фильтрации при стабильном сигнале, быстрее адаптация к дрейфу
	Уменьшить → сильнее подавление шума, но медленнее адаптация к медленным изменениям

	значения (k_H=0.4, k_L=0.1) хорошо подходят для частоты 25 Гц.
	значения (k_H=0.55, k_L=0.15) хорошо подходят для частоты 35 Гц.
*/
float sensor :: ExpRunningAvgAdaptive(float newVal) { 
	float k_H, k_L;

	xSemaphoreTake(setMutexHandle, 100);
	k_H = p_settings->sensorSett.k_H;
	k_L = p_settings->sensorSett.k_L;
	xSemaphoreGive(setMutexHandle);

	// резкость фильтра зависит от модуля разности значений
    // Защита от переполнения и некорректных значений
    if (newVal < 0 || newVal > 65535) return filVal;
    
    if (abs(newVal - filVal) > 1.5) {
        filVal += (newVal - filVal) * k_H;
    } else {
        filVal += (newVal - filVal) * k_L;
    }

	//filVal += (newVal - filVal) * k;
	return filVal;
}

bool sensor :: DetectPoll(uint32_t tRising, uint32_t tFalling){

	if (!p_settings) return false;  // Проверка указателя
	uint32_t currentTime = HAL_GetTick();
	uint32_t tempTimeOutRising, tempTimeOutFalling;

	xSemaphoreTake(setMutexHandle, 100);
	// если таймауты не переданы в функцию или они нулевые выставляем из откалиброванных данных
	if(tRising == 0 && tFalling == 0){

		tempTimeOutRising = p_settings->sensorSett.timeParametrs.callTimeMax + p_settings->sensorSett.offsetTime;
		tempTimeOutFalling = p_settings->sensorSett.timeParametrs.timOutFalling;

	}else{
		tempTimeOutRising = tRising;
		tempTimeOutFalling = tFalling;
	}

	uint16_t threshold = (p_settings->sensorSett.callDistanceMin + p_settings->sensorSett.triger);
	xSemaphoreGive(setMutexHandle);

	// если у нас сработка по входным данным и низкий уровень по выходу
	if(result > threshold){

		if(detect == false){ // если сработки нету то проходим процедуру
			if(oldTimeRising == 0){
				oldTimeRising = currentTime;
			}

			timeRising = getTimeDiff(oldTimeRising, currentTime); // обновляем время на таймере переднего фронта

			// если время вышло значит переводим в 1 и сбрасываем таймера
			if(timeRising >= tempTimeOutRising){
				detect = true;
				oldTimeFalling = oldTimeRising = timeRising = timeFalling = 0;
			}

			// сбрасываем таймер заднего фронта
			if (oldTimeFalling != 0) {
                oldTimeFalling = 0;
            } 
		}
		else{ // иначе сработка есть и процедуру не проходим а только сбрасываем таймера
			// Сброс таймеров при активном detect
			oldTimeFalling = oldTimeRising = timeRising = timeFalling = 0;
		}
	}
	else{
		if(oldTimeRising != 0){ // если таймер переднего запущен то работаем с задним фронтом

			if(oldTimeFalling == 0){// если таймер заднего фронта в нуле значит нужно запустить таймер заднего
				oldTimeFalling = currentTime;
			}
			timeFalling = getTimeDiff(oldTimeFalling, currentTime); // обновляем время на таймере заднего фронта
			timeRising = getTimeDiff(oldTimeRising, currentTime);	// обновляем время на таймере переднего фронта

			// если время вышло значит переводим в 0 и сбрасываем таймера
            if (timeFalling >= tempTimeOutFalling || timeRising >= tempTimeOutRising) {
                detect = false;
                oldTimeFalling = oldTimeRising = timeRising = timeFalling = 0;
            }
		}
		else{

			detect = false;
            oldTimeFalling = oldTimeRising = timeRising = timeFalling = 0;
		}

	}
	//g_Detect = detect;
	return detect;
}

void sensor :: CallDistance(){
	calibrationInProgress = true;
	float peak = 0;
	float gorge = 10000;

	//добавить защиту при переходе времени через 0

	uint32_t old_time = HAL_GetTick();
	xSemaphoreTake(setMutexHandle, 100);
	uint32_t timeF1 = p_settings->sensorSett.timeCall;
	xSemaphoreGive(setMutexHandle);

	//uint32_t timeF2 = timeCall - timeF1;

	//uint32_t timePulse = 0;
	//uint32_t timePulseOld = 0;
	//uint32_t timePulseMin = 0;
	//uint32_t timePulseMax = 0;

	//bool detect = false;
	//bool detectoOld = false;

	do
	{
		HAL_ADC_Start_DMA(hadc, (uint32_t*)adc_buffer, Depth);
		osSemaphoreWait(*ADC_endHandle, osWaitForever);
		DataProcessing(adc_buffer); // оброботка данных

		if((HAL_GetTick() - old_time) >= 10) // ждем стабилизации и начинаем писать данные
		{
			if(result > 500){ //отсекаем маленькие значения
				if(result > peak){peak = result;}
				if(result < gorge){gorge = result;}
			}
		}
	}
	while(!(timeF1 <= (HAL_GetTick() - old_time)));

	//пишем настройки
	xSemaphoreTake(setMutexHandle, 100);
	p_settings->sensorSett.callDistanceMax = peak;
	p_settings->sensorSett.callDistanceMin = gorge;
	xSemaphoreGive(setMutexHandle);

	calibrationInProgress = false;
}

int sensor :: CallTime(){

	calibrationInProgress = true;

	//если канал не установлен ставим канал 1
	xSemaphoreTake(setMutexHandle, 100);
	uint32_t timeF2 = p_settings->sensorSett.timeCall;
	xSemaphoreGive(setMutexHandle);

	//добавить защиту при переходе времени через 0

	uint32_t old_time = HAL_GetTick();
	uint32_t timePulse = 0;
	uint32_t timePulseOld = 0;
	uint32_t timePulseMin = 0;
	uint32_t timePulseMax = 0;

	bool detect = false;
	static bool detectOld = false;
	timePulseOld = HAL_GetTick();

	do
	{
		HAL_ADC_Start_DMA(hadc, (uint32_t*)adc_buffer, Depth);
		osSemaphoreWait(*ADC_endHandle, osWaitForever);
		DataProcessing(adc_buffer); // оброботка данных

		detect = DetectPoll(1,1); // детектируем с минимальным временем

		if(detect != detectOld){
			if(detect){
				timePulseOld = HAL_GetTick();
			}else{
				timePulse = (HAL_GetTick() - timePulseOld);
				if(timePulse > timePulseMax){timePulseMax = timePulse;}
				if(timePulse < timePulseMin){timePulseMin = timePulse;}
				//timePulseOld = HAL_GetTick();
			}

		}

		detectOld = detect;
		osDelay(1);


	}
	while(!(timeF2 <= (HAL_GetTick() - old_time)));

	xSemaphoreTake(setMutexHandle, 100);
	p_settings->sensorSett.timeParametrs.callTimeMax = timePulseMax;
	p_settings->sensorSett.timeParametrs.callTimeMin = timePulseMin;
	xSemaphoreGive(setMutexHandle);

	calibrationInProgress = false;
	return 1;
}

// функция обще калибровки на вращвющейся ленте функция должна опрелелить минимальное и максимальное время прохождения ребра
// а также расстояние до ленты (между ребрами)
int sensor :: Calibration(){
	calibrationInProgress = true;
	
	// Структура для сбора статистики калибровки
	struct CalibrationData {
		float distancePeak = 0;              // Максимальное расстояние (между ребрами)
		float distanceGorge = 10000;         // Минимальное расстояние (на ребре)
		uint32_t timePulseMin = UINT32_MAX;  // Минимальное время прохождения ребра
		uint32_t timePulseMax = 0;           // Максимальное время прохождения ребра
		uint32_t pulseCount = 0;             // Количество обнаруженных ребер
		uint32_t validDistanceSamples = 0;   // Количество валидных измерений расстояния между ребрами
		float avgBeltDistance = 0;           // Среднее расстояние до ленты (только между ребрами)
		float avgPulseTime = 0;              // Среднее время импульса
	} calData;
	
	// Константы калибровки
	const uint32_t STABILIZATION_TIME = 100;    // Время стабилизации в мс
	const uint32_t MIN_PULSE_TIME = 5;          // Минимальное время импульса в мс
	const uint32_t MAX_PULSE_TIME = 5000;       // Максимальное время импульса в мс
	const uint16_t MIN_SIGNAL_THRESHOLD = 500;  // Минимальный порог сигнала
	const uint32_t MIN_PULSES_FOR_SUCCESS = 3;  // Минимум ребер для успешной калибровки
	
	xSemaphoreTake(setMutexHandle, 100);
	uint32_t calibrationTime = p_settings->sensorSett.timeCall;
	xSemaphoreGive(setMutexHandle);
	
	uint32_t startTime = HAL_GetTick();
	uint32_t timePulse = 0;
	uint32_t timePulseStart = 0;
	
	bool detect = false;
	bool detectOld = false;
	bool firstDetection = true;
	
	// Переменные для двухэтапной калибровки
	enum CalibrationPhase {
		PHASE_TIME_CALIBRATION,     // Этап 1: калибровка времени
		PHASE_DISTANCE_CALIBRATION  // Этап 2: калибровка расстояния между ребрами
	} currentPhase = PHASE_TIME_CALIBRATION;
	
	uint32_t phaseStartTime = startTime;
	uint32_t timeCalibrationDuration = calibrationTime / 2; // Половина времени на калибровку времени
	
	// Переменные для измерения расстояния между ребрами
	uint32_t lastEdgeEndTime = 0;
	uint32_t nextEdgeStartTime = 0;
	bool measuringBetweenEdges = false;
	float beltDistanceSum = 0;
	uint32_t beltDistanceSamples = 0;
	
	STM_LOG("Calibration started for %d ms", (int)calibrationTime);
	STM_LOG("Phase 1: Time calibration (%d ms)", (int)timeCalibrationDuration);
	
	do {
		// Получение данных с датчика
		HAL_ADC_Start_DMA(hadc, (uint32_t*)adc_buffer, Depth);
		osSemaphoreWait(*ADC_endHandle, osWaitForever);
		DataProcessing(adc_buffer);
		
		uint32_t elapsed = getTimeDiff(startTime, HAL_GetTick());
		uint32_t phaseElapsed = getTimeDiff(phaseStartTime, HAL_GetTick());
		
		// Ждем стабилизации перед началом измерений
		if (elapsed >= STABILIZATION_TIME) {
			
			// === ЭТАП 1: КАЛИБРОВКА ВРЕМЕНИ ===
			if (currentPhase == PHASE_TIME_CALIBRATION) {
				
				// Детекция с минимальными таймаутами для точного измерения времени
				detect = DetectPoll(1, 1);
				
				// Поиск общего диапазона сигнала (грубая оценка)
				if (result > MIN_SIGNAL_THRESHOLD) {
					if (result > calData.distancePeak) calData.distancePeak = result;
					if (result < calData.distanceGorge) calData.distanceGorge = result;
				}
				
				// Обнаружение фронтов для измерения времени импульсов
				if (detect != detectOld) {
					if (detect) {
						// Передний фронт - начало ребра
						timePulseStart = HAL_GetTick();
						if (firstDetection) {
							firstDetection = false;
							STM_LOG("First edge detected");
						}
					} else {
						// Задний фронт - конец ребра
						if (timePulseStart != 0) {
							timePulse = getTimeDiff(timePulseStart, HAL_GetTick());
							
							// Фильтрация разумных значений времени
							if (timePulse >= MIN_PULSE_TIME && timePulse <= MAX_PULSE_TIME) {
								// Обновление минимума и максимума времени
								if (timePulse > calData.timePulseMax) {
									calData.timePulseMax = timePulse;
								}
								if (timePulse < calData.timePulseMin) {
									calData.timePulseMin = timePulse;
								}
								
								// Расчет среднего времени импульса
								calData.avgPulseTime = (calData.avgPulseTime * calData.pulseCount + timePulse) / (calData.pulseCount + 1);
								calData.pulseCount++;
								
								STM_LOG("Edge #%d: time=%d ms", (int)calData.pulseCount, (int)timePulse);
								
								// Сохраняем время окончания ребра для следующего этапа
								lastEdgeEndTime = HAL_GetTick();
							}
							timePulseStart = 0;
						}
					}
				}
				detectOld = detect;
				
				// Переход ко второму этапу
				if (phaseElapsed >= timeCalibrationDuration && calData.pulseCount >= MIN_PULSES_FOR_SUCCESS) {
					currentPhase = PHASE_DISTANCE_CALIBRATION;
					phaseStartTime = HAL_GetTick();
					
					// Устанавливаем предварительные параметры времени для точной детекции
					xSemaphoreTake(setMutexHandle, 100);
					p_settings->sensorSett.timeParametrs.callTimeMin = calData.timePulseMin;
					p_settings->sensorSett.timeParametrs.callTimeMax = calData.timePulseMax;
					p_settings->sensorSett.timeParametrs.timOutFalling = calData.timePulseMax * 2;
					p_settings->sensorSett.offsetTime = calData.timePulseMin / 2;
					
					// Устанавливаем предварительный порог (между min и max)
					uint16_t tempThreshold = calData.distanceGorge + (calData.distancePeak - calData.distanceGorge) * 0.4f;
					p_settings->sensorSett.callDistanceMin = calData.distanceGorge;
					p_settings->sensorSett.triger = tempThreshold;
					xSemaphoreGive(setMutexHandle);
					
					STM_LOG("Phase 2: Distance calibration between edges");
					STM_LOG("Temp threshold set to: %d", tempThreshold);
				}
			}
			
			// === ЭТАП 2: КАЛИБРОВКА РАССТОЯНИЯ МЕЖДУ РЕБРАМИ ===
			else if (currentPhase == PHASE_DISTANCE_CALIBRATION) {
				
				// Используем откалиброванные параметры времени для точной детекции
				detect = DetectPoll();
				
				// Обнаружение фронтов
				if (detect != detectOld) {
					if (detect) {
						// Передний фронт - начало нового ребра
						nextEdgeStartTime = HAL_GetTick();
						measuringBetweenEdges = false;
						
						STM_LOG("Edge start detected");
					} else {
						// Задний фронт - конец ребра, начинаем измерение между ребрами
						lastEdgeEndTime = HAL_GetTick();
						measuringBetweenEdges = true;
						beltDistanceSum = 0;
						beltDistanceSamples = 0;
						
						STM_LOG("Edge end - start measuring belt distance");
					}
				}
				
				// Измеряем расстояние только между ребрами
				if (measuringBetweenEdges && !detect && result > MIN_SIGNAL_THRESHOLD) {
					beltDistanceSum += result;
					beltDistanceSamples++;
					calData.validDistanceSamples++;
				}
				
				// Если обнаружили следующее ребро, завершаем измерение участка ленты
				if (measuringBetweenEdges && detect && beltDistanceSamples > 0) {
					float avgSegmentDistance = beltDistanceSum / beltDistanceSamples;
					
					// Обновляем общее среднее расстояние до ленты (алгоритм Welford)
					uint32_t totalSegments = (calData.validDistanceSamples > 0) ? (calData.validDistanceSamples / beltDistanceSamples) : 1;
					float delta = avgSegmentDistance - calData.avgBeltDistance;
					calData.avgBeltDistance += delta / totalSegments;
					
					STM_LOG("Belt segment: avg=%.1f, samples=%d", avgSegmentDistance, (int)beltDistanceSamples);
					
					measuringBetweenEdges = false;
				}
				
				detectOld = detect;
			}
		}
		
		osDelay(1); // Небольшая задержка для стабильности
		
	} while (getTimeDiff(startTime, HAL_GetTick()) < calibrationTime);
	
	// === АНАЛИЗ И СОХРАНЕНИЕ РЕЗУЛЬТАТОВ ===
	
	// Проверка успешности калибровки
	bool calibrationSuccess = (calData.pulseCount >= MIN_PULSES_FOR_SUCCESS) && 
							  (calData.validDistanceSamples > 50) &&
							  (calData.avgBeltDistance > 0) &&
							  (calData.distancePeak > calData.distanceGorge);
	
	if (calibrationSuccess) {
		xSemaphoreTake(setMutexHandle, 100);
		
		// === СОХРАНЕНИЕ ПАРАМЕТРОВ РАССТОЯНИЯ ===
		p_settings->sensorSett.callDistanceMax = calData.avgBeltDistance + 200; // Небольшой запас
		p_settings->sensorSett.callDistanceMin = calData.distanceGorge;
		
		// Оптимальный порог: между ребром и лентой
		float optimalThreshold = calData.distanceGorge + (calData.avgBeltDistance - calData.distanceGorge) * 0.5f;
		p_settings->sensorSett.triger = (uint16_t)optimalThreshold;
		
		// === ПАРАМЕТРЫ ВРЕМЕНИ УЖЕ УСТАНОВЛЕНЫ НА ЭТАПЕ 1 ===
		
		xSemaphoreGive(setMutexHandle);
		
		// Логирование результатов
		STM_LOG("Calibration SUCCESS:");
		STM_LOG("  Belt distance: avg=%.1f, threshold=%d", calData.avgBeltDistance, (int)optimalThreshold);
		STM_LOG("  Edge distance: min=%d", (int)calData.distanceGorge);
		STM_LOG("  Time: min=%d ms, max=%d ms, avg=%.1f ms", 
				(int)calData.timePulseMin, (int)calData.timePulseMax, calData.avgPulseTime);
		STM_LOG("  Edges detected: %d, Belt samples: %d", 
				(int)calData.pulseCount, (int)calData.validDistanceSamples);
	} else {
		STM_LOG("Calibration FAILED:");
		STM_LOG("  Edges detected: %d (min %d required)", 
				(int)calData.pulseCount, (int)MIN_PULSES_FOR_SUCCESS);
		STM_LOG("  Belt samples: %d (min 50 required)", (int)calData.validDistanceSamples);
		STM_LOG("  Belt distance valid: %s", (calData.avgBeltDistance > 0) ? "YES" : "NO");
		STM_LOG("  Distance range valid: %s", 
				(calData.distancePeak > calData.distanceGorge) ? "YES" : "NO");
	}
	
	calibrationInProgress = false;
	
	// Возвращаем количество обнаруженных ребер (0 = ошибка)
	return calibrationSuccess ? calData.pulseCount : 0;
}

bool sensor :: Getdetect(){

	return detect;
}


uint16_t sensor :: GetResult(){
	uint16_t Ret = 0;
	switch (p_settings->sensorSett.sensorType) {
	case Optic: // оптика
		Ret = result;
		break;
	case Ultrasound: // ултразвук

		break;
	default:

		break;
	}

	//защитить результат мютексом
	return Ret;

}

// пока не реализованна, нет необходимости
uint32_t sensor :: GetDistance_mm(){
	// вычисляем расстояние в мм для GP2Y0A02YK0F
	return 0; // K_CALIBRATION / (result - OFFSET);
}

void sensor :: StartCalibration(){
	if(!calibrationInProgress){
		calibrationInProgress = true;
	}
}

void sensor :: SetOffsetMin(uint16_t offset){
	xSemaphoreTake(setMutexHandle, 100);
	p_settings->sensorSett.callDistanceMin = offset;
	xSemaphoreGive(setMutexHandle);
}

void sensor :: SetTrigger(uint16_t offset){
	xSemaphoreTake(setMutexHandle, 100);
	p_settings->sensorSett.triger = offset;
	xSemaphoreGive(setMutexHandle);
}

void sensor :: SetOffsetMax(uint16_t offset){
	xSemaphoreTake(setMutexHandle, 100);
	p_settings->sensorSett.callDistanceMax = offset;
	xSemaphoreGive(setMutexHandle);
}

void sensor :: SetTimeCall(uint32_t time){
	xSemaphoreTake(setMutexHandle, 100);
	p_settings->sensorSett.timeCall = time;
	xSemaphoreGive(setMutexHandle);
}

bool sensor :: GetCalibrationStatus(){
	return calibrationInProgress;
}

uint16_t sensor :: GetOffsetMin(){
	xSemaphoreTake(setMutexHandle, 100);
	uint16_t ret = p_settings->sensorSett.callDistanceMin;
	xSemaphoreGive(setMutexHandle);
	return ret;
}

uint16_t sensor :: GetOffsetMax(){
	xSemaphoreTake(setMutexHandle, 100);
	uint16_t ret = p_settings->sensorSett.callDistanceMax;
	xSemaphoreGive(setMutexHandle);
	return ret;
}

//for hcsr04

uint16_t sensor::GetTrigger(){
	xSemaphoreTake(setMutexHandle, 100);
	uint16_t ret = p_settings->sensorSett.triger;
	xSemaphoreGive(setMutexHandle);
	return ret;
}

bool sensor::StatusCalibration() {
	return calibrationInProgress;
}

void sensor::SetOffsetTime(uint32_t time) {
	p_settings->sensorSett.offsetTime = time;
}

uint32_t sensor::GetOffsetTime() {
	return p_settings->sensorSett.offsetTime;
}

uint32_t sensor::GetTimeoutRasing() {
	uint16_t ret = 0;
	xSemaphoreTake(setMutexHandle, 100);
	ret = p_settings->sensorSett.timeParametrs.callTimeMax;
	xSemaphoreGive(setMutexHandle);

	return ret;
}

void sensor::SetTimeoutRasing(uint32_t time) {

	xSemaphoreTake(setMutexHandle, 100);
	p_settings->sensorSett.timeParametrs.callTimeMax = time;
	xSemaphoreGive(setMutexHandle);
}

void sensor::SetSensorType(SensorType type) {
	xSemaphoreTake(setMutexHandle, 100);
	p_settings->sensorSett.sensorType = type;
	xSemaphoreGive(setMutexHandle);
}

SensorType sensor::GetSensorType() {
	xSemaphoreTake(setMutexHandle, 100);
	SensorType type = p_settings->sensorSett.sensorType;
	xSemaphoreGive(setMutexHandle);
	return type;
}
