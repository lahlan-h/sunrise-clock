#include <Arduino.h>

#include "secrets.h"
#include "pins.h"

#include "input/RotaryEncoder.h"
#include "input/PhotoResistor.h"
#include "input/MotionSensor.h"
#include "input/PushButton.h"
#include "light/LightArray.h"

RotaryEncoder rotary(ROTARY_ENCODER_CLK, ROTARY_ENCODER_DT);
LightArray lights(LED_STRIP_DIN, 8);
PushButton buttonA(BTN_A_PIN, 40);
PhotoResistor ldr(LDR_PIN);
MotionSensor pir(PIR_PIN);

uint8_t configuration = 0x00000000;

enum class Screen {
  Home,         // 0
  Main_Menu,    // 1
  Alarm_Songs,  // 2
  Alarm_Time,   // 3
  Settings,     // 4
};

void setup(){

}

void loop(){

}