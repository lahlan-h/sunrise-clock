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

const char* INFLUX_HOST = "https://eu-central-1-1.aws.cloud2.influxdata.com";
const char* INFLUX_ORG = "";
const char* INFLUX_BUCKET = "sunrise-clock-bucket";
const char* INFLUX_API_TOKEN = "";
