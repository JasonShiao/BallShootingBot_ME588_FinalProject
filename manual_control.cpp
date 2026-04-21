#include "manual_control.h"
#include <Arduino.h>
#include "FreeRTOS.h"
#include "globals.h"
#include "mobility.h"

constexpr size_t MANUAL_CONTROL_QUEUE_SIZE = 10;
QueueHandle_t g_manualControlQueue = nullptr; // send for manual control task only
bool isManualControlMode = false;

TaskHandle_t manualControlTaskHandle = NULL;
void manualControlTask(void *parameter);
static QueueSetHandle_t xManualControlQueueSet;

void initManualControl() {
    // Prepare freeRTOS queue and queue set
    g_manualControlQueue = xQueueCreate(MANUAL_CONTROL_QUEUE_SIZE, sizeof(ManualControlMotionQueueItem));
    xManualControlQueueSet = xQueueCreateSet(MANUAL_CONTROL_QUEUE_SIZE + NOTIF_QUEUE_SIZE);
    xQueueAddToSet(g_fsmNotifQueue[toIndex(TaskId::ManualControl)], xManualControlQueueSet);
    xQueueAddToSet(g_manualControlQueue, xManualControlQueueSet);

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
    FsmNotifQueueItem notif_item;
    QueueSetMemberHandle_t xActivatedMember;
    ManualControlMotionQueueItem motion_item;

    for (;;) {
        // Block Wait for any queue in the queue set non empty
        xActivatedMember = xQueueSelectFromSet(xManualControlQueueSet, portMAX_DELAY);

        if (xActivatedMember == g_manualControlQueue) {
            xQueueReceive(xActivatedMember, &motion_item, 0);

            DEBUG_LEVEL_2("motion cmd rcvd by ManualControl");

            if (isManualControlMode) {
                // TODO: set speed ...
                switch (motion_item.data.dir) {
                    case MotionDir::Forward:
                        setMotorSpeed(230, 230);
                        break;
                    case MotionDir::ForwardLeft:
                        setMotorSpeed(180, 230);
                        break;
                    case MotionDir::ForwardRight:
                        setMotorSpeed(230, 180);
                        break;
                    case MotionDir::Backward:
                        setMotorSpeed(-230, -230);
                        break;
                    case MotionDir::BackwardLeft:
                        setMotorSpeed(-180, -230);
                        break;
                    case MotionDir::BackwardRight:
                        setMotorSpeed(-230, -180);
                        break;
                    case MotionDir::RotateCW:
                        setMotorSpeed(150, -150);
                        break;
                    case MotionDir::RotateCCW:
                        setMotorSpeed(-150, 150);
                        break;
                    default:
                        break;
                }
            }

        } else if (xActivatedMember == g_fsmNotifQueue[toIndex(TaskId::ManualControl)]) {
            xQueueReceive(
              g_fsmNotifQueue[toIndex(TaskId::ManualControl)], &notif_item, 0);
            
            DEBUG_LEVEL_2("notif from fsm rcvd by ManualControl");

            switch (notif_item.type) {
                case FsmNotifType::StateChanged:
                    //DEBUG_LEVEL_1("IR beacon count: %d", count);
                    if (notif_item.data.state == RobotState::ManualControl) {
                        isManualControlMode = true;
                    } else {
                        // stop motors
                        setMotorSpeed(0, 0);
                        isManualControlMode = false;
                    }
                    break;
                default:
                    break;
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
