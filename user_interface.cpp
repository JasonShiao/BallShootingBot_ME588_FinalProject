#include "user_interface.h"
#include "Arduino.h"
#include "FreeRTOS.h"
#include "globals.h"

void userIntefaceTask(void *parameter);
TaskHandle_t userInterfaceTaskHandle = NULL;

void initUserInterface() {
    xTaskCreatePinnedToCore(
        userIntefaceTask,      // Task function
        "UserIntefaceTask",    // Task name
        4096,                // Stack size (bytes)
        NULL,                // Parameters
        1,                   // Priority
        &userInterfaceTaskHandle,  // Task handle
        1                  // Core 1
    );
}

void userIntefaceTask(void *parameter) {
    char rcvdChar;
    for (;;) {
        if (Serial.available() > 0) {
            rcvdChar = Serial.read();
            if (rcvdChar == 'b') {
                DEBUG_LEVEL_1("Force ball launching...");
                FsmEventQueueItem ev{};
                ev.type = FsmEventType::UserStateChangeReq;
                ev.data.newState = RobotState::LaunchingBall;
                BaseType_t ok = xQueueSend(g_fsmEventQueue, &ev, 0);
            } else if (rcvdChar != '\n') {
              DEBUG_LEVEL_1("Rcvd: %c", rcvdChar);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}