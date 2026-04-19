#include "globals.h"
#include <Arduino.h>

QueueHandle_t g_FsmNotifQueue[2] = {nullptr}; // game_status, team_status

QueueHandle_t g_FsmEventQueue = nullptr;

void initQueues() {
    g_FsmEventQueue = xQueueCreate(10, sizeof(FsmEventQueueItem));

    for (size_t i = 0; i < NUM_TASK; ++i) {
        g_FsmNotifQueue[i] = xQueueCreate(10, sizeof(FsmNotifQueueItem));
    }

    if( ( g_FsmEventQueue == NULL ))
    {
        /* One or more queues were not created successfully as there was not enough
           heap memory available. Handle the error here. Queues can also be created
           statically. */
        Serial.println("Fsm Event queue created failed");
    }

    for (int i = 0; i < NUM_TASK; i++) {
        if (g_FsmNotifQueue[i] == NULL) {
            Serial.println("Fsm Notif queue created failed");
        }
    }
}
