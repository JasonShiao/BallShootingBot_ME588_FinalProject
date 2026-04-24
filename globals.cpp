#include "globals.h"
#include <Arduino.h>

// From other task -> fsm task
QueueHandle_t g_fsmEventQueue = nullptr;

bool sendFsmEventItem(const FsmEventQueueItem& ev) {
    if (g_fsmEventQueue == nullptr) {
        return false;
    }
    return xQueueSend(g_fsmEventQueue, &ev, 0) == pdPASS;
}

bool sendFsmEventItemFromISR(const FsmEventQueueItem& ev, 
  BaseType_t& xHigherPriorityTaskWoken) {
    return xQueueSendFromISR(g_fsmEventQueue, &ev, &xHigherPriorityTaskWoken);
}

BaseType_t receiveFsmEventItem(FsmEventQueueItem& ev, TickType_t xTicksToWait) {
    return xQueueReceive(g_fsmEventQueue, &ev, xTicksToWait) == pdPASS;
}


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
    if (strcmp(str, "Startup") == 0) {
        out = RobotState::Startup;
        return true;
    }
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
        case FsmEventType::StartupDone:  return "StartupDone";
        case FsmEventType::GameStartReq: return "GameStartReq";
        case FsmEventType::GameTimeout:  return "GameTimeout";
        case FsmEventType::JunctionCrossed: return "JunctionCrossed";
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

const char* locationToString(RobotLocation location) {
    switch (location) {
        case RobotLocation::Home: return "Home";
        case RobotLocation::HomeBorderCrossed1: return "HomeBorderCrossed1";
        case RobotLocation::HomeBorderCrossed2: return "HomeBorderCrossed2";
        case RobotLocation::HomeToJunction1: return "HomeToJunction1";
        case RobotLocation::Junction1ToJunction2: return "Junction1ToJunction2";
        case RobotLocation::Junction2ToJunction3: return "Junction2ToJunction3";
        case RobotLocation::Junction3ToJunction4: return "Junction3ToJunction4";
        case RobotLocation::Junction4ToRoadEnd: return "Junction4ToRoadEnd";
        default:
            return "Unknown";
    }
}

const char* headingToString(RobotHeading head) {
    switch (head) {
        case RobotHeading::Forward: return "Forward";
        case RobotHeading::Backward: return "Backward";
        default:
            return "Unknown";
    }
}

RobotLocation determineNewLocation(
  RobotLocation currLocation, RobotHeading heading, 
  JunctionCrossedInfo junct, RobotTeam team) {

    RobotLocation newLocation = currLocation;

    if (heading == RobotHeading::Forward) {
        switch (currLocation) {
            case RobotLocation::Home:
                if (junct.left && junct.right) {
                    newLocation = RobotLocation::HomeToJunction1;
                } else if (junct.left) {
                    newLocation = RobotLocation::HomeBorderCrossed1;
                } else if (junct.right) {
                    newLocation = RobotLocation::HomeBorderCrossed2;
                } else {
                    DEBUG_LEVEL_1("Unexpected junction cross");
                }
                break;
            case RobotLocation::HomeBorderCrossed1: // left sensor already outside home
                if (junct.right) {
                    newLocation = RobotLocation::HomeToJunction1;
                }
                break;
            case RobotLocation::HomeBorderCrossed2: // right sensor already outside home
                if (junct.left) {
                    newLocation = RobotLocation::HomeToJunction1;
                }
                break;
            case RobotLocation::HomeToJunction1:
                if (junct.left && team == RobotTeam::Blue) {
                    newLocation = RobotLocation::Junction1ToJunction2;
                } else if (junct.right && team == RobotTeam::Red) {
                    newLocation = RobotLocation::Junction1ToJunction2;
                }
                break;
            case RobotLocation::Junction1ToJunction2:
                if (junct.right && team == RobotTeam::Blue) {
                    newLocation = RobotLocation::Junction2ToJunction3;
                } else if (junct.left && team == RobotTeam::Red) {
                    newLocation = RobotLocation::Junction2ToJunction3;
                }
                break;
            case RobotLocation::Junction2ToJunction3:
                if (junct.left && team == RobotTeam::Blue) {
                    newLocation = RobotLocation::Junction3ToJunction4;
                } else if (junct.right && team == RobotTeam::Red) {
                    newLocation = RobotLocation::Junction3ToJunction4;
                }
                break;
            case RobotLocation::Junction3ToJunction4:
                if (junct.right && team == RobotTeam::Blue) {
                    newLocation = RobotLocation::Junction4ToRoadEnd;
                } else if (junct.left && team == RobotTeam::Red) {
                    newLocation = RobotLocation::Junction4ToRoadEnd;
                }
                break;
            case RobotLocation::Junction4ToRoadEnd:
                DEBUG_LEVEL_1("Unexpected Junction Crossed after Road End");
                break;
            default:
                DEBUG_LEVEL_1("Unexpected Robot Location");
        }
    } else { // backward
        switch (currLocation) {
            case RobotLocation::Home:
                DEBUG_LEVEL_1("Unexpected junction cross before home");
                break;
            case RobotLocation::HomeBorderCrossed1: // left is outside home
                if (junct.left) {
                    newLocation = RobotLocation::Home;
                }
                break;
            case RobotLocation::HomeBorderCrossed2:
                if (junct.right) {
                    newLocation = RobotLocation::Home;
                }
                break;
            case RobotLocation::HomeToJunction1:
                if (junct.left && junct.right) {
                    newLocation = RobotLocation::Home;
                } else if (junct.left) {
                    newLocation = RobotLocation::HomeBorderCrossed2; // right is outside
                } else if (junct.right) {
                    newLocation = RobotLocation::HomeBorderCrossed1; // left is outside
                } else {
                    DEBUG_LEVEL_1("Unexpected junction cross");
                }
                break;
            case RobotLocation::Junction1ToJunction2:
                if (junct.left && team == RobotTeam::Blue) {
                    newLocation = RobotLocation::HomeToJunction1;
                } else if (junct.right && team == RobotTeam::Red) {
                    newLocation = RobotLocation::HomeToJunction1;
                }
                break;
            case RobotLocation::Junction2ToJunction3:
                if (junct.right && team == RobotTeam::Blue) {
                    newLocation = RobotLocation::Junction1ToJunction2;
                } else if (junct.left && team == RobotTeam::Red) {
                    newLocation = RobotLocation::Junction1ToJunction2;
                }
                break;
            case RobotLocation::Junction3ToJunction4:
                if (junct.left && team == RobotTeam::Blue) {
                    newLocation = RobotLocation::Junction2ToJunction3;
                } else if (junct.right && team == RobotTeam::Red) {
                    newLocation = RobotLocation::Junction2ToJunction3;
                }
                break;
            case RobotLocation::Junction4ToRoadEnd:
                if (junct.right && team == RobotTeam::Blue) {
                    newLocation = RobotLocation::Junction3ToJunction4;
                } else if (junct.left && team == RobotTeam::Red) {
                    newLocation = RobotLocation::Junction3ToJunction4;
                }
                break;
            default:
                DEBUG_LEVEL_1("Unexpected Robot Location");
        }
    }

    return newLocation;
}

