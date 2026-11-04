#include "LightArray.h"

LightArray::LightArray(uint8_t pin, uint16_t count)
: strip(count, pin, NEO_GRB + NEO_KHZ800) {}

void LightArray::begin(){
  strip.begin();
  strip.setBrightness(100);
  off();
}

void LightArray::applyControl(uint8_t brightnessPercent, LightMode mode) {
  if (brightnessPercent > 100) brightnessPercent = 100;

  this->brightnessPercent = brightnessPercent;
  strip.setBrightness(
      static_cast<uint8_t>((static_cast<uint16_t>(brightnessPercent) * 255) / 100));

  switch (mode) {
    case LightMode::Off:
      off();
      break;
    case LightMode::Steady:
      running = false;
      on = brightnessPercent > 0;
      strip.fill(strip.Color(255, 160, 40));
      strip.show();
      break;
    case LightMode::Sunrise:
      startSunrise(30UL * 60UL * 1000UL);
      break;
  }
}

void LightArray::update(){
  if(!running) return;

  float progress = (millis() - startMs) / float(durationMs);
  if(progress >= 1.0){ progress = 1.0; running = false; }

  strip.fill(sunriseColour(progress));
  strip.show();
  on = true;
}

void LightArray::startSunrise(unsigned long ms){
  durationMs = ms;
  startMs = millis();
  running = true;
  on = true; // Report on straight away, not only after the first update()
}

void LightArray::off(){
  running = false;
  on = false;
  strip.clear();
  strip.show();
}

bool LightArray::isRunning(){
  return running;
}

bool LightArray::isOn() const {
  return on;
}

uint8_t LightArray::getBrightnessPercent() const {
  return on ? brightnessPercent : 0;
}

LightMode LightArray::getMode() const {
  return running ? LightMode::Sunrise : (on ? LightMode::Steady : LightMode::Off);
}

uint8_t lerp(uint8_t a, uint8_t b, float t) {
  return a + (b - a) * t;
}

uint32_t LightArray::sunriseColour(float progress) {
  if (progress < 0.5) {
    float t = progress / 0.5;
    return strip.Color(lerp(40, 255, t), lerp(0, 100, t), 0);
  }
  float t = (progress - 0.5) / 0.5;
  return strip.Color(255, lerp(100, 200, t), lerp(0, 120, t));
}