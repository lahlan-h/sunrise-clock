#pragma once

#include <Arduino.h>

// - - - - - LEDS - - - - -
const uint8_t LED_STRIP_DIN       = 4;

// - - - - - INPUT - - - - -
const uint8_t BTN_A_PIN           = 13;

const uint8_t ROTARY_ENCODER_CLK  = 32;
const uint8_t ROTARY_ENCODER_DT   = 33;

// - - - - - SENSORS - - - - -
const uint8_t LDR_PIN             = 34; // ADC1, so Wi-Fi doesn't interfere
const uint8_t PIR_PIN             = 15;
const uint8_t DHT_PIN             = 18;
const uint8_t MP3_RX_PIN          = 26;
const uint8_t MP3_TX_PIN          = 27;

// - - - - - OLED - - - - -
const uint8_t OLED_SDA_PIN        = 21;
const uint8_t OLED_SCL_PIN        = 22;