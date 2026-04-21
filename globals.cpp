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

const char* stateToString(RobotState s) {
    switch (s) {
        case RobotState::Idle:           return "Idle";
        case RobotState::Started:          return "Started";
        case RobotState::CheckHillLoyalty:     return "CheckHillLoyalty";
        case RobotState::LaunchingBall:        return "LaunchingBall";
        case RobotState::ForceStopped:    return "ForceStopped";
        case RobotState::ManualControl:   return "ManualControl";
        case RobotState::Error: return "Error";
        default:                         return "Unknown";
    }
}

bool stringToState(const char* str, RobotState& out) {
    if (strcmp(str, "Idle") == 0) {
        out = RobotState::Idle;
        return true;
    }
    if (strcmp(str, "Started") == 0) {
        out = RobotState::Started;
        return true;
    }
    if (strcmp(str, "CheckHillLoyalty") == 0) {
        out = RobotState::CheckHillLoyalty;
        return true;
    }
    if (strcmp(str, "LaunchingBall") == 0) {
        out = RobotState::LaunchingBall;
        return true;
    }
    if (strcmp(str, "ForceStopped") == 0) {
        out = RobotState::ForceStopped;
        return true;
    }
    if (strcmp(str, "ManualControl") == 0) {
        out = RobotState::ManualControl;
        return true;
    }
    if (strcmp(str, "Error") == 0) {
        out = RobotState::Error;
        return true;
    }
    return false;
}

const char* teamToString(RobotTeam s) {
    switch (s) {
        case RobotTeam::Red:           return "Red";
        case RobotTeam::Blue:          return "Blue";
        default:                       return "Unknown";
    }
}

const char* beaconStateToString(BeaconState s) {
    switch (s) {
        case BeaconState::Unknown:       return "Unknown";
        case BeaconState::Beacon750:     return "Beacon750";
        case BeaconState::Beacon1k5:     return "Beacon1k5";
        default:                         return "Unknown";
    }
}
