#include "RotaryEncoder.h"

// A KY-040 passes through all 4 pin states per click
static const int32_t transitionsPerClick = 4;
static const uint8_t ROTARY_TURN_EFFECT = 2;

RotaryEncoder::RotaryEncoder(uint8_t clkPin, uint8_t dtPin)
: clkPin(clkPin), dtPin(dtPin) {}

void RotaryEncoder::begin(){
  // The library defaults to pull-downs, which would fight the KY-040's pull-ups
  ESP32Encoder::useInternalWeakPullResistors = puType::up;

  // DT first, so turning clockwise counts up
  pulseCounter.attachFullQuad(dtPin, clkPin);
}

void RotaryEncoder::update(){
  // Truncating drops a half-turned click until the knob settles into it
  int32_t clicksNow = pulseCounter.getCount() / transitionsPerClick;

  clicksSinceLastUpdate = clicksNow - clicksSinceReset;
  clicksSinceReset = clicksNow;

  if (clicksSinceLastUpdate != 0) {
    mp3.playEffect(ROTARY_TURN_EFFECT);
  }
}

void RotaryEncoder::reset(){
  pulseCounter.clearCount();
  clicksSinceReset = 0;
  clicksSinceLastUpdate = 0;
}

int32_t RotaryEncoder::getClicksSinceReset(){
  return clicksSinceReset;
}

int32_t RotaryEncoder::getClicksSinceLastUpdate(){
  return clicksSinceLastUpdate;
}
