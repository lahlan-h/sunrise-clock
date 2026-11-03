#pragma once

#include <Arduino.h>

const char* WIFI_SSID     = "your-home-network";
const char* WIFI_PASSWORD = "your-home-network-password";

const char* BLE_ADVERTISER_NAME = "your-ble-advertiser-name";

const char* MQTT_HOST = "your-mqtt-broker";
const uint16_t MQTT_PORT = 1883;
const char* MQTT_USERNAME = "";
const char* MQTT_PASSWORD = "";
const char* MQTT_DEVICE_ID = "sunrise-clock-01";
const bool MQTT_USE_TLS = false;
