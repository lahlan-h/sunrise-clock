#include <Arduino.h>
#include "Rotary.h"

static const int8_t table[16] = {
   0, -1,  1,  0,
   1,  0,  0, -1,
   -1,  0,  0,  1,
   0,  1, -1,  0,
};

RotaryEncode::RotaryEncode(uint8_t clkPin, uint8_t dtPin){

}