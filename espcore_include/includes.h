// Includes
// Note <> looks in pio lib folders ... "" looks in project folders first then pio lib folders
// C C++ Core
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
//#include <vector>
// Arduino Core
#include <Arduino.h>
// Arduino OTA Core
#include <ArduinoOTA.h>   
#include <fnmatch.h>
// Esp32 FreeRTOS (Real Time OS) Core
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
// Esp32 System Core
#include <esp_system.h>
#include <driver/gpio.h>
#include <esp_timer.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <Preferences.h>
#include <esp_task_wdt.h>      // DO NOT USE Watchdog in OTA mode
// Esp32 File System
#include <LittleFS.h>
// Networking
#include <WiFi.h>
// Crypto
#include "mbedtls/sha256.h"
// Web Core 
#include <ESPAsyncWebServer.h>
// Json Core
#include <ArduinoJson.h>
// MQTT Client
#include <PubSubClient.h>
// Hardware helper
#include <Bounce2.h>
#include <Adafruit_NeoPixel.h>
