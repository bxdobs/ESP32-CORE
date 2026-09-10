#ifndef SECRETS_H
   #define SECRETS_H
   
   // WiFi credentials
   #define _WSSID    "ABCDEF" 
   #define _WABC     "1234567890ZZ"
   
   // URL config values
   #define _OTA_HOST "ota-esp32"      // host required for OTA mode from the Arduino IDE
   #define _ABC      "ABC@123456"     // password suffix
   #define _CMD_PRFX "cmd@"           // command password prefix
   #define _CMD_ABC _CMD_PRFX _ABC    // command server password
   #define _OTA_PRFX "ota@"           // OTA password prefix
   #define _OTA_ABC _OTA_PRFX _ABC    // OTA password
#endif

// =============================
// ESP32 Hardware Timer Defaults
// =============================

// APB clock is fixed at 80 MHz on ALL ESP32 variants
#define _TMR_PRESCALE        80   // 80 MHz / 80 = 1 MHz timer clock
#define _TMR_TICK             1   // 1 MHz clock → 1 µS per tick

// =========================
// Desired timing model
// =========================

// 500 ms period → 500,000 µs → 500,000 ticks at 1 µS / tick
#define _TMR_PERIOD     500000UL  // 500 mS "tock"

#define _LOG_MSG_MAXLEN      128  // max length of each message
#define _LOG_QSIZE           200  // number of messages that can be queued

#define _MQTT_TOPIC_MAXLEN    64  // max length of each message
#define _MQTT_PAYLOAD_MAXLEN 128  // max length of each message
#define _MQTT_LOG_QSIZE      200  // number of messages that can be queued
#define _MQTT_QSIZE           16  // number of messages that can be queued

#define _SYS_CMD_QSIZE        16  
#define _APPQ_QSIZE           10
#define _APPQ_BUF_MAXLEN     128
#define _ECCQ_QSIZE           10
#define _ECCQ_BUF_MAXLEN     128
