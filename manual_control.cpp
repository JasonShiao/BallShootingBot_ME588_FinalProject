#include "manual_control.h"
#include <Arduino.h>
#include "FreeRTOS.h"
#include "globals.h"
#include "mobility.h"

constexpr size_t MANUAL_CONTROL_QUEUE_SIZE = 10;
bool manualControlEnabled = false;

static TaskHandle_t manualControlTaskHandle = NULL;
static QueueHandle_t manualControlCmdQueue = nullptr; // send for manual control task only

void manualControlTask(void *parameter);
static QueueSetHandle_t xManualControlQueueSet;

void initManualControl() {
    // Prepare freeRTOS queue and queue set
    manualControlCmdQueue = xQueueCreate(MANUAL_CONTROL_QUEUE_SIZE, sizeof(ManualControlCmd));
    if (manualControlCmdQueue == NULL) {
        DEBUG_LEVEL_1("Manual control cmd queue created failed");
    }

    // Create software timer for periodic report
    // periodicReportTimerHandle = xTimerCreate( 
    //     "PeriodicReportTimerHandle",
    //     pdMS_TO_TICKS(IR_BEACON_FREQ_PERIODIC_REPORT_PERIOD),
    //     pdTRUE,
    //     (void *) 0,
    //     onPeriodicReportTimeoutCallback);

    xTaskCreatePinnedToCore(
        manualControlTask,      // Task function
        "ManualControlTask",    // Task name
        2048,                // Stack size (bytes)
        NULL,                // Parameters
        1,                   // Priority
        &manualControlTaskHandle,  // Task handle
        1                  // Core 1
    );
}


void manualControlTask(void *parameter) {
    ManualControlCmd cmd;

    for (;;) {
        // Block Wait for any queue in the queue set non empty
        if (xQueueReceive(manualControlCmdQueue, &cmd, 0) == pdTRUE) {
            DEBUG_LEVEL_2("Cmd rcvd by ManualControl");
            if (cmd.type == ManualControlCmdType::CtrlCmd) {
                manualControlEnabled = cmd.data.enable;
                DEBUG_LEVEL_1("Manual control %s", manualControlEnabled ? "enabled" : "disabled");
                if (!manualControlEnabled) {
                    setMotorSpeed(0, 0, false);
                }
            } else if (cmd.type == ManualControlCmdType::MotionCmd) {
                DEBUG_LEVEL_1("Motion cmd rcvd: dir=%d, speed=%d", 
                    cmd.data.motion.dir, cmd.data.motion.speed);
                if (!manualControlEnabled) {
                    // ignore
                    continue;
                }
                switch (cmd.data.motion.dir) {
                    case MotionDir::Forward:
                        setMotorSpeed(230, 230, false);
                        break;
                    case MotionDir::ForwardLeft:
                        setMotorSpeed(180, 230, false);
                        break;
                    case MotionDir::ForwardRight:
                        setMotorSpeed(230, 180, false);
                        break;
                    case MotionDir::Backward:
                        setMotorSpeed(-230, -230, false);
                        break;
                    case MotionDir::BackwardLeft:
                        setMotorSpeed(-180, -230, false);
                        break;
                    case MotionDir::BackwardRight:
                        setMotorSpeed(-230, -180, false);
                        break;
                    case MotionDir::RotateCW:
                        setMotorSpeed(150, -150, false);
                        break;
                    case MotionDir::RotateCCW:
                        setMotorSpeed(-150, 150, false);
                        break;
                    default:
                        break;
                }
            }
        }
    }
}


bool stringToDir(const char* str, MotionDir& dir) {
    if (strcmp(str, "Forward") == 0) {
        dir = MotionDir::Forward;
        return true;
    }
    if (strcmp(str, "ForwardRight") == 0) {
        dir = MotionDir::ForwardRight;
        return true;
    }
    if (strcmp(str, "ForwardLeft") == 0) {
        dir = MotionDir::ForwardLeft;
        return true;
    }
    if (strcmp(str, "Backward") == 0) {
        dir = MotionDir::Backward;
        return true;
    }
    if (strcmp(str, "BackwardRight") == 0) {
        dir = MotionDir::BackwardRight;
        return true;
    }
    if (strcmp(str, "BackwardLeft") == 0) {
        dir = MotionDir::BackwardLeft;
        return true;
    }
    if (strcmp(str, "RotateCW") == 0) {
        dir = MotionDir::RotateCW;
        return true;
    }
    if (strcmp(str, "RotateCCW") == 0) {
        dir = MotionDir::RotateCCW;
        return true;
    }
    return false;
}

bool sendManualControlCmd(const ManualControlCmd& item) {
    if (manualControlCmdQueue == nullptr) {
        DEBUG_LEVEL_1("Manual control cmd queue not initialized");
        return false;
    }
    return xQueueSend(manualControlCmdQueue, &item, 0) == pdPASS;
}
