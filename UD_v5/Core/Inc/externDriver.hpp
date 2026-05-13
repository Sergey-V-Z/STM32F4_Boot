#pragma once
#include "main.h"
#include "cmsis_os.h"
#include "Delay_us_DWT.h"
#include "stdlib.h"

#define DIRECT_CCW HAL_GPIO_WritePin(CW_CCW_GPIO_Port, CW_CCW_Pin, GPIO_PIN_RESET);
#define DIRECT_CW HAL_GPIO_WritePin(CW_CCW_GPIO_Port, CW_CCW_Pin, GPIO_PIN_SET);

#define SET_TIM_ARR_AND_PULSE(htim, channel, arr_value) do { \
    __HAL_TIM_SET_AUTORELOAD((htim), (arr_value)); \
    __HAL_TIM_SET_COMPARE((htim), (channel), (arr_value) / 2); \
} while(0)

// Новый enum для состояния драйвера
typedef enum {
    DRIVER_OK = 0,
    DRIVER_ERROR,
    DRIVER_STATUS_UNKNOWN
} driver_status_t;

typedef enum {
    HALF,
    FULL
} step;

typedef enum {
    inProgress = 0,
    finished,
    errMotion,
    errDirection,
    errDriver,              // ошибка драйвера
    errLimitSwitch,         // аварийный стоп по концевику (limit_switch_pool)
    finishedByLimitSwitch   // нормальная остановка на концевике (SensHandler)
} statusTarget_t;

typedef enum {
    MOTION,
    STOPPED,
    ACCEL,
    BRAKING,
    ERROR_M   // Добавляем состояние ошибки
} statusMotor;

typedef enum {
    ENCODER,
    HALLSENSOR,
    NON
} fb;

typedef enum {
    OK,
    No_Connect,
    No_Signal
} sensorsERROR;

enum class pos_t {
    D0,
    D_0_1,
    D1
};

class extern_driver {
public:
    extern_driver(settings_t *set, TIM_HandleTypeDef *timCount, TIM_HandleTypeDef *timFreq,
                 uint32_t channelFreq, TIM_HandleTypeDef *timDebounce, TIM_HandleTypeDef *timENC);
    ~extern_driver();

    // Методы для работы с состоянием драйвера
    driver_status_t getDriverStatus();
    uint32_t getLastDriverCheck();
    void checkDriverStatus();
    bool isDriverStatusValid();

    // Методы установки параметров
    void SetDirection(dir direction);
    void SetSpeed(uint32_t percent);
    void SetStartSpeed(uint32_t percent);
    void SetAcceleration(uint32_t StepsINmS);
    void SetSlowdown(uint32_t steps);
    void setTimeOut(uint32_t time);
    //void SetZeroPoint(void);
    void SetMode(mode_rotation_t mod);
    void Parameter_update(void);
    void updateCurrentPosition(uint32_t pos);
    void setPoint(uint32_t point, uint32_t abs_steps);

    // Методы получения параметров
    pos_t get_pos();
    uint32_t getAcceleration();
    uint32_t getSlowdown();
    uint32_t getSpeed();
    uint32_t getStartSpeed();
    uint32_t getTimeOut();
    dir getStatusDirect();
    statusMotor getStatusRotation();
    uint16_t getRPM();
    mode_rotation_t getMode();
    statusTarget_t getStatusTarget();
    //uint32_t getLastDistance();

    // Методы управления движением
    bool start(uint32_t steps, dir d = dir::END_OF_LIST);
    bool startForCall(dir d); // используется только калибровкой

    void stop(statusTarget_t status, bool force_disable = false);
    void slowdown();
    void removeBreak(bool status);
    void Init();
    bool Calibration_pool();
    bool limit_switch_pool();
    void CallStart();

    bool saveCurrentPositionAsPoint(uint32_t point_number);
    uint32_t getCurrentSteps();
    bool gotoPoint(uint32_t point_number);
    bool gotoLSwitch(uint8_t sw_x);
    uint32_t getCurrentPoint() { return settings->points.current_point; }
    uint32_t getTargetPoint() { return settings->points.target_point; }
    bool gotoPosition(uint32_t position);
    bool gotoInfinity();
    uint32_t getMaxPosition() const;
    uint32_t getMinPosition() const;
    bool isCalibrated();

    // Управление режимом отключения EN (0=выкл, 1=любой стоп, 2=только по шагам, 3=только концевик)
    uint8_t getDisableMode() const  { return (uint8_t)((settings->res1 & DISABLE_MODE_MASK) >> DISABLE_MODE_SHIFT); }
    void setDisableMode(uint8_t mode) {
        settings->res1 = (settings->res1 & ~DISABLE_MODE_MASK) | (((uint32_t)(mode & 0x03U)) << DISABLE_MODE_SHIFT);
    }
    // Для обратной совместимости (устарели)
    bool getDisableOnStop() const  { return getDisableMode() != 0; }
    void setDisableOnStop(bool val) { setDisableMode(val ? 1U : 0U); }
    bool getEnInvert() const { return (settings->res1 & FLAG_EN_INVERT) != 0; }
    void setEnInvert(bool val) {
        if (val) settings->res1 |=  FLAG_EN_INVERT;
        else     settings->res1 &= ~FLAG_EN_INVERT;
    }

    // Обработчики прерываний
    void SensHandler(uint16_t GPIO_Pin);             // EXTI концевики
    void StartDebounceTimer(uint16_t GPIO_Pin);       // Запуск антидребезга при старте
    void HandleDebounceTimeout();                    // TIM6 callback
    void handleTimerInterrupt();                     // TIM1 OC callback (шаг генератор)
    // Единый обработчик feedback-таймера (заменяет handleEncoderCompare + handleTimerCompare)
    void handleFeedbackCompare(TIM_HandleTypeDef *htim, uint32_t channel);

    // Управление EN с учётом инверсии
    void enableDriver();
    void disableDriver();
private:
    // ---- Feedback: единая позиция для энкодера и таймера счётчика ----
    volatile int32_t feedbackPosition = 0;   // заменяет globalPosition и globalPositionTimer
    uint32_t targetAbsolutePosition = 0;          // Целевая позиция для коррекции
    static const uint16_t FEEDBACK_MID_VALUE = 0x7FFF; // Середина 16-битного диапазона
    static const uint16_t FEEDBACK_MAX_PART  = 0x6000; // Максимальный шаг одного сегмента
    volatile bool isLastSegment = false;                // Флаг последнего сегмента движения
    uint32_t totalRemainingSteps = 0;          // Оставшиеся шаги

    bool validatePointNumber(uint32_t point_number);
    bool validatePosition(uint32_t position);

    // Helpers для выбора feedback-таймера (энкодер TIM3 или счётчик TIM4)
    bool isEncoderMode() const;
    TIM_HandleTypeDef* getFeedbackTimer() const;
    uint32_t getFeedbackCH_brake() const; // канал начала торможения
    uint32_t getFeedbackCH_stop()  const; // канал остановки / конца сегмента
    void feedbackSetup(uint32_t totalSteps, dir direction); // настройка OC перед движением
    void handleFeedbackStop();                               // внутренний обработчик остановки
    void resetFeedbackCounter();                            // сброс счётчика на MID

    double map(double x, double in_min, double in_max, double out_min, double out_max);
    void ChangeTimerMode(TIM_HandleTypeDef *htim, uint32_t Mode);


    void calculateTimerFrequency();
    uint16_t map_PercentFromARR(uint16_t arr_value);
    uint16_t map_ARRFromPercent(uint16_t percent_value);
    void calculateAccelTable(uint32_t accelSteps);
    void calculateDecelTable(uint32_t brakingSteps);
    uint32_t calculateBrakingSteps();
    uint32_t calculateAccelSteps();

    // ---- Calibration FSM ----
    enum class CalibState { IDLE, MOVE_TO_D1, AT_D1_SETTLE, MOVE_TO_D0, DONE, ERROR };
    CalibState calibState = CalibState::IDLE;
    uint32_t   calibDelayUntil = 0;
    mode_rotation_t tempCalibMode = step_inf;

    // ---- Параметры драйвера ----
    driver_status_t currentDriverStatus;
    uint32_t lastDriverCheckTime;
    const uint32_t DRIVER_STATUS_VALIDITY_TIME = 1000;
    GPIO_TypeDef* driverErrorPort;
    uint16_t driverErrorPin;

    // ---- Основные дескрипторы ----
    settings_t *settings;
    TIM_HandleTypeDef *TimCountAllSteps; // TIM4 — счётчик шагов (slave)
    TIM_HandleTypeDef *TimFrequencies;   // TIM1 — генератор шагов (PWM)
    uint32_t ChannelClock;
    TIM_HandleTypeDef* debounceTimer;    // TIM6 — антидребезг
    TIM_HandleTypeDef *TimEncoder;       // TIM3 — квадратурный энкодер

    const uint32_t DEBOUNCE_TIMEOUT = 50;
    // Раздельное игнорирование концевиков: устанавливается при старте
    // с соответствующего концевика, чтобы не реагировать на дребезг при отъезде.
    // Концевик НАЗНАЧЕНИЯ при этом НИКОГДА не игнорируется.
    volatile bool ignore_D0 = false;  // игнорировать D0 (только что стартовали с него)
    volatile bool ignore_D1 = false;  // игнорировать D1 (только что стартовали с него)
    bool is_start_ignore_timer = false;
    volatile bool d0_debounce_active = false;
    volatile bool d1_debounce_active = false;

    // Счётчики последовательных срабатываний для limit_switch_pool.
    // ISR (SensHandler) останавливает мотор в первые микросекунды после касания,
    // поэтому при нормальной работе счётчик никогда не достигает порога.
    // Аварийная остановка — только если мотор застрял и ISR не сработал.
    uint8_t d0_pool_cnt = 0;
    uint8_t d1_pool_cnt = 0;

    // ---- Параметры движения ----
    uint32_t MaxSpeed = 1;
    uint32_t MinSpeed = 20000;
    uint32_t Time = 0;
    uint8_t TimerIsStart = false;
    uint32_t PrevCounterENC = 0;
    uint8_t countErrDir = 3;
    uint32_t CallSteps = 0;
    uint32_t motionSteps = 0;
    uint32_t Speed_Call = 0;
    uint32_t Speed_temp = 0;

    // ---- Состояния ----
    volatile statusMotor Status = statusMotor::STOPPED;
    volatile statusTarget_t StatusTarget = statusTarget_t::finished;
    fb FeedbackType = fb::NON;

    // ---- Позиционирование ----
    const uint32_t START_VIBRATION_TIMEOUT = 30;
    uint32_t vibration_start_time = 0;
    pos_t position = pos_t::D_0_1;
    pos_t target = pos_t::D_0_1;
    uint32_t watchdog = 10000;
    bool permission_calibrate = false;
    bool change_pos = false;
    uint32_t time = 0;
    uint8_t bos_bit = 0;
    uint32_t timerTickFreq;

    // Максимальное количество шагов в таблице разгона/торможения
    static const uint16_t MAX_RAMP_STEPS = 1000;

    // Структура для таблиц разгона и торможения
    struct {
        uint32_t accelTable[MAX_RAMP_STEPS];  // Таблица значений ARR для разгона
        uint32_t decelTable[MAX_RAMP_STEPS];  // Таблица значений ARR для торможения
        uint16_t accelSteps;                  // Количество шагов в таблице разгона
        uint16_t decelSteps;                  // Количество шагов в таблице торможения
        uint16_t currentStep;                 // Текущий шаг в таблице (для разгона или торможения)
    } rampTables;

};
