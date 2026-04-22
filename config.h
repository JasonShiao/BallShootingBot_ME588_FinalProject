#ifndef CONFIG_H
#define CONFIG_H

#define DEBUG_LEVEL 1

// Wifi config
#define SOFT_AP_MODE 0
#if SOFT_AP_MODE
#define WIFI_SSID "me588_team10"
#define WIFI_PSWD "asdf1234"
#else
#define WIFI_SSID "Linksys02714"
#define WIFI_PSWD "xxxxxxxxxx"
#endif
// Servo config
#define SERVO_MAX_ANGLE 180 // 270
// IR beacon detector config
#define IR_BEACON_DETECT_PERIOD_MS 80 // 80ms -> 750Hz ~ 60 pulses; 1500Hz ~ 120 pulses // 60ms -> 750Hz ~ 45 pulses; 1500Hz ~ 90 pulses
#define IR_BEACON_FREQ_PERIODIC_REPORT_PERIOD 1000 // 1 per second
// Line follower config
#define MANUAL_CALIBRATE_LINE_FOLLOWER 1
#define LINE_FOLLOWER_POLLING_PERIOD_MS 5 // 5 ms for actual run // 200ms for demo and testing only

//#define FULLY_AUTONOMOUS 1

#define DEBUG_LEVEL_1(fmt, ...) \
    do { if (DEBUG_LEVEL >= 1) Serial.printf(fmt "\n", ##__VA_ARGS__); } while(0)

#define DEBUG_LEVEL_2(fmt, ...) \
    do { if (DEBUG_LEVEL >= 2) Serial.printf(fmt "\n", ##__VA_ARGS__); } while(0)

#define DEBUG_LEVEL_3(fmt, ...) \
    do { if (DEBUG_LEVEL >= 3) Serial.printf(fmt "\n", ##__VA_ARGS__); } while(0)

#endif 

