#ifndef GLOBALS_H
#define GLOBALS_H

#include "FreeRTOS.h"
#include "queue.h"
#include <stdint.h>
#include "config.h"
#include <Arduino.h>


constexpr size_t EVENT_QUEUE_SIZE = 20;

// -----------------------
enum class RobotState {
    Idle,
    MoveToNextJunction,
    CheckHillLoyalty,
    BallLoading,
    BallLaunching,
    BackHome,
    WaitBucketReload,
    ManualControl,
    Error,
    // Parent States
    GameActive,
    GameInactive,
    Navigation,
    HillInteraction,
    //
    Unknown
};

enum class RobotTeam {
    Blue,
    Red
};

enum class BeaconState {
    Unknown,
    Beacon750,
    Beacon1k5
};

bool isOwnBeacon(RobotTeam team, BeaconState beacon);
bool stringToState(const char* str, RobotState& out);
const char* teamToString(RobotTeam t);
const char* beaconStateToString(BeaconState s);
// -----------------------

/* ---------- Event (request) sent from other to FSM task ------------- */
enum class FsmEventType {
    GameStartReq,
    TeamChangeReq,
    GameTimeout,
    BallLoaded,
    BallLaunched,
    BucketEmptyDetected,
    BucketReloadTimeout,
    IrBeaconChangeDetected, // automatically send
    IrBeaconQueryResponse,  // response to query 
    // === Special event below ===
    UserStateChangeReq
};

const char* eventToString(FsmEventType e);

struct FsmEventQueueItem {
    FsmEventType type;

    union {
        bool startPressed;
        bool teamChanged;
        bool ballLoaded;
        bool ballLaunched;
        RobotState newState;
        BeaconState newBeaconState;
    } data;
};

// -------------------------------------

void initQueues();



#endif