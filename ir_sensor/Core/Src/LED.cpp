#include "LED.h"


void led_t::poll(){
   if(StatusLED == status_led_t::ON){
      switch(LED_mod)
      {
        case mode::BLINK:
        {
           if(Timer == Counter_ms){
              if(StateLED == status_led_t::ON){
                 HAL_GPIO_WritePin(GPIOx, GPIO_Pin, GPIO_PIN_SET); // off led
                 Timer = Period - Time_on;
                 Counter_ms = 0;
                 StateLED = status_led_t::OFF;
              }else{
                 HAL_GPIO_WritePin(GPIOx, GPIO_Pin, GPIO_PIN_RESET); // on led
                 Timer = Time_on;
                 Counter_ms = 0;
                 StateLED = status_led_t::ON;
              }
           }else{
              Counter_ms++; 
           }
          break;
        }
        case mode::ON_OFF:
        {
           if(StateLED == status_led_t::OFF) {
              HAL_GPIO_WritePin(GPIOx, GPIO_Pin, GPIO_PIN_RESET); // on led
              StateLED = status_led_t::ON;
           }
          break;
        }
        case mode::PULSE:
        {
           if(Timer == Counter_ms){
              if(StateLED == status_led_t::ON){
                 HAL_GPIO_WritePin(GPIOx, GPIO_Pin, GPIO_PIN_SET); // off led
                 Timer = Period - Time_on;
                 Counter_ms = 0;
                 StateLED = status_led_t::OFF;
                 if(PulseCount == Pulses){
                    StatusLED = status_led_t::OFF;
                    LED_mod = mode::ON_OFF;
                    PulseCount = 0;
                    Pulses = 0;
                 }else{
                    PulseCount++; 
                 }
              }else{
                 HAL_GPIO_WritePin(GPIOx, GPIO_Pin, GPIO_PIN_RESET); // on led
                 Timer = Time_on;
                 Counter_ms = 0;
                 StateLED = status_led_t::ON;
              }
           }else{
              Counter_ms++; 
           }
          break;
        }
        default:
        {
          break;
        }
        
      }

   }else{
      HAL_GPIO_WritePin(GPIOx, GPIO_Pin, GPIO_PIN_SET); // off led
      Counter_ms = 0;
      StateLED = status_led_t::OFF;
   }
}

void led_t::Init(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin){
   led_t::GPIOx = GPIOx;
   led_t::GPIO_Pin = GPIO_Pin;
   HAL_GPIO_WritePin(GPIOx, GPIO_Pin, GPIO_PIN_SET); // off led
   InitStatus = 1;
}

void led_t::LEDon(){
   StatusLED = status_led_t::ON;
}

void led_t::LEDon(uint32_t pulses){
   StatusLED = status_led_t::ON;
   LED_mod = mode::PULSE;
   Pulses += pulses;
}

void led_t::LEDoff(){
   StatusLED = status_led_t::OFF;
}

void led_t::setParameters(mode Mode, uint32_t period, uint32_t time_on){
   LED_mod = Mode;
   Period = period;
   Time_on = time_on;
}

void led_t::setParameters(mode Mode){
   LED_mod = Mode;
}

led_t::led_t(){

}

led_t::~led_t(){

}
