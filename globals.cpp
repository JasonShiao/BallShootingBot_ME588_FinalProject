#include "globals.h"
#include <Arduino.h>

QueueHandle_t g_fsmNotifQueue[NUM_TASK] = {nullptr}; // game_status, team_status

QueueHandle_t g_fsmEventQueue = nullptr;

void initQueues() {
    g_fsmEventQueue = xQueueCreate(EVENT_QUEUE_SIZE, sizeof(FsmEventQueueItem));

    for (size_t i = 0; i < NUM_TASK; ++i) {
        g_fsmNotifQueue[i] = xQueueCreate(NOTIF_QUEUE_SIZE, sizeof(FsmNotifQueueItem));
    }

    if(g_fsmEventQueue == NULL) {
        /* One or more queues were not created successfully as there was not enough
           heap memory available. Handle the error here. */
        DEBUG_LEVEL_1("Fsm Event queue created failed");
    }

    for (int i = 0; i < NUM_TASK; i++) {
        if (g_fsmNotifQueue[i] == NULL) {
            DEBUG_LEVEL_1("Fsm Notif queue created failed");
        }
    }
}
