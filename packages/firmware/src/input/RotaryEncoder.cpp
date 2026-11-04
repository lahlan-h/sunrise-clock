#include "RotaryEncoder.h"

// Bounce cancels itself out (+1 then -1), and impossible jumps (both pins changing) count as 0
static const int8_t directionForTransition[16] = {
  // to: 00  01  10  11
          0, -1,  1,  0,  // from 00
          1,  0,  0, -1,  // from 01
         -1,  0,  0,  1,  // from 10
          0,  1, -1,  0,  // from 11
};

// A KY-040 passes through throuigh all 4 pin states per click
static const int32_t transitionsPerClick = 4;

// Pull-ups hold both pins HIGH while the knob sits in a click
static const uint8_t restingPinState = 0b11;

RotaryEncoder::RotaryEncoder(uint8_t clkPin, uint8_t dtPin)
: clkPin(clkPin), dtPin(dtPin) {}

void RotaryEncoder::begin(){
  pinMode(clkPin, INPUT_PULLUP);
  pinMode(dtPin, INPUT_PULLUP);

  previousPinState = readPinState();
}

void RotaryEncoder::update(){
  clicksSinceLastUpdate = 0;

  uint8_t currentPinState = readPinState();
  uint8_t transitionIndex = (previousPinState << 2) | currentPinState;

  pendingTransitions += directionForTransition[transitionIndex];
  previousPinState = currentPinState;
  
  // Only count once the knob settles into a click, so contact bounce can't flicker the count
  if (currentPinState == restingPinState){
    clicksSinceLastUpdate = pendingTransitions / transitionsPerClick;
    clicksSinceReset += clicksSinceLastUpdate;
    pendingTransitions = 0;
  }
}

void RotaryEncoder::reset(){
  pendingTransitions = 0;
  clicksSinceReset = 0;
  clicksSinceLastUpdate = 0;
}

int32_t RotaryEncoder::getClicksSinceReset(){
  return clicksSinceReset;
}

int32_t RotaryEncoder::getClicksSinceLastUpdate(){
  return clicksSinceLastUpdate;
}

uint8_t RotaryEncoder::readPinState(){
  return (digitalRead(clkPin) << 1) | digitalRead(dtPin);
}