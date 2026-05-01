#include <Arduino.h>
#include <QTRSensors.h>
#include "navigation.h"
#include "FreeRTOS.h"
#include "globals.h"
#include "mobility.h"

// TODO: Manually set with previous calibrate data
const uint16_t calMin[LINE_FOLLOWER_LINE_COUNT] = {70, 70, 70, 70};
const uint16_t calMax[LINE_FOLLOWER_LINE_COUNT] = {2500, 2500, 2500, 2500};

QTRSensors qtr;
static uint16_t sensor_values[LINE_FOLLOWER_LINE_COUNT];
void initLineFollower();
//void initJunctionDetect();
#define QTR_1A_THRESHOLD_HIGH_VAL 3700 // 12-bit: 0-4095 -> black
#define QTR_1A_THRESHOLD_LOW_VAL  2800 // 12-bit: 0-4095 -> white
bool junct1 = false; // low (white) at init location
bool junct2 = false; // low (white) at init location

volatile bool swappedHead = false; // swap the head of robot

// ======== PID control ========
static constexpr int LINE_POS_CENTER = 1500;   // 4-sensor QTR readLineBlack(): 0 ~ 3000
static constexpr int PID_ERR_HIST_LEN = 5;

static float g_kr = 0.0f; // 0.005f; //0.001f;

static int g_baseSpeedLeft  = 120;
static int g_baseSpeedRight = 120;
static int g_maxSpeedLeft   = 255;
static int g_maxSpeedRight  = 255;

static int g_lastError = 0;
static int g_errHist[PID_ERR_HIST_LEN] = {0};

static void resetPidState() {
    g_lastError = 0;
    for (int i = 0; i < PID_ERR_HIST_LEN; ++i) {
        g_errHist[i] = 0;
    }
}

static void pushPidError(int err) {
    for (int i = PID_ERR_HIST_LEN - 1; i > 0; --i) {
        g_errHist[i] = g_errHist[i - 1];
    }
    g_errHist[0] = err;
}

static int sumPidErrors(int count, bool useAbs = false) {
    int sum = 0;
    count = min(count, PID_ERR_HIST_LEN);
    for (int i = 0; i < count; ++i) {
        sum += useAbs ? abs(g_errHist[i]) : g_errHist[i];
    }
    return sum;
}

static int clampInt(int x, int lo, int hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

// ========= private variables ==========
//static bool junctionDetect1 = false; // notify on falling edge
//static bool junctionDetect2 = false; // notify on falling edge

// ========= Shared localization values ========= 
volatile static uint16_t linePosition = 0;
volatile static uint16_t filteredLinePos = 1500;

// ========= freeRTOS variables =========
TaskHandle_t localizationTaskHandle = nullptr;
TaskHandle_t pidControlTaskHandle = nullptr;
QueueHandle_t navigationCmdQueue = nullptr;

// FreeRTOS task: periodically read line position
void localizationTask(void *parameter) {
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(LINE_FOLLOWER_POLLING_PERIOD_MS); 
    
    for (;;) {

        linePosition = qtr.readLineBlack(sensor_values);
        filteredLinePos = (filteredLinePos * 4 + linePosition) / 5;
        
        bool detected = false;
        FsmEventQueueItem ev{};
        ev.type = FsmEventType::JunctionCrossed;
        int junct1_adc_val = analogRead(JUNCTION_DETECTOR_PIN_1);
        int junct2_adc_val = analogRead(JUNCTION_DETECTOR_PIN_2);
        if (junct1) { // was high
            // test low threshold
            if (junct1_adc_val < QTR_1A_THRESHOLD_LOW_VAL) {
                // Detect falling edge for sensor 1
                junct1 = false;
                detected = true;
                ev.data.junctionCrossed.left = true;
                DEBUG_LEVEL_2("Junct1 Back to white");
            }
        } else {
            if (junct1_adc_val > QTR_1A_THRESHOLD_HIGH_VAL) {
                junct1 = true;
                DEBUG_LEVEL_2("Junct1 Enter to black");
            }
        }
        if (junct2) { // was high
            // test low threshold
            if (junct2_adc_val < QTR_1A_THRESHOLD_LOW_VAL) {
                // Detect falling edge for sensor 2
                junct2 = false;
                detected = true;
                ev.data.junctionCrossed.right = true;
                DEBUG_LEVEL_2("Junct2 Back to white");
            }
        } else {
            if (junct2_adc_val > QTR_1A_THRESHOLD_HIGH_VAL) {
                junct2 = true;
                DEBUG_LEVEL_2("Junct2 Enter to black");
            }
        }

        if (detected) {
            BaseType_t ok = sendFsmEventItem(ev);
        }
        // Serial.print(",");
        // for (uint8_t i = 0; i < LINE_FOLLOWER_LINE_COUNT; i++) {
        //     Serial.print(sensor_values[i]);
        //     if (i == LINE_FOLLOWER_LINE_COUNT - 1) {
        //         Serial.println();
        //     } else {
        //         Serial.print(",");
        //     }
        // }

        vTaskDelayUntil(&lastWakeTime, period);
    }
}

void pidControlTask(void *parameter) {
    bool activateLineFollower = false;
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t controlPeriod = pdMS_TO_TICKS(LINE_FOLLOWER_POLLING_PERIOD_MS); 
    NavigationCmd cmd{};

    for (;;) {
        if (xQueueReceive(
                navigationCmdQueue, &cmd, 0) == pdPASS) {
            DEBUG_LEVEL_2("Navigation task rcvd a new cmd");

            activateLineFollower = cmd.activateLineFollower;
            if (cmd.activateLineFollower) {
                DEBUG_LEVEL_1("Activate line follower");
                swappedHead = cmd.headingSwapped;
                // TODO: GPIO pin for selecting Line follower
                //digitalWrite(....);

            } else {
                // set motor speed to 0
                setMotorSpeed(0, 0, 0);
                DEBUG_LEVEL_1("Deactivate line follower");
            }
        }

        if (activateLineFollower) {
            // PID line follower logic here
            const int position = static_cast<int>(filteredLinePos);
            const int error = LINE_POS_CENTER - position;

            pushPidError(error);

            const int p = error;
            const int i = sumPidErrors(5, false);
            const int d = error - g_lastError;
            const int r = sumPidErrors(5, true);
            g_lastError = error;

            const float correction = g_kp * p + g_ki * i + g_kd * d;

            int leftCmd  = static_cast<int>(g_baseSpeedLeft  + correction);
            int rightCmd = static_cast<int>(g_baseSpeedRight - correction);

            leftCmd  = clampInt(leftCmd,  0, g_maxSpeedLeft);
            rightCmd = clampInt(rightCmd, 0, g_maxSpeedRight);

            setMotorSpeed(leftCmd, rightCmd, swappedHead);
        } else {

        }
        vTaskDelayUntil(&lastWakeTime, controlPeriod);
    }
}

void initLocalizationTask() {

    //initJunctionDetect();
    initLineFollower();

    // Create FreeRTOS task
    xTaskCreatePinnedToCore(
        localizationTask,        // Task function
        "LocalizationTask",      // Task name
        4096,                // Stack size
        NULL,                // Parameter
        2,                   // Priority
        &localizationTaskHandle, // Task handle
        0                    // Core (0 or 1)
    );
}

void initPIDControlTask() {
    xTaskCreatePinnedToCore(
        pidControlTask,
        "PIDControlTask",
        4096,
        NULL,
        2,
        &pidControlTaskHandle,
        0
    );
}

void initNavigation() {
    navigationCmdQueue = xQueueCreate(10, sizeof(NavigationCmd));
    if (navigationCmdQueue == NULL) {
        DEBUG_LEVEL_1("Navigation command queue creation failed");
    }

    initLocalizationTask();
    initPIDControlTask();
}

// =============================================
//              Helpers
// =============================================
// void initJunctionDetect() {

// }

void initLineFollower() {
    qtr.setTypeRC();
    qtr.setSensorPins((const uint8_t[]){
      LINE_FOLLOWER_PIN_1, LINE_FOLLOWER_PIN_2, 
      LINE_FOLLOWER_PIN_3, LINE_FOLLOWER_PIN_4}, LINE_FOLLOWER_LINE_COUNT);

#if MANUAL_CALIBRATE_LINE_FOLLOWER
    // Ensure calibration arrays exist
    qtr.calibrate();
    // Overwrite calibration data
    for (uint8_t i = 0; i < LINE_FOLLOWER_LINE_COUNT; i++) {
      qtr.calibrationOn.minimum[i] = calMin[i];
      qtr.calibrationOn.maximum[i] = calMax[i];
    }

    Serial.println("Manual calibration loaded.");

    Serial.print("Min: ");
    for (uint8_t i = 0; i < LINE_FOLLOWER_LINE_COUNT; i++) {
      Serial.print(qtr.calibrationOn.minimum[i]);
      Serial.print(' ');
    }
    Serial.println();

    Serial.print("Max: ");
    for (uint8_t i = 0; i < LINE_FOLLOWER_LINE_COUNT; i++) {
      Serial.print(qtr.calibrationOn.maximum[i]);
      Serial.print(' ');
    }
    Serial.println();
#else
    Serial.println("calibrating line follower...");
    // Calibration
    for (uint16_t i = 0; i < 30; i++) {
        qtr.calibrate();
    }

    // Print calibration minimum values
    for (uint8_t i = 0; i < LINE_FOLLOWER_LINE_COUNT; i++) {
        Serial.print(qtr.calibrationOn.minimum[i]);
        Serial.print(' ');
    }
    Serial.println();

    // Print calibration maximum values
    for (uint8_t i = 0; i < LINE_FOLLOWER_LINE_COUNT; i++) {
        Serial.print(qtr.calibrationOn.maximum[i]);
        Serial.print(' ');
    }
    Serial.println();
    Serial.println();
#endif

}

bool sendNavigationCmd(NavigationCmd cmd) {
    if (navigationCmdQueue == nullptr) {
        DEBUG_LEVEL_1("Navigation command queue not initialized");
        return false;
    }
    return xQueueSend(navigationCmdQueue, &cmd, 0) == pdPASS;
}
