#pragma once

// =================== PINS =================== //
#define R_PIN D7
#define G_PIN D6
#define B_PIN D8

#define S1_PIN D4
#define S2_PIN D2
#define KEY_PIN D5

#define IR_PIN D1
#define INDICATOR_PIN D3

// =================== OTHER =================== //
#define MODE_AMOUNT 3

// =================== WI-FI =================== //
#define SSID "Kerosinovka"
#define WIFI_PASSWORD "c588tb36"
#define MAX_CLIENTS 5

#define UDP_PORT 1679
#define KEEP_ALIVE_SEND 10000
#define KEEP_ALIVE_RESPOSE 1000
#define SYNC_DELAY 500

#define RECONNECT_DELAY 10000

#define HEADER_LENGTH 3

// =================== FIRE =================== //
#define SMOOTH_K 0.15   // коэффициент плавности огня
#define MIN_BRIGHT 80   // мин. яркость огня
#define MAX_BRIGHT 255  // макс. яркость огня
#define MIN_SAT 255     // мин. насыщенность
#define MAX_SAT 255     // макс. насыщенность

// =================== DEBUG =================== //
#define DEBUG_EN 1

#if (DEBUG_EN)
#define DBG_PRINT(x)          (Serial.print(x))
#define DBG_PRINTLN(x)        (Serial.println(x))
#define DBG_UDP(x, size)      (udpSend(x, size, IPAddress(192,168,1,2)))
#else
#define DBG_PRINT(x)
#define DBG_PRINTLN(x)
#define DBG_UDP(x, size)
#endif
