#include "globals.h"
#include <Arduino.h>

//QueueHandle_t g_fsmNotifQueue[NUM_TASK] = {nullptr}; // game_status, team_status

QueueHandle_t g_fsmEventQueue = nullptr;

void initQueues() {
    g_fsmEventQueue = xQueueCreate(EVENT_QUEUE_SIZE, sizeof(FsmEventQueueItem));

    //for (size_t i = 0; i < NUM_TASK; ++i) {
    //    g_fsmNotifQueue[i] = xQueueCreate(NOTIF_QUEUE_SIZE, sizeof(FsmNotifQueueItem));
    //}

    if(g_fsmEventQueue == NULL) {
        /* One or more queues were not created successfully as there was not enough
           heap memory available. Handle the error here. */
        DEBUG_LEVEL_1("Fsm Event queue created failed");
    }

    //for (int i = 0; i < NUM_TASK; i++) {
    //    if (g_fsmNotifQueue[i] == NULL) {
    //        DEBUG_LEVEL_1("Fsm Notif queue created failed");
    //    }
    //}
}

bool isGameStartedState(RobotState s) {
    switch (s) {
        case RobotState::Idle:
            return false;
        case RobotState::MoveToNextJunction:
        case RobotState::CheckHillLoyalty:
        case RobotState::BallLaunching:
        case RobotState::WaitLoyaltyChange:
        case RobotState::BackHome:
        case RobotState::WaitBallReload:
            return true;
        // ====== Special states =====
        case RobotState::ForceStopped:
        case RobotState::ManualControl:
        case RobotState::Error:
            return false;
        default:
            return false;
    }
}

/**
 * Moving (autonomously), i.e. Excluding manual control
 */
bool isRobotMoving(RobotState s) {
    switch (s) {
        case RobotState::MoveToNextJunction:
        case RobotState::BackHome:
            return true;
        default:
            return false;
    }
}

const char* stateToString(RobotState s) {
    switch (s) {
        case RobotState::Idle:            return "Idle";
        case RobotState::MoveToNextJunction:   return "MoveToNextJunction";
        case RobotState::CheckHillLoyalty:     return "CheckHillLoyalty";
        case RobotState::BallLaunching:        return "BallLaunching";
        case RobotState::WaitLoyaltyChange:    return "WaitLoyaltyChange";
        case RobotState::BackHome:        return "BackHome";
        case RobotState::WaitBallReload:  return "WaitBallReload";
        case RobotState::ForceStopped:    return "ForceStopped";
        case RobotState::ManualControl:   return "ManualControl";
        case RobotState::Error:           return "Error";
        default:                          return "Unknown";
    }
}

bool stringToState(const char* str, RobotState& out) {
    if (strcmp(str, "Idle") == 0) {
        out = RobotState::Idle;
        return true;
    }
    if (strcmp(str, "MoveToNextJunction") == 0) {
        out = RobotState::MoveToNextJunction;
        return true;
    }
    if (strcmp(str, "CheckHillLoyalty") == 0) {
        out = RobotState::CheckHillLoyalty;
        return true;
    }
    if (strcmp(str, "BallLaunching") == 0) {
        out = RobotState::BallLaunching;
        return true;
    }
    if (strcmp(str, "WaitLoyaltyChange") == 0) {
        out = RobotState::WaitLoyaltyChange;
        return true;
    }
    if (strcmp(str, "BackHome") == 0) {
        out = RobotState::BackHome;
        return true;
    }
    if (strcmp(str, "WaitBallReload") == 0) {
        out = RobotState::WaitBallReload;
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

const char* teamToString(RobotTeam t) {
    switch (t) {
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
