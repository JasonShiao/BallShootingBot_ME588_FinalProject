#include <Arduino.h>
#include <QTRSensors.h>
#include "navigation.h"
#include "FreeRTOS.h"
#include "globals.h"
#include "mobility.h"

// TODO: Manually set with previous calibrate data
const uint16_t calMin[LINE_FOLLOWER_LINE_COUNT] = {123, 110, 118, 130};
const uint16_t calMax[LINE_FOLLOWER_LINE_COUNT] = {820, 790, 845, 810};

QTRSensors qtr;
static uint16_t sensor_values[LINE_FOLLOWER_LINE_COUNT];
void initLineFollower();
//void initJunctionDetect();
//#define QTR_1A_THRESHOLD_VAL 500
volatile bool swappedHead = false; // swap the head of robot

// ======== PID control ========
static constexpr int LINE_POS_CENTER = 1500;   // 4-sensor QTR readLineBlack(): 0 ~ 3000
static constexpr int PID_ERR_HIST_LEN = 5;

static float g_kp = 0.08f;
static float g_ki = 0.0f;
static float g_kd = 0.18f;

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
volatile static uint16_t filteredLinePos = 0;

// ========= freeRTOS variables =========
TaskHandle_t localizationTaskHandle = nullptr;
TaskHandle_t pidControlTaskHandle = nullptr;
QueueHandle_t navigationCmdQueue = nullptr;

// FreeRTOS task: periodically read line position
void localizationTask(void *parameter) {
    TickType_t lastWakeTime = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(LINE_FOLLOWER_POLLING_PERIOD_MS); 
    
    //FsmNotifQueueItem notif_item;

    for (;;) {
        // receive notification (non blocking)
        // if (xQueueReceive(
        //         g_fsmNotifQueue[toIndex(TaskId::Localization)], 
        //         &notif_item, 0) == pdPASS) {
            
        //     DEBUG_LEVEL_2("notif from fsm rcvd by Localization");

        //     switch (notif_item.type) {
        //         case FsmNotifType::StateChanged:
        //             currState = notif_item.data.state;
        //             if (!isGameStartedState(currState)) {
        //                 junctionDetect1 = false;
        //                 junctionDetect2 = false;
        //             }
        //             break;
        //         default:
        //             break;
        //     }

        // }

        linePosition = qtr.readLineBlack(sensor_values);
        filteredLinePos = (filteredLinePos * 4 + linePosition) / 5;
        
        // if (isRobotMoving(currState)) {
        //     bool newJunctionDetect1 = analogRead(JUNCTION_DETECTOR_PIN_1) > QTR_1A_THRESHOLD_VAL;
        //     bool newJunctionDetect2 = analogRead(JUNCTION_DETECTOR_PIN_2) > QTR_1A_THRESHOLD_VAL;
        //     // Detect falling edge for sensor 1
        //     bool detected = false;
        //     FsmEventQueueItem ev{};
        //     ev.type = FsmEventType::JuncionDetected;
        //     if (junctionDetect1 && !newJunctionDetect1) {
        //         detected = true;
        //         ev.data.junctionState.sensor[0] = true;
        //     }
        //     junctionDetect1 = newJunctionDetect1;
        //     // Detect falling edge for sensor 2
        //     if (junctionDetect2 && !newJunctionDetect2) {
        //         detected = true;
        //         ev.data.junctionState.sensor[1] = true;
        //     }
        //     junctionDetect2 = newJunctionDetect2;
        //     if (detected) {
        //         BaseType_t ok = xQueueSend(g_fsmEventQueue, &ev, 0);
        //     }
        // }
        // Serial.print(linePosition);
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
