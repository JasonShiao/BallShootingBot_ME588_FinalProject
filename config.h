#ifndef CONFIG_H
#define CONFIG_H

#define DEBUG_LEVEL 1
#define WIFI_SSID "Linksys02714"
#define WIFI_PSWD "6t81ha6a0h"
#define SERVO_MAX_ANGLE 180 // 270
#define IR_BEACON_DETECT_PERIOD_MS 60 // 60ms -> 750Hz ~ 45 pulses; 1500Hz ~ 90 pulses
#define IR_BEACON_FREQ_PERIODIC_REPORT_PERIOD 1000 // 1 per second

#define DEBUG_LEVEL_1(fmt, ...) \
    do { if (DEBUG_LEVEL >= 1) Serial.printf(fmt "\n", ##__VA_ARGS__); } while(0)

#define DEBUG_LEVEL_2(fmt, ...) \
    do { if (DEBUG_LEVEL >= 2) Serial.printf(fmt "\n", ##__VA_ARGS__); } while(0)

#define DEBUG_LEVEL_3(fmt, ...) \
    do { if (DEBUG_LEVEL >= 3) Serial.printf(fmt "\n", ##__VA_ARGS__); } while(0)

#endif 

