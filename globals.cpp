#include "globals.h"
#include <Arduino.h>

// From other task -> fsm task
QueueHandle_t g_fsmEventQueue = nullptr;

void initQueues() {
    g_fsmEventQueue = xQueueCreate(EVENT_QUEUE_SIZE, sizeof(FsmEventQueueItem));

    if(g_fsmEventQueue == NULL) {
        /* One or more queues were not created successfully as there was not enough
           heap memory available. Handle the error here. */
        DEBUG_LEVEL_1("Fsm Event queue created failed");
    }
}

bool isOwnBeacon(RobotTeam team, BeaconState beacon) {
    return (team == RobotTeam::Blue && beacon == BeaconState::Beacon750) ||
           (team == RobotTeam::Red  && beacon == BeaconState::Beacon1k5);
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
    if (strcmp(str, "BallLoading") == 0) {
        out = RobotState::BallLoading;
        return true;
    }
    if (strcmp(str, "BallLaunching") == 0) {
        out = RobotState::BallLaunching;
        return true;
    }
    if (strcmp(str, "BackHome") == 0) {
        out = RobotState::BackHome;
        return true;
    }
    if (strcmp(str, "WaitBucketReload") == 0) {
        out = RobotState::WaitBucketReload;
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

const char* eventToString(FsmEventType e) {
    switch (e) {
        case FsmEventType::GameStartReq: return "GameStartReq";
        case FsmEventType::GameTimeout:  return "GameTimeout";
        case FsmEventType::BallLoaded:   return "BallLoaded";
        case FsmEventType::BallLaunched: return "BallLaunched";
        case FsmEventType::BucketEmptyDetected: return "BucketEmptyDetected";
        case FsmEventType::BucketReloadTimeout: return "BucketReloadTimeout";
        case FsmEventType::IrBeaconChangeDetected: return "IrBeaconChangeDetected";
        case FsmEventType::IrBeaconQueryResponse:  return "IrBeaconQueryResponse";
        case FsmEventType::TeamChangeReq:  return "TeamChangeReq";
        case FsmEventType::UserStateChangeReq: return "UserStateChangeReq";
        default: return "Unknown";
    }
}