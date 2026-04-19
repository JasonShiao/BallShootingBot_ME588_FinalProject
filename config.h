#ifndef CONFIG_H
#define CONFIG_H

#define DEBUG_LEVEL 1

#define DEBUG_LEVEL_1(...) do { if (DEBUG_LEVEL >= 1) Serial.printf(__VA_ARGS__); } while(0)
#define DEBUG_LEVEL_2(...) do { if (DEBUG_LEVEL >= 2) Serial.printf(__VA_ARGS__); } while(0)
#define DEBUG_LEVEL_3(...) do { if (DEBUG_LEVEL >= 3) Serial.printf(__VA_ARGS__); } while(0)


#endif 