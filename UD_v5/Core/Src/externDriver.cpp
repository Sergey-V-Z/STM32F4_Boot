#include <externDriver.hpp>
#include "stdio.h"
#include <cmath>
#include <algorithm>

/***************************************************************************
 * Класс для шагового двухфазного мотора
 *
 * В этом классе реализован цикл управления и контроля шагового двигателя
 ****************************************************************************/

void extern_driver::Init() {
    currentDriverStatus = DRIVER_STATUS_UNKNOWN;
    lastDriverCheckTime = 0;
    driverErrorPort = DRIVER_ERR_GPIO_Port;
    driverErrorPin = DRIVER_ERR_Pin;

    // Инициализация параметров для таймера
    feedbackPosition = 0;
    isLastSegment = true;

 	switch (settings->mod_rotation) {
		case step_by_meter_enc_intermediate:
		case step_by_meter_timer_intermediate:
		case step_inf:
		{
            TimFrequencies->Instance->PSC = 399;
            MaxSpeed = 50;
            MinSpeed = 13000;
			break;
		}
		case bldc_limit:
    	case bldc_inf:
		{
            TimFrequencies->Instance->PSC = 200-1;
            MaxSpeed = 100;
            MinSpeed = 2666;
			break;
		}
		default:
		{
            TimFrequencies->Instance->PSC = 399;
            MaxSpeed = 50;
            MinSpeed = 13000;
			break;
		}
 	}


    // Рассчитываем частоту тактирования таймера
    calculateTimerFrequency();

    TimFrequencies->Instance->ARR = MinSpeed;
    //Speed_Call = (uint16_t) map(950, 1, 1000, MinSpeed, MaxSpeed);
    Speed_Call = (uint16_t) map_ARRFromPercent(950);

    // Инициализация переменных для работы с таблицами
    rampTables.accelSteps = 0;
    rampTables.decelSteps = 0;
    rampTables.currentStep = 0;

    // Рассчитываем количество шагов, необходимых для торможения
    uint32_t brakingSteps = calculateBrakingSteps();
    uint32_t accelSteps = calculateAccelSteps();
    // Пересчитываем таблицы разгона и торможения
    calculateAccelTable(accelSteps);
    calculateDecelTable(brakingSteps);

    Status = statusMotor::STOPPED;
    FeedbackType = fb::ENCODER;
    StatusTarget = statusTarget_t::finished;

    if (settings->Direct == dir::CW) {
        DIRECT_CW
    } else if (settings->Direct == dir::CCW) {
        DIRECT_CCW
    }

    // Инициализация таймера энкодера
    __HAL_TIM_SET_COUNTER(TimEncoder, FEEDBACK_MID_VALUE);
    HAL_TIM_Encoder_Start(TimEncoder, TIM_CHANNEL_ALL);
    HAL_TIM_OC_Start_IT(TimEncoder, TIM_CHANNEL_3);
    HAL_TIM_OC_Start_IT(TimEncoder, TIM_CHANNEL_4);
    HAL_TIM_Base_Start_IT(TimEncoder);

    // Инициализация таймера шагов — среднее значение
    __HAL_TIM_SET_COUNTER(TimCountAllSteps, FEEDBACK_MID_VALUE);
    HAL_TIM_OC_Start_IT(TimCountAllSteps, TIM_CHANNEL_1);
    HAL_TIM_OC_Start_IT(TimCountAllSteps, TIM_CHANNEL_2);

    HAL_GPIO_WritePin(EN_GPIO_Port, EN_Pin, GPIO_PIN_SET); // Выключение драйвера

    checkDriverStatus();
    Parameter_update();
}

driver_status_t extern_driver::getDriverStatus() {
    if (!isDriverStatusValid()) {
        checkDriverStatus();
    }
    return currentDriverStatus;
}

uint32_t extern_driver::getLastDriverCheck() {
    return lastDriverCheckTime;
}

void extern_driver::checkDriverStatus() {
    GPIO_PinState errorState = HAL_GPIO_ReadPin(driverErrorPort, driverErrorPin);
    lastDriverCheckTime = HAL_GetTick();

    if (errorState == GPIO_PIN_SET) {
        currentDriverStatus = DRIVER_ERROR;
        if (Status != statusMotor::ERROR_M) {
            stop(statusTarget_t::errDriver);
            Status = statusMotor::ERROR_M;
        }
    } else {
        currentDriverStatus = DRIVER_OK;
    }
}

bool extern_driver::isDriverStatusValid() {
    return (HAL_GetTick() - lastDriverCheckTime) < DRIVER_STATUS_VALIDITY_TIME;
}

//methods for aktion*********************************************

//
bool extern_driver::start(uint32_t steps, dir d) {

	dir temp_diretion = settings->Direct;
    if (Status == statusMotor::STOPPED) {

        // Проверяем текущее состояние датчиков
        bool on_D0 = HAL_GPIO_ReadPin(D0_GPIO_Port, D0_Pin) == GPIO_PIN_SET;
        bool on_D1 = HAL_GPIO_ReadPin(D1_GPIO_Port, D1_Pin) == GPIO_PIN_SET;

        // Проверяем состояние драйвера перед стартом
        checkDriverStatus();
        if (currentDriverStatus != DRIVER_OK) {
            STM_LOG("Cannot start: driver error detected");
            return false;
        }

     	// Направление
     	if(d ==  END_OF_LIST)
     	{
     		temp_diretion = settings->Direct;
     	} else {
     		temp_diretion = d;
     	}
        // проверка по концевикам выставление направления
     	switch (settings->mod_rotation) {
			case step_by_meter_enc_intermediate:
			case step_by_meter_timer_intermediate:
			case calibration_timer:
			case calibration_enc:
			{
				if (temp_diretion == dir::CW && on_D0) {
					STM_LOG("Cannot move CW: at CW limit switch");
					return false;
				}
				else if (temp_diretion == dir::CCW && on_D1) {
					STM_LOG("Cannot move CCW: at CCW limit switch");
					return false;
				}

				if (temp_diretion == dir::CCW) {
					DIRECT_CCW
					ChangeTimerMode(TimCountAllSteps, TIM_COUNTERMODE_UP); // от 0 датчика к 1 CCW
				} else {
					DIRECT_CW
					ChangeTimerMode(TimCountAllSteps, TIM_COUNTERMODE_DOWN); // от 1 датчика к 0 CW
				}
				break;
			}
    		case step_inf:
    		case bldc_inf:
    		{
    			// добавить отключение таймеров обратной связи

    			if (temp_diretion == dir::CCW) {
    				DIRECT_CCW
    			} else {
    				DIRECT_CW
    			}
    			break;
    		}
    		case bldc_limit:
    		{
    			// добавить отключение таймеров обратной связи

    			if (temp_diretion == dir::CW && on_D0) {
    				STM_LOG("Cannot move CW: at CW limit switch");
    				return false;
    			}
    			else if (temp_diretion == dir::CCW && on_D1) {
    				STM_LOG("Cannot move CCW: at CCW limit switch");
    				return false;
    			}
    			if (temp_diretion == dir::CCW) {
    				DIRECT_CCW
    			} else {
    				DIRECT_CW
    			}
    			break;
    		}
    		default:
    		{
    			break;
    		}
        }

        PrevCounterENC = TimEncoder->Instance->CNT;
        countErrDir = 3;
        StatusTarget = statusTarget_t::inProgress;

        //uint32_t SlowdownDistance = 50; //steps/10; //1%

        // Рассчитываем количество шагов, необходимых для торможения
        uint32_t brakingSteps = calculateBrakingSteps();
        uint32_t accelSteps = calculateAccelSteps();
        // Пересчитываем таблицы разгона и торможения
        calculateAccelTable(accelSteps);
        calculateDecelTable(brakingSteps);

        // Сбрасываем счетчик шагов таблицы
        rampTables.currentStep = 0;
        //rampTables.isAccelerating = true;
        //rampTables.isDecelerating = false;

        switch (settings->mod_rotation) {
        	case calibration_enc:
        	case step_by_meter_enc_intermediate:
        	case calibration_timer:
        	case step_by_meter_timer_intermediate:
        	{
                SET_TIM_ARR_AND_PULSE(TimFrequencies, ChannelClock, rampTables.accelTable[0]);
                feedbackSetup(steps, temp_diretion);
                Status = statusMotor::ACCEL;
                break;
        	}

        	case bldc_limit:
        	case bldc_inf:
        	{
        		//__HAL_TIM_SET_AUTORELOAD(TimFrequencies, settings->Speed);
        		SET_TIM_ARR_AND_PULSE(TimFrequencies, ChannelClock, settings->Speed);
        		Status = statusMotor::MOTION;
        		break;
        	}
        	case step_inf:
        	{
        		SET_TIM_ARR_AND_PULSE(TimFrequencies, ChannelClock, rampTables.accelTable[0]);

        		Status = statusMotor::ACCEL;
        		break;
        	}
        	default:
        	{
        		break;
        	}
        }

        //STM_LOG("Speed start: %d", (int)(TimFrequencies->Instance->ARR));
        STM_LOG("Start motor.");

     	// Игнорируем ВСЕ активные концевики при старте (независимо друг от друга).
     	// Если оба активны одновременно — устанавливаем оба ignore-флага.
     	if(on_D0) StartDebounceTimer(D0_Pin);
     	if(on_D1) StartDebounceTimer(D1_Pin);

        HAL_GPIO_WritePin(EN_GPIO_Port, EN_Pin, GPIO_PIN_RESET); // Включение драйвера
        HAL_TIM_OC_Start_IT(TimFrequencies, ChannelClock);

        return true;
    } else {
        STM_LOG("Fail started motor.");
        return false;
    }
}

bool extern_driver::startForCall(dir d) {
	STM_LOG("Start for call. status: %d, dir: %s", (int)Status, d == dir::CW ? "CW" : "CCW");

    if (Status == statusMotor::STOPPED) {

        bool on_D0 = HAL_GPIO_ReadPin(D0_GPIO_Port, D0_Pin) == GPIO_PIN_SET;
        bool on_D1 = HAL_GPIO_ReadPin(D1_GPIO_Port, D1_Pin) == GPIO_PIN_SET;

        rampTables.currentStep = 0;
    	settings->Direct = d;

        checkDriverStatus();
        if (currentDriverStatus != DRIVER_OK) {
            STM_LOG("Cannot start: driver error detected");
            return false;
        }

		// startForCall вызывается ТОЛЬКО из калибровки, где мы явно движемся к концевику.
		if (settings->Direct == dir::CCW) {
			DIRECT_CCW
		} else {
			DIRECT_CW
		}

        StatusTarget = statusTarget_t::inProgress;

        uint32_t brakingSteps = calculateBrakingSteps();
        uint32_t accelSteps = calculateAccelSteps();
        calculateAccelTable(accelSteps);
        calculateDecelTable(brakingSteps);

		SET_TIM_ARR_AND_PULSE(TimFrequencies, ChannelClock, rampTables.accelTable[0]);

        // Сброс feedback-счётчика, OC вынести за пределы досягаемости (бесконечное движение)
        TIM_HandleTypeDef* fbTim = getFeedbackTimer();
        uint32_t chBrake = getFeedbackCH_brake();
        uint32_t chStop  = getFeedbackCH_stop();

        resetFeedbackCounter();
        feedbackPosition = 0;
        __HAL_TIM_SET_AUTORELOAD(fbTim, 0xffff);
        // OC за пределами досягаемости — мотор едет до физического EXTI концевика.
        // CCW: счётчик растёт от MID вверх → OC=0 ниже MID, никогда не достигнет.
        // CW:  счётчик падает от MID вниз → OC=0xFFFF выше MID, никогда не достигнет.
        if (settings->Direct == dir::CCW) {
            __HAL_TIM_SET_COMPARE(fbTim, chBrake, 0);
            __HAL_TIM_SET_COMPARE(fbTim, chStop,  0);
        } else {
            __HAL_TIM_SET_COMPARE(fbTim, chBrake, 0xFFFF);
            __HAL_TIM_SET_COMPARE(fbTim, chStop,  0xFFFF);
        }
        HAL_TIM_OC_Start_IT(fbTim, chBrake);
        HAL_TIM_OC_Start_IT(fbTim, chStop);

        Status = statusMotor::ACCEL;
        if (!isEncoderMode()) {
            // Направление счёта TIM4 должно совпадать с направлением движения.
            if (settings->Direct == dir::CCW) {
                ChangeTimerMode(TimCountAllSteps, TIM_COUNTERMODE_UP);
            } else {
                ChangeTimerMode(TimCountAllSteps, TIM_COUNTERMODE_DOWN);
            }
        }

     	if (on_D0) StartDebounceTimer(D0_Pin);
     	if (on_D1) StartDebounceTimer(D1_Pin);

        HAL_GPIO_WritePin(EN_GPIO_Port, EN_Pin, GPIO_PIN_RESET);
        HAL_TIM_OC_Start_IT(TimFrequencies, ChannelClock);

        return true;
    } else {
        STM_LOG("Fail started motor. motion or mode");
        return false;
    }
}

void extern_driver::stop(statusTarget_t status) {
    // Фиксируем незавершённый сегмент обратной связи ПЕРЕД остановкой.
    // Работает при любой причине стопа: OC (последний сегмент), EXTI (концевик), авария.
    // Защита через Status: повторный вызов stop() (например EXTI после OC) ничего не делает.
    if (Status != statusMotor::STOPPED) {
        switch (settings->mod_rotation) {
            case step_by_meter_enc_intermediate:
            case step_by_meter_timer_intermediate:
            case calibration_enc:
            case calibration_timer:
            {
                uint16_t cnt = __HAL_TIM_GET_COUNTER(getFeedbackTimer());
                feedbackPosition += (int32_t)cnt - (int32_t)FEEDBACK_MID_VALUE;
                resetFeedbackCounter();
                break;
            }
            default: break;
        }
    }

    ignore_D0 = false;
    ignore_D1 = false;

    HAL_TIM_OC_Stop_IT(TimFrequencies, ChannelClock);
    HAL_GPIO_WritePin(EN_GPIO_Port, EN_Pin, GPIO_PIN_SET); // Выключение драйвера
    //permission_calibrate = false;
    //permission_findHome = false;

    switch (status) {
        case statusTarget_t::finished:
            break;
        case statusTarget_t::errMotion:
            break;
        case statusTarget_t::errDirection:
            break;
        case statusTarget_t::errDriver:
            STM_LOG("Motor stopped: driver error detected");
            break;
        default:
            break;
    }

    StatusTarget = status;
    TimerIsStart = false;
    Time = 0;
/*
    switch (settings->mod_rotation) {
    	case by_meter_timer_intermediate:
		case by_meter_timer_limit_switch:
		case by_meter_timer:
		{

		    if (settings->Direct == dir::CCW) {
		            LastDistance = motionSteps + TimCountAllSteps->Instance->CNT;
		            motionSteps = 0;
		    } else {
		            LastDistance = motionSteps + TimCountAllSteps->Instance->CNT;
		            motionSteps = 0;
		    }
			break;
		}
		case infinity_enc:
        case by_meter_enc_intermediate:
        case by_meter_enc_limit_switch:
		case by_meter_enc:
		{

		    if (settings->Direct == dir::CCW) {
		            LastDistance = TimEncoder->Instance->CNT;
		    } else {
		            LastDistance = 0xffff - TimEncoder->Instance->CNT;
		    }
			break;
		}
		default:
		{
			break;
		}
    }
    */
    Status = statusMotor::STOPPED;
}

/**
 * Функция для начала торможения двигателя.
 * Инициирует процесс торможения в зависимости от режима работы.
 */
// Модификация метода slowdown() для использования таблицы торможения
void extern_driver::slowdown() {
    // Если двигатель уже тормозит или остановлен, ничего не делаем
    if (Status == statusMotor::BRAKING || Status == statusMotor::STOPPED) return;

    // Сбрасываем счетчик шагов таблицы
    rampTables.currentStep = 0;
    //rampTables.isAccelerating = false;
    //rampTables.isDecelerating = true;

    // Устанавливаем статус торможения
    Status = statusMotor::BRAKING;

    STM_LOG("Motor slowdown with deceleration table (%u steps)", rampTables.decelSteps);
}

void extern_driver::removeBreak(bool status) {
	if (status) {
		//      HAL_GPIO_WritePin(RELE1_GPIO_Port, RELE1_Pin, GPIO_PIN_SET);
	} else {
		//      HAL_GPIO_WritePin(RELE1_GPIO_Port, RELE1_Pin, GPIO_PIN_RESET);
	}
}

//handlers*******************************************************

void extern_driver::SensHandler(uint16_t GPIO_Pin) {

    // Если включено игнорирование датчиков, проверяем не истек ли таймаут
    /*
	if (ignore_sensors) {
        if ((HAL_GetTick() - vibration_start_time) < START_VIBRATION_TIMEOUT) {
            // Игнорируем прерывание
            return;
        } else {
            // Таймаут истек, выключаем игнорирование
            ignore_sensors = false;
            position = pos_t::D_0_1;
        }
    }*/

	// Раздельное игнорирование: каждый флаг блокирует только СВОЙ концевик.
	// D0 активен — игнорируем если только что стартовали с D0 (отъезжаем от него).
	// D1 активен — игнорируем если только что стартовали с D1 (отъезжаем от него).
	if (GPIO_Pin == D0_Pin && ignore_D0) return;
	if (GPIO_Pin == D1_Pin && ignore_D1) return;

    stop(statusTarget_t::finished);

	if (GPIO_Pin == D0_Pin) {
		position = pos_t::D0;

        switch (settings->mod_rotation) {

        	case calibration_timer:
        	case calibration_enc:
        	{
        		break;
        	}
        	case step_by_meter_enc_intermediate:
        	case step_by_meter_timer_intermediate:
        	case bldc_limit:
        	case step_inf:
        	case bldc_inf:
        	{
                // D0 — физический ноль. stop() уже зафиксировал сегмент и сбросил счётчик.
                // Просто устанавливаем позицию в 0: это физическая истина.
                feedbackPosition = 0;
                settings->points.current_point = 0;
                STM_LOG("D0 hit: position reset to 0");
        		break;
        	}
        	default:
        	{
        		break;
        	}
        }
	}
	else if (GPIO_Pin == D1_Pin){
		position = pos_t::D1;

        switch (settings->mod_rotation) {

        	case calibration_timer:
        	case calibration_enc:
        	{
        		break;
        	}
        	case step_by_meter_enc_intermediate:
        	case step_by_meter_timer_intermediate:
        	{
                // Позиция уже точно зафиксирована в stop() выше: feedbackPosition += (cnt - MID).
                // Никакого дополнительного вычисления не нужно.
                STM_LOG("D1 hit: pos=%ld", feedbackPosition);
        		break;
        	}
        	case bldc_limit:
        	case step_inf:
        	case bldc_inf:
        	{
        		break;
        	}
        	default:
        	{
        		break;
        	}
        }
	}
	else {
		position = pos_t::D_0_1;
	}

}

// ============================================================
// Calibration_pool() — неблокирующий FSM (вызывается периодически ~10 мс)
//   Возвращает true ровно один раз — когда калибровка успешно завершена.
//   Возвращает false во время выполнения и при ошибке.
// ============================================================
bool extern_driver::Calibration_pool() {

    switch (calibState) {

    // -----------------------------------------------------------
    case CalibState::IDLE:
    {
        if (!permission_calibrate) return false;
        if (settings->mod_rotation == step_inf || settings->mod_rotation == bldc_inf) return false;

        // Запомним исходный режим и переключимся в калибровочный
        tempCalibMode = settings->mod_rotation;
        switch (settings->mod_rotation) {
            case step_by_meter_timer_intermediate:
            case bldc_limit:
            case calibration_timer:
                settings->mod_rotation = mode_rotation_t::calibration_timer;
                break;
            case step_by_meter_enc_intermediate:
            case calibration_enc:
            default:
                settings->mod_rotation = mode_rotation_t::calibration_enc;
                break;
        }

        settings->points.is_calibrated = false;
        feedbackPosition = 0;
        resetFeedbackCounter();

        bool on_D0 = HAL_GPIO_ReadPin(D0_GPIO_Port, D0_Pin) == GPIO_PIN_SET;
        bool on_D1 = HAL_GPIO_ReadPin(D1_GPIO_Port, D1_Pin) == GPIO_PIN_SET;

        if (on_D1) {
            // Уже на D1 — едем сразу к D0
            STM_LOG("Calib: on D1, moving to D0");
            startForCall(dir::CW);
            calibState = CalibState::MOVE_TO_D0;
        } else {
            // На D0 или в середине — едем к D1
            STM_LOG("Calib: moving to D1");
            startForCall(dir::CCW);
            calibState = CalibState::MOVE_TO_D1;
        }
        return false;
    }

    // -----------------------------------------------------------
    case CalibState::MOVE_TO_D1:
    {
        if (Status != statusMotor::STOPPED) return false;  // ещё едем

        bool on_D1 = HAL_GPIO_ReadPin(D1_GPIO_Port, D1_Pin) == GPIO_PIN_SET;
        if (StatusTarget == statusTarget_t::finished && on_D1) {
            STM_LOG("Calib: reached D1, settling");
            feedbackPosition = 0;
            resetFeedbackCounter();
            calibDelayUntil = HAL_GetTick() + 10;
            calibState = CalibState::AT_D1_SETTLE;
        } else {
            STM_LOG("Calib: failed to reach D1");
            calibState = CalibState::ERROR;
        }
        return false;
    }

    // -----------------------------------------------------------
    case CalibState::AT_D1_SETTLE:
    {
        if (HAL_GetTick() < calibDelayUntil) return false;  // дождёмся задержки
        STM_LOG("Calib: moving D1 -> D0");
        startForCall(dir::CW);
        calibState = CalibState::MOVE_TO_D0;
        return false;
    }

    // -----------------------------------------------------------
    case CalibState::MOVE_TO_D0:
    {
        if (Status != statusMotor::STOPPED) return false;  // ещё едем

        bool on_D0 = HAL_GPIO_ReadPin(D0_GPIO_Port, D0_Pin) == GPIO_PIN_SET;
        if (StatusTarget == statusTarget_t::finished && on_D0) {
            // stop() уже зафиксировал полный путь D1→D0 в feedbackPosition.
            // feedbackPosition отрицательный (CW от feedbackPosition=0 на D1).
            CallSteps = (uint32_t)abs(feedbackPosition);
            STM_LOG("Calib: done. Steps=%lu", CallSteps);
            calibState = CalibState::DONE;
        } else {
            STM_LOG("Calib: failed to reach D0");
            calibState = CalibState::ERROR;
        }
        return false;
    }

    // -----------------------------------------------------------
    case CalibState::DONE:
    {
        settings->sensors_map.detected = true;
        settings->points.is_calibrated = true;
        permission_calibrate = false;
        feedbackPosition = 0;
        resetFeedbackCounter();
        settings->mod_rotation = tempCalibMode;
        calibState = CalibState::IDLE;
        return true;
    }

    // -----------------------------------------------------------
    case CalibState::ERROR:
    {
        permission_calibrate = false;
        settings->mod_rotation = tempCalibMode;
        calibState = CalibState::IDLE;
        return false;
    }

    } // switch

    return false;
}




bool extern_driver::limit_switch_pool() {
	if(Status != statusMotor::STOPPED)
	{
	    bool on_D0 = HAL_GPIO_ReadPin(D0_GPIO_Port, D0_Pin) == GPIO_PIN_SET;
	    bool on_D1 = HAL_GPIO_ReadPin(D1_GPIO_Port, D1_Pin) == GPIO_PIN_SET;

	    // Считаем последовательные срабатывания по каждому концевику.
	    // SensHandler (ISR) останавливает мотор в первые микросекунды после касания —
	    // следующий вызов pool увидит Status==STOPPED и сбросит счётчик.
	    // Если ISR по какой-то причине не сработал (мотор застрял), счётчик
	    // достигнет порога STUCK_THRESHOLD и сработает аварийная остановка.
	    if(!ignore_D0 && on_D0) d0_pool_cnt++; else d0_pool_cnt = 0;
	    if(!ignore_D1 && on_D1) d1_pool_cnt++; else d1_pool_cnt = 0;

	    const uint8_t STUCK_THRESHOLD = 3; // 3мс: ISR точно успел бы сработать
	    if(d0_pool_cnt >= STUCK_THRESHOLD) {
	        STM_LOG("emergency stop on limit switch:0");
	        d0_pool_cnt = 0;
	        stop(statusTarget_t::errLimitSwitch);
	    } else if(d1_pool_cnt >= STUCK_THRESHOLD) {
	        STM_LOG("emergency stop on limit switch:1");
	        d1_pool_cnt = 0;
	        stop(statusTarget_t::errLimitSwitch);
	    }
	}
	else
	{
	    // Мотор остановлен — сбрасываем счётчики чтобы не было накопленных значений
	    d0_pool_cnt = 0;
	    d1_pool_cnt = 0;
	}

	return true;
}

void extern_driver::CallStart() {
	permission_calibrate = true;
}

//methods for set************************************************
void extern_driver::SetSpeed(uint32_t percent) {
	if (percent > 1000) {
		percent = 1000;
	}
	else if (percent < 1) {
		percent = 1;
	}

	if (Status == statusMotor::STOPPED) {
		settings->Speed = (uint32_t) map_ARRFromPercent(percent);
	}
 	switch (settings->mod_rotation) {
		case step_by_meter_timer_intermediate:
		//case step_by_meter_timer_limit:
		case calibration_timer:
		case calibration_enc:
		case step_by_meter_enc_intermediate:
		//case step_by_meter_enc_limit:
		{
			break;
		}
		case bldc_limit:
    	case step_inf:
    	case bldc_inf:
		{
			//__HAL_TIM_SET_AUTORELOAD(TimFrequencies,(uint32_t) map_ARRFromPercent(percent));
			SET_TIM_ARR_AND_PULSE(TimFrequencies, ChannelClock, (uint32_t) map_ARRFromPercent(percent));
			break;
		}
		default:
		{
			break;
		}
    }
	Parameter_update();
}


void extern_driver::SetStartSpeed(uint32_t percent) {
	if (percent > 1000) {
		percent = 1000;
	}
	if (percent < 1) {
		percent = 1;
	}
	//settings->StartSpeed = (uint32_t) map(percent, 1, 1000, MinSpeed, MaxSpeed);
	settings->StartSpeed = (uint32_t) map_ARRFromPercent(percent);

}

void extern_driver::SetAcceleration(uint32_t StepsINmS) {
	settings->Accel = StepsINmS;
	Parameter_update();
}

void extern_driver::SetSlowdown(uint32_t steps) {
	settings->Slowdown = steps;
	Parameter_update();

}

void extern_driver::setTimeOut(uint32_t time) {
	settings->TimeOut = time;
}

// Модифицированный метод SetMode() с пересчетом частоты
void extern_driver::SetMode(mode_rotation_t mod) {
    if (!settings) {
        STM_LOG("Error: settings is null");
        return;
    }

    // Запоминаем текущий режим для сравнения
    mode_rotation_t oldMode = settings->mod_rotation;

    // Устанавливаем новый режим
    settings->mod_rotation = mod;

    // Если режим изменился, обновляем параметры таймера и пересчитываем частоты
    if (oldMode != mod) {
        // Обновляем предделитель таймера и диапазон скоростей
        switch (mod) {
            case step_by_meter_enc_intermediate:
            case step_by_meter_timer_intermediate:
            case step_inf:
            {
                TimFrequencies->Instance->PSC = 399;
                MaxSpeed = 50;
                MinSpeed = 13000;
                break;
            }
            case bldc_limit:
            case bldc_inf:
            {
                TimFrequencies->Instance->PSC = 200-1;
                MaxSpeed = 100;
                MinSpeed = 2666;
                break;
            }
            default:
            {
                TimFrequencies->Instance->PSC = 399;
                MaxSpeed = 50;
                MinSpeed = 13000;
                break;
            }
        }

        // Пересчитываем частоту тактирования таймера
        calculateTimerFrequency();

        // Рассчитываем количество шагов, необходимых для торможения
        uint32_t brakingSteps = calculateBrakingSteps();
        uint32_t accelSteps = calculateAccelSteps();
        // Пересчитываем таблицы разгона и торможения
        calculateAccelTable(accelSteps);
        calculateDecelTable(brakingSteps);


        STM_LOG("Mode changed to %d, timer parameters updated", (int)mod);
    }
}


// расчитывает и сохраняет все параметры разгона и торможения
void extern_driver::Parameter_update(void) {

	//FeedbackBraking_P1 = target - ((uint16_t) map(Deaccel, 1, 1000, 1, target));
	//FeedbackBraking_P0 = CircleCounts - ((uint16_t) map(Deaccel, 1, 1000, 1, CircleCounts - target)); // Начало торможения перед нулевой точкой в отсчетах это 1000

	//расчет шага ускорения/торможения
	//Accel = Speed / (target - FeedbackBraking_P1);
	//TimCountAllSteps->Instance->ARR = target;
}

void extern_driver::SetDirection(dir direction) {
	settings->Direct = direction;
}

//methods for get************************************************

uint32_t extern_driver::getAcceleration() {
	return (uint32_t) settings->Accel;
}

uint32_t extern_driver::getSlowdown() {
	return (uint32_t) settings->Slowdown;
}

uint32_t extern_driver::getSpeed() {

	//return (uint32_t) map(settings->Speed, MinSpeed, MaxSpeed, 1, 1000);
	return (uint32_t) map_PercentFromARR(settings->Speed);
}

uint32_t extern_driver::getStartSpeed() {
 //return StartSpeed;
 //return (uint32_t) map(settings->StartSpeed, MinSpeed, MaxSpeed, 1, 1000);
	return (uint32_t) map_PercentFromARR(settings->StartSpeed);
}

uint32_t extern_driver::getTimeOut() {
	return settings->TimeOut;
}

pos_t extern_driver::get_pos() {

	pos_t tmp_pos = pos_t::D_0_1;

	GPIO_PinState D0 = HAL_GPIO_ReadPin(D0_GPIO_Port, D0_Pin);
	GPIO_PinState D1 = HAL_GPIO_ReadPin(D1_GPIO_Port, D1_Pin);

	if ((D0 == GPIO_PIN_SET) && (D1 == GPIO_PIN_RESET)) {
		tmp_pos = pos_t::D0;
	}
	else if ((D0 == GPIO_PIN_RESET) && (D1 == GPIO_PIN_SET)){
		tmp_pos = pos_t::D1;
	}
	else {
		tmp_pos = pos_t::D_0_1;
	}

	return tmp_pos;
}

dir extern_driver::getStatusDirect() {
	return settings->Direct;
}

statusMotor extern_driver::getStatusRotation() {
	return Status;
}

uint16_t extern_driver::getRPM() {
	return 0;
}

mode_rotation_t extern_driver::getMode() {
	return settings->mod_rotation;
}

statusTarget_t extern_driver::getStatusTarget() {
	return StatusTarget;
}

void extern_driver::StartDebounceTimer(uint16_t GPIO_Pin) {
	// если мы находимся между концевиками то нужно остановить иначе запустить антидребезг

	//if((position == pos_t::D_0_1) && (ignore_sensors == false))
	/*if(ignore_sensors == false) // если таймер не запущен то просто обработка концевика
	{
		SensHandler(GPIO_Pin);
	}
	else // иначе запускаем таймер
	{
		// если таймер уже запущен то выходим
		if(is_start_ignore_timer) return;

	    if(GPIO_Pin == D0_Pin && !d0_debounce_active) {
			d0_debounce_active = true;
		}
		else if(GPIO_Pin == D1_Pin && !d1_debounce_active) {
			d1_debounce_active = true;
		}

		// Настраиваем и запускаем таймер
		__HAL_TIM_SET_COUNTER(debounceTimer, 0);
		__HAL_TIM_SET_AUTORELOAD(debounceTimer, DEBOUNCE_TIMEOUT);// настроить период
		HAL_TIM_Base_Start_IT(debounceTimer);
		is_start_ignore_timer = true;
	}*/

	// Игнорируем только тот концевик, с которого стартуем (защита от дребезга при отъезде).
	// Концевик назначения при этом остаётся активным.
	if(GPIO_Pin == D0_Pin) {
		ignore_D0 = true;
	} else if(GPIO_Pin == D1_Pin) {
		ignore_D1 = true;
	}
	vibration_start_time = HAL_GetTick();

	// если таймер уже запущен то выходим
	if(is_start_ignore_timer) return;

    if(GPIO_Pin == D0_Pin && !d0_debounce_active) {
		d0_debounce_active = true;
	}
	else if(GPIO_Pin == D1_Pin && !d1_debounce_active) {
		d1_debounce_active = true;
	}

	// Настраиваем и запускаем таймер
	__HAL_TIM_SET_COUNTER(debounceTimer, 0);
	__HAL_TIM_SET_AUTORELOAD(debounceTimer, (DEBOUNCE_TIMEOUT*1000)-1);// настроить период
	HAL_TIM_Base_Start_IT(debounceTimer);
	is_start_ignore_timer = true;

}

// обработчик таймера антидребезга
void extern_driver::HandleDebounceTimeout() {
    // Останавливаем таймер
    HAL_TIM_Base_Stop_IT(debounceTimer);
    is_start_ignore_timer = false;

    // Проверяем текущее состояние датчиков после таймаута дребезга
    bool on_D0 = HAL_GPIO_ReadPin(D0_GPIO_Port, D0_Pin) == GPIO_PIN_SET;
    bool on_D1 = HAL_GPIO_ReadPin(D1_GPIO_Port, D1_Pin) == GPIO_PIN_SET;

    if(on_D0) {
        // Мотор ещё на D0 (медленный отъезд или стоит) — оставляем ignore_D0
        d0_debounce_active = false;
    } else {
        // Мотор уехал с D0 — снимаем игнорирование
        ignore_D0 = false;
        d0_debounce_active = false;
    }

    if(on_D1) {
        // Мотор ещё на D1
        d1_debounce_active = false;
    } else {
        // Мотор уехал с D1 — снимаем игнорирование
        ignore_D1 = false;
        d1_debounce_active = false;
    }

    if(!on_D0 && !on_D1) {
        position = pos_t::D_0_1;
    }
    /*
    // Проверяем D0
    if(d0_debounce_active) {
        if(HAL_GPIO_ReadPin(D0_GPIO_Port, D0_Pin) == GPIO_PIN_SET) {
            // Сигнал все еще активен - вызываем обработчик
            SensHandler(D0_Pin);
        }
        d0_debounce_active = false;
    }

    // Проверяем D1
    if(d1_debounce_active) {
        if(HAL_GPIO_ReadPin(D1_GPIO_Port, D1_Pin) == GPIO_PIN_SET) {
            // Сигнал все еще активен - вызываем обработчик
            SensHandler(D1_Pin);
        }
        d1_debounce_active = false;
    }
    */
}

bool extern_driver::validatePointNumber(uint32_t point_number) {
    return (point_number < MAX_POINTS);
}

void extern_driver::updateCurrentPosition(uint32_t pos) {
    switch (settings->mod_rotation) {
        case calibration_enc:
        case step_by_meter_enc_intermediate:
            feedbackPosition = pos;
            __HAL_TIM_SET_COUNTER(TimEncoder, FEEDBACK_MID_VALUE);
            break;
        case calibration_timer:
        case step_by_meter_timer_intermediate:
            feedbackPosition = pos;
            __HAL_TIM_SET_COUNTER(TimCountAllSteps, FEEDBACK_MID_VALUE);
            break;
        default:
            __HAL_TIM_SET_COUNTER(TimCountAllSteps, pos);
            __HAL_TIM_SET_COUNTER(TimEncoder, FEEDBACK_MID_VALUE);
            break;
    }
}

bool extern_driver::saveCurrentPositionAsPoint(uint32_t point_number) {
    if(!validatePointNumber(point_number)) return false;
    if(!settings->points.is_calibrated) return false;

    settings->points.points[point_number] = getCurrentSteps();
    if(point_number >= settings->points.count) {
        settings->points.count = point_number + 1;
    }

    return true;
}

// Получение текущей абсолютной позиции
uint32_t extern_driver::getCurrentSteps() {
    uint32_t ret = 0;

    switch (settings->mod_rotation) {
        case step_by_meter_timer_intermediate:
        case calibration_timer:
        case step_by_meter_enc_intermediate:
        case calibration_enc:
        {
        	ret = (uint32_t)feedbackPosition;
            break;
        }
        case bldc_limit:
        case step_inf:
        case bldc_inf:
        default:
        {
            break;
        }
    }

    return ret;
}

bool extern_driver::gotoPoint(uint32_t point_number) {
    if(!validatePointNumber(point_number)) return false;
    if(!settings->points.is_calibrated) return false;
    if(point_number >= settings->points.count) return false;

    settings->points.target_point = point_number;

    // Делегируем в gotoPosition — она корректно вычисляет относительное расстояние
    return gotoPosition(settings->points.points[point_number]);
}

bool extern_driver::gotoLSwitch(uint8_t sw_x) {
    if (!settings->points.is_calibrated) return false;

    dir direction = (sw_x == 0) ? dir::CW : dir::CCW;

    // Точное расстояние до концевика — как в gotoPosition, но цель концевик.
    // D0: нужно пройти feedbackPosition шагов CW.
    // D1: нужно пройти (CallSteps - feedbackPosition) шагов CCW.
    // feedbackSetup настроит OC на это расстояние, мотор затормозит точно у цели.
    // SensHandler отработает: D0→pos=0, D1→pos=CallSteps.
    uint32_t stepsToSwitch;
    if (direction == dir::CW) {
        if (feedbackPosition <= 0) {
            STM_LOG("gotoLSwitch D0: already at D0");
            return false;
        }
        stepsToSwitch = (uint32_t)feedbackPosition;
    } else {
        if ((uint32_t)feedbackPosition >= CallSteps) {
            STM_LOG("gotoLSwitch D1: already at D1");
            return false;
        }
        stepsToSwitch = CallSteps - (uint32_t)feedbackPosition;
    }

    // Устанавливаем направление до start() — handleFeedbackStop использует settings->Direct.
    settings->Direct = direction;

    STM_LOG("gotoLSwitch: to %s, pos=%ld, steps=%lu",
            direction == dir::CCW ? "D1" : "D0", feedbackPosition, stepsToSwitch);

    return start(stepsToSwitch, direction);
}

bool extern_driver::validatePosition(uint32_t position) {
    if(!settings->points.is_calibrated) return false;

    // Проверяем что позиция в допустимых пределах
    uint32_t max_pos = getMaxPosition();
    uint32_t min_pos = getMinPosition();

    return (position >= min_pos && position <= max_pos);
}

uint32_t extern_driver::getMaxPosition() const {
    // Максимальная позиция - это позиция второго концевика
    return CallSteps;
}

uint32_t extern_driver::getMinPosition() const {
    return 0;
}

void extern_driver::setPoint(uint32_t point, uint32_t abs_steps){
	settings->points.points[point] = abs_steps;
}

bool extern_driver::gotoPosition(uint32_t position) {
    if (!validatePosition(position)) {
        STM_LOG("Invalid position requested: %lu", position);
        return false;
    }

    // Проверяем состояние драйвера
    if (getDriverStatus() != DRIVER_OK) {
        STM_LOG("Driver error, cannot move");
        return false;
    }

    // Получаем текущую позицию
    uint32_t currentPosition = getCurrentSteps();

    // Если уже в заданной позиции, ничего не делаем
    if (position == currentPosition) {
        STM_LOG("Already at requested position: %lu", position);
        return true;
    }

    // Определяем направление движения
    dir targetDirection;
    uint32_t stepsToMove;

    if (position > currentPosition) {
        targetDirection = dir::CCW;
        stepsToMove = position - currentPosition;
        STM_LOG("Moving CCW: %lu steps to position %lu", stepsToMove, position);
    } else {
        targetDirection = dir::CW;
        stepsToMove = currentPosition - position;
        STM_LOG("Moving CW: %lu steps to position %lu", stepsToMove, position);
    }

    // Устанавливаем направление
    settings->Direct = targetDirection;

    // Для режимов энкодера фиксируем целевую позицию, чтобы правильно обрабатывать остановку
    if (settings->mod_rotation == step_by_meter_enc_intermediate ||
        settings->mod_rotation == calibration_enc) {
        targetAbsolutePosition = position;
    }

    // Вызываем метод start, передавая количество шагов, которые нужно пройти
    return start(stepsToMove, targetDirection);
}


bool extern_driver::gotoInfinity() {
    switch (settings->mod_rotation) {
    	case calibration_enc:
    	case step_by_meter_enc_intermediate:
    	case calibration_timer:
    	case step_by_meter_timer_intermediate:
    	{
    		return false;
    		break;
    	}

    	case bldc_inf:
    	case step_inf:
    	case bldc_limit:
    	{
    		return start(-1);
    		break;
    	}
    	default:
    	{
    		return false;
    		break;
    	}
    }
}

bool extern_driver::isCalibrated() {
    return settings->points.is_calibrated;
}

//*******************************************************

double extern_driver::map(double x, double in_min, double in_max,
		double out_min, double out_max) {
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

/**
 * @brief Преобразует процентное значение в значение ARR с линейным изменением частоты
 * @param percent_value Значение в процентах (от 1 до 1000)
 * @return Значение регистра ARR (от MinSpeed до MaxSpeed)
 */
uint16_t extern_driver::map_ARRFromPercent(uint16_t percent_value)
{
    // Проверка входных значений
    if (percent_value < 1) {
        percent_value = 1;
    } else if (percent_value > 1000) {
        percent_value = 1000;
    }

    // Вычисляем частоту вместо ARR для линейности изменения скорости
    // Чем меньше ARR, тем выше частота (и скорость)
    float min_freq = 1.0f / MinSpeed;  // Минимальная частота
    float max_freq = 1.0f / MaxSpeed;  // Максимальная частота

    // Нормализованный процент (от 0.0 до 1.0)
    float normalized_percent = (float)(percent_value - 1) / (1000.0f - 1.0f);

    // Линейная интерполяция частоты
    float target_freq = min_freq + normalized_percent * (max_freq - min_freq);

    // Преобразуем частоту обратно в ARR (округляем до ближайшего целого)
    uint16_t arr_value = (uint16_t)(1.0f / target_freq + 0.5f);

    return arr_value;
}

/**
 * @brief Преобразует значение ARR в процентное значение с учетом линейности частоты
 * @param arr_value Значение регистра ARR
 * @return Значение в процентах (от 1 до 1000)
 */
uint16_t extern_driver::map_PercentFromARR(uint16_t arr_value)
{
    // Проверка входных значений
    if (arr_value < MaxSpeed) {
        arr_value = MaxSpeed;
    } else if (arr_value > MinSpeed) {
        arr_value = MinSpeed;
    }

    // Преобразуем ARR в частоту
    float current_freq = 1.0f / arr_value;
    float min_freq = 1.0f / MinSpeed;
    float max_freq = 1.0f / MaxSpeed;

    // Вычисляем нормализованный процент на основе линейной шкалы частоты
    float normalized_percent = (current_freq - min_freq) / (max_freq - min_freq);

    // Преобразуем в процентную шкалу от 1 до 1000
    float temp_percent = 1.0f + normalized_percent * (1000.0f - 1.0f);

    // Округляем до ближайшего целого
    uint16_t percent_value = (uint16_t)(temp_percent + 0.5f);

    // Проверка на граничные значения
    if (percent_value < 1) {
        percent_value = 1;
    } else if (percent_value > 1000) {
        percent_value = 1000;
    }

    return percent_value;
}



void  extern_driver::ChangeTimerMode(TIM_HandleTypeDef *htim, uint32_t Mode)
{
    // Остановить таймер
    HAL_TIM_Base_Stop(htim);

    // Изменить режим
    htim->Instance->CR1 &= ~TIM_CR1_CMS;  // Сбросить биты режима
    htim->Instance->CR1 &= ~TIM_CR1_DIR;  // Сбросить бит направления
    htim->Instance->CR1 |= Mode;          // Установить новый режим

    // Перезапустить таймер
    HAL_TIM_Base_Start(htim);
}

// Реализация метода расчета частоты тактирования таймера
void extern_driver::calculateTimerFrequency() {
    uint32_t timerClockFreq;

    // Определяем, к какой шине подключен таймер (APB1 или APB2)
    // TIM1 и TIM8-TIM11 обычно на APB2, остальные на APB1
    if (TimFrequencies->Instance == TIM1 ||
        TimFrequencies->Instance == TIM8 ||
        TimFrequencies->Instance == TIM9 ||
        TimFrequencies->Instance == TIM10 ||
        TimFrequencies->Instance == TIM11) {
        // Получаем частоту шины APB2
        timerClockFreq = HAL_RCC_GetPCLK2Freq();

        // Если делитель APB2 не равен 1, частота тактирования таймера удваивается
        if ((RCC->CFGR & RCC_CFGR_PPRE2) != 0) {
            timerClockFreq *= 2;
        }
    } else {
        // Получаем частоту шины APB1
        timerClockFreq = HAL_RCC_GetPCLK1Freq();

        // Если делитель APB1 не равен 1, частота тактирования таймера удваивается
        if ((RCC->CFGR & RCC_CFGR_PPRE1) != 0) {
            timerClockFreq *= 2;
        }
    }

    // Учитываем предделитель таймера
    uint32_t prescaler = TimFrequencies->Instance->PSC + 1;
    timerTickFreq = timerClockFreq / prescaler;

    //STM_LOG("Timer frequency calculated: %lu Hz (Clock: %lu Hz, Prescaler: %lu)", timerTickFreq, timerClockFreq, prescaler);
}

// Расчет количества шагов для ускорения
uint32_t extern_driver::calculateAccelSteps() {
    // Получаем начальную и целевую скорости
    uint32_t startARR = settings->StartSpeed;
    uint32_t targetARR = settings->Speed;

    // Проверка соответствия значений диапазонам
    if (startARR < MaxSpeed) startARR = MaxSpeed;
    if (startARR > MinSpeed) startARR = MinSpeed;
    if (targetARR < MaxSpeed) targetARR = MaxSpeed;
    if (targetARR > MinSpeed) targetARR = MinSpeed;

    // Преобразуем ARR в частоту (Гц) с использованием предварительно рассчитанной частоты
    float initialFreq = static_cast<float>(timerTickFreq) / (startARR + 1);   // Начальная скорость
    float finalFreq = static_cast<float>(timerTickFreq) / (targetARR + 1);    // Целевая скорость

    // Рассчитываем количество шагов для разгона на основе разницы скоростей
    // Используем формулу: s = (v_final^2 - v_initial^2) / (2 * acceleration)
    float v_initial = initialFreq / 1000.0f;  // Нормализуем для расчета
    float v_final = finalFreq / 1000.0f;      // Нормализуем для расчета
    float acceleration = settings->Accel / 10000.0f;  // Коэффициент ускорения

    // Рассчитываем количество шагов (округляем вверх)
    uint32_t accelSteps = static_cast<uint32_t>(
        (powf(v_final, 2) - powf(v_initial, 2)) / (2 * acceleration) + 0.5f
    );

    // Минимальное безопасное значение
    if (accelSteps < 20U) {
        accelSteps = 20U;
    }

    // Ограничиваем количество шагов размером буфера таблицы
    if (accelSteps > MAX_RAMP_STEPS) {
        accelSteps = MAX_RAMP_STEPS;
        STM_LOG("Warning: Acceleration steps limited to %d (buffer size)", MAX_RAMP_STEPS);
    }

    //STM_LOG("Calculated acceleration steps: %lu (initial freq: %f Hz, final freq: %f Hz)", accelSteps, initialFreq, finalFreq);

    return accelSteps;
}

// Обновленный метод расчета количества шагов для торможения с использованием целевой скорости
uint32_t extern_driver::calculateBrakingSteps() {
    // Получаем целевую скорость из настроек, а не текущее значение ARR
    uint32_t targetARR = settings->Speed;

    // Проверка соответствия значения диапазону
    if (targetARR < MaxSpeed) targetARR = MaxSpeed;
    if (targetARR > MinSpeed) targetARR = MinSpeed;

    // Преобразуем ARR в частоту (Гц) с использованием предварительно рассчитанной частоты
    float initialFreq = static_cast<float>(timerTickFreq) / (targetARR + 1);  // Целевая скорость (начальная для торможения)
    float finalFreq = static_cast<float>(timerTickFreq) / (MinSpeed + 1);     // Минимальная скорость (конечная для торможения)

    // Рассчитываем дистанцию торможения на основе разницы скоростей
    // Используем формулу: s = (v_initial^2 - v_final^2) / (2 * deceleration)
    float v_initial = initialFreq / 1000.0f;  // Нормализуем для расчета
    float v_final = finalFreq / 1000.0f;      // Нормализуем для расчета
    float deceleration = settings->Slowdown / 10000.0f;  // Коэффициент замедления

    // Добавляем запас 20%
    uint32_t brakingSteps = static_cast<uint32_t>(
        (powf(v_initial, 2) - powf(v_final, 2)) / (2 * deceleration) * 1.2f
    );

    // Минимальное безопасное значение
    if (brakingSteps < 50U) {
        brakingSteps = 50U;
    }

    // Ограничиваем количество шагов размером буфера таблицы
    if (brakingSteps > MAX_RAMP_STEPS) {
        brakingSteps = MAX_RAMP_STEPS;
        STM_LOG("Warning: Braking steps limited to %d (buffer size)", MAX_RAMP_STEPS);
    }

    // Используем %f вместо %.2f для улучшения совместимости с форматированием
    //STM_LOG("Calculated braking steps: %lu (initial freq: %f Hz, final freq: %f Hz)", brakingSteps, initialFreq, finalFreq);

    return brakingSteps;
}

// Реализация метода расчета таблицы разгона
void extern_driver::calculateAccelTable(uint32_t accelSteps) {
    // Получаем параметры разгона
    uint32_t startARR = settings->StartSpeed;  // Большое значение ARR = низкая скорость
    uint32_t targetARR = settings->Speed;      // Малое значение ARR = высокая скорость

    // Проверка соответствия значений диапазонам
    if (startARR < MaxSpeed) startARR = MaxSpeed;
    if (startARR > MinSpeed) startARR = MinSpeed;
    if (targetARR < MaxSpeed) targetARR = MaxSpeed;
    if (targetARR > MinSpeed) targetARR = MinSpeed;

    // Ограничиваем количество шагов таблицей и имеющимся диапазоном
    uint16_t steps = static_cast<uint16_t>(accelSteps);
    if (steps > MAX_RAMP_STEPS) steps = MAX_RAMP_STEPS;

    // Для линейного изменения частоты, нам нужно нелинейно менять ARR
    // Преобразуем ARR в частоту
    double startFreq = 1.0 / startARR;
    double targetFreq = 1.0 / targetARR;
    double freqDiff = targetFreq - startFreq;

    // Заполняем таблицу ускорения с линейным изменением частоты
    for (uint16_t i = 0; i < steps; i++) {
        // Линейно увеличиваем частоту
        double progress = static_cast<double>(i) / steps;
        double currentFreq = startFreq + (freqDiff * progress);

        // Преобразуем частоту обратно в ARR
        uint32_t arr = static_cast<uint32_t>(1.0 / currentFreq);

        // Проверка границ значения ARR
        if (arr < MaxSpeed) arr = MaxSpeed;
        if (arr > MinSpeed) arr = MinSpeed;

        // Сохраняем значение в таблице
        rampTables.accelTable[i] = arr;
    }

    // Убедимся, что последнее значение точно соответствует целевому
    if (steps > 0) {
        rampTables.accelTable[steps - 1] = targetARR;
    }

    // Сохраняем количество шагов в таблице
    rampTables.accelSteps = steps;

    // Выводим диагностику
    //STM_LOG("Linear acceleration table: steps=%d, start=%lu, end=%lu", steps, rampTables.accelTable[0], rampTables.accelTable[steps-1]);
}

// Реализация метода расчета таблицы торможения
void extern_driver::calculateDecelTable(uint32_t brakingSteps) {
    // Получаем параметры торможения
    uint32_t startARR = settings->Speed;   // Начинаем с основной скорости
    uint32_t targetARR = MinSpeed;        // Заканчиваем на минимальной скорости

    // Проверка соответствия значений диапазонам
    if (startARR < MaxSpeed) startARR = MaxSpeed;
    if (startARR > MinSpeed) startARR = MinSpeed;

    // Ограничиваем количество шагов таблицей
    uint16_t steps = static_cast<uint16_t>(brakingSteps);
    if (steps > MAX_RAMP_STEPS) steps = MAX_RAMP_STEPS;

    // Для линейного изменения частоты, нам нужно нелинейно менять ARR
    // Преобразуем ARR в частоту
    double startFreq = 1.0 / startARR;
    double targetFreq = 1.0 / targetARR;
    double freqDiff = targetFreq - startFreq; // Будет отрицательное значение для торможения

    // Заполняем таблицу торможения с линейным изменением частоты
    for (uint16_t i = 0; i < steps; i++) {
        // Линейно уменьшаем частоту
        double progress = static_cast<double>(i) / steps;
        double currentFreq = startFreq + (freqDiff * progress);

        // Преобразуем частоту обратно в ARR
        uint32_t arr = static_cast<uint32_t>(1.0 / currentFreq);

        // Проверка границ значения ARR
        if (arr < MaxSpeed) arr = MaxSpeed;
        if (arr > MinSpeed) arr = MinSpeed;

        // Сохраняем значение в таблице
        rampTables.decelTable[i] = arr;
    }

    // Убедимся, что последнее значение точно соответствует целевому
    if (steps > 0) {
        rampTables.decelTable[steps - 1] = targetARR;
    }

    // Сохраняем количество шагов в таблице
    rampTables.decelSteps = steps;

    // Выводим диагностику
    //STM_LOG("Linear deceleration table: steps=%d, start=%lu, end=%lu", steps, rampTables.decelTable[0], rampTables.decelTable[steps-1]);
}

/**
 * Обработчик разгона и торможения двигателя.
 * Управляет ускорением, замедлением и проверяет корректность движения в зависимости от режима работы.
 *
 * Обработчик прерывания таймера Разгон и торможение
 */
void extern_driver::handleTimerInterrupt() {

	// Обработка разгона
	if (Status == statusMotor::ACCEL) {
		// Увеличиваем счетчик шагов в таблице
		rampTables.currentStep++;

		// Проверяем, достигли ли конца таблицы разгона
		if (rampTables.currentStep >= rampTables.accelSteps) {
			// Достигнута целевая скорость
			//__HAL_TIM_SET_AUTORELOAD(TimFrequencies, settings->Speed);
			SET_TIM_ARR_AND_PULSE(TimFrequencies, ChannelClock, settings->Speed);
			Status = statusMotor::MOTION;
			STM_LOG("Acceleration complete, reached target speed");
		} else {
			// Устанавливаем следующее значение ARR из таблицы
			//__HAL_TIM_SET_AUTORELOAD(TimFrequencies, rampTables.accelTable[rampTables.currentStep]);
			SET_TIM_ARR_AND_PULSE(TimFrequencies, ChannelClock, rampTables.accelTable[rampTables.currentStep]);
		}
	}
	// Обработка торможения
	else if (Status == statusMotor::BRAKING) {
		// Увеличиваем счетчик шагов в таблице
		rampTables.currentStep++;

		// Проверяем, достигли ли конца таблицы торможения
		if (rampTables.currentStep >= rampTables.decelSteps) {
			// Достигнута минимальная скорость
			//__HAL_TIM_SET_AUTORELOAD(TimFrequencies, MinSpeed);
			SET_TIM_ARR_AND_PULSE(TimFrequencies, ChannelClock, MinSpeed);

			// В зависимости от режима работы, можем остановиться или продолжить на минимальной скорости
			switch (settings->mod_rotation) {
				case step_inf:
				case bldc_inf:
					// В бесконечных режимах останавливаемся
					stop(statusTarget_t::finished);
					break;
				default:
					// В других режимах продолжаем на минимальной скорости до достижения целевой позиции
					break;
			}

			//STM_LOG("Deceleration complete, reached minimum speed");
		} else {
			// Устанавливаем следующее значение ARR из таблицы
			//__HAL_TIM_SET_AUTORELOAD(TimFrequencies, rampTables.decelTable[rampTables.currentStep]);
			SET_TIM_ARR_AND_PULSE(TimFrequencies, ChannelClock, rampTables.decelTable[rampTables.currentStep]);
		}
	}
}

extern_driver::~extern_driver() {

}

extern_driver::extern_driver(settings_t *set, TIM_HandleTypeDef *timCount,
		TIM_HandleTypeDef *timFreq, uint32_t channelFreq,
		TIM_HandleTypeDef *timDebounce, TIM_HandleTypeDef *timENC) :
		settings(set), TimCountAllSteps(timCount), TimFrequencies(timFreq), ChannelClock(
				channelFreq), debounceTimer(timDebounce), TimEncoder(timENC) {
}

// ============================================================
// Helpers для выбора feedback-таймера в зависимости от режима
// ============================================================

bool extern_driver::isEncoderMode() const {
    return (settings->mod_rotation == step_by_meter_enc_intermediate ||
            settings->mod_rotation == calibration_enc);
}

TIM_HandleTypeDef* extern_driver::getFeedbackTimer() const {
    return isEncoderMode() ? TimEncoder : TimCountAllSteps;
}

// Канал начала торможения: TIM3.CH3 (энкодер) или TIM4.CH1 (счётчик)
uint32_t extern_driver::getFeedbackCH_brake() const {
    return isEncoderMode() ? TIM_CHANNEL_3 : TIM_CHANNEL_1;
}

// Канал остановки / конца сегмента: TIM3.CH4 (энкодер) или TIM4.CH2 (счётчик)
uint32_t extern_driver::getFeedbackCH_stop() const {
    return isEncoderMode() ? TIM_CHANNEL_4 : TIM_CHANNEL_2;
}

// Сброс feedback-счётчика в среднее значение диапазона
void extern_driver::resetFeedbackCounter() {
    __HAL_TIM_SET_COUNTER(getFeedbackTimer(), FEEDBACK_MID_VALUE);
}

// ============================================================
// feedbackSetup — настройка OC перед началом движения
//   Заменяет setupEncoderMovement() и setupTimerMovement()
// ============================================================
void extern_driver::feedbackSetup(uint32_t totalSteps, dir direction) {
    TIM_HandleTypeDef* fbTim = getFeedbackTimer();
    uint32_t chBrake = getFeedbackCH_brake();
    uint32_t chStop  = getFeedbackCH_stop();

    resetFeedbackCounter();
    totalRemainingSteps = totalSteps;

    if (totalSteps <= FEEDBACK_MAX_PART) {
        // Одна часть — сразу настраиваем тормоз и остановку
        isLastSegment = true;
        if (direction == dir::CCW) {
            uint16_t brakePoint = FEEDBACK_MID_VALUE + (totalSteps - rampTables.decelSteps);
            uint16_t stopPoint  = FEEDBACK_MID_VALUE + totalSteps;
            __HAL_TIM_SET_COMPARE(fbTim, chBrake, brakePoint);
            __HAL_TIM_SET_COMPARE(fbTim, chStop,  stopPoint);
        } else {
            uint16_t brakePoint = FEEDBACK_MID_VALUE - (totalSteps - rampTables.decelSteps);
            uint16_t stopPoint  = FEEDBACK_MID_VALUE - totalSteps;
            __HAL_TIM_SET_COMPARE(fbTim, chBrake, brakePoint);
            __HAL_TIM_SET_COMPARE(fbTim, chStop,  stopPoint);
        }
    } else {
        // Многочастное движение — первый сегмент максимальной длины
        isLastSegment = false;
        uint16_t partEnd = (direction == dir::CCW)
            ? FEEDBACK_MID_VALUE + FEEDBACK_MAX_PART
            : FEEDBACK_MID_VALUE - FEEDBACK_MAX_PART;
        __HAL_TIM_SET_COMPARE(fbTim, chBrake, partEnd);
        __HAL_TIM_SET_COMPARE(fbTim, chStop,  partEnd);
    }

    HAL_TIM_OC_Start_IT(fbTim, chBrake);
    HAL_TIM_OC_Start_IT(fbTim, chStop);
}

// ============================================================
// handleFeedbackStop — внутренний обработчик конца сегмента/остановки
//
// Последний сегмент: делегирует stop(), которая сама фиксирует (cnt-MID).
// Промежуточный сегмент: вручную накапливает ровно FEEDBACK_MAX_PART шагов
//   (OC выставлен точно на это значение), сбрасывает счётчик, настраивает следующий OC.
// ============================================================
void extern_driver::handleFeedbackStop() {
    TIM_HandleTypeDef* fbTim = getFeedbackTimer();
    uint32_t chBrake = getFeedbackCH_brake();
    uint32_t chStop  = getFeedbackCH_stop();

    if (isLastSegment) {
        // stop() зафиксирует (cnt - MID) в feedbackPosition и сбросит счётчик.
        stop(statusTarget_t::finished);
        // Притягиваем к точной цели, если погрешность в пределах 5 шагов.
        if (targetAbsolutePosition > 0 &&
            (uint32_t)abs((int32_t)getCurrentSteps() - (int32_t)targetAbsolutePosition) <= 5) {
            feedbackPosition = (int32_t)targetAbsolutePosition;
            STM_LOG("Position snapped to target: %lu", targetAbsolutePosition);
        }
    } else {
        // Промежуточный сегмент: мотор продолжает движение.
        // Накапливаем ровно FEEDBACK_MAX_PART — OC был выставлен именно на эту точку.
        if (settings->Direct == dir::CCW) {
            feedbackPosition += (int32_t)FEEDBACK_MAX_PART;
        } else {
            feedbackPosition -= (int32_t)FEEDBACK_MAX_PART;
        }
        if (totalRemainingSteps > FEEDBACK_MAX_PART)
            totalRemainingSteps -= FEEDBACK_MAX_PART;
        else
            totalRemainingSteps = 0;
        resetFeedbackCounter();

        if (totalRemainingSteps <= FEEDBACK_MAX_PART) {
            isLastSegment = true;
            if (settings->Direct == dir::CCW) {
                uint16_t brakePoint = FEEDBACK_MID_VALUE + (totalRemainingSteps - rampTables.decelSteps);
                uint16_t stopPoint  = FEEDBACK_MID_VALUE + totalRemainingSteps;
                __HAL_TIM_SET_COMPARE(fbTim, chBrake, brakePoint);
                __HAL_TIM_SET_COMPARE(fbTim, chStop,  stopPoint);
            } else {
                uint16_t brakePoint = FEEDBACK_MID_VALUE - (totalRemainingSteps - rampTables.decelSteps);
                uint16_t stopPoint  = FEEDBACK_MID_VALUE - totalRemainingSteps;
                __HAL_TIM_SET_COMPARE(fbTim, chBrake, brakePoint);
                __HAL_TIM_SET_COMPARE(fbTim, chStop,  stopPoint);
            }
        } else {
            uint16_t nextPartEnd = (settings->Direct == dir::CCW)
                ? FEEDBACK_MID_VALUE + FEEDBACK_MAX_PART
                : FEEDBACK_MID_VALUE - FEEDBACK_MAX_PART;
            __HAL_TIM_SET_COMPARE(fbTim, chBrake, nextPartEnd);
            __HAL_TIM_SET_COMPARE(fbTim, chStop,  nextPartEnd);
        }
    }
}

// ============================================================
// handleFeedbackCompare — единый обработчик OC feedback-таймера
//   Заменяет handleEncoderCompare() и handleTimerCompare()
//   Вызывается из HAL_TIM_OC_DelayElapsedCallback в main.cpp
// ============================================================
void extern_driver::handleFeedbackCompare(TIM_HandleTypeDef *htim, uint32_t channel) {
    TIM_HandleTypeDef* fbTim = getFeedbackTimer();
    if (htim->Instance != fbTim->Instance) return;

    uint16_t currentCount = __HAL_TIM_GET_COUNTER(htim);
    uint32_t chBrake = getFeedbackCH_brake();
    uint32_t chStop  = getFeedbackCH_stop();

    if (channel == chBrake) {
        slowdown();
    } else if (channel == chStop) {
        handleFeedbackStop();
    }
}


