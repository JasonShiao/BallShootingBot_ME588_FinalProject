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
    Startup,
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

enum class RobotLocation {
    Home,               // sensor 1 and sensor 2 both at home
    HomeBorderCrossed1, // sensor 1 side outside home
    HomeBorderCrossed2, // sensor 2 side outside home
    HomeToJunction1,
    Junction1ToJunction2,
    Junction2ToJunction3,
    Junction3ToJunction4,
    Junction4ToRoadEnd
};

enum class RobotHeading {
    Forward, // leave home 
    Backward // back home
};

bool isOwnBeacon(RobotTeam team, BeaconState beacon);
bool stringToState(const char* str, RobotState& out);
const char* teamToString(RobotTeam t);
const char* beaconStateToString(BeaconState s);
const char* locationToString(RobotLocation l);
const char* headingToString(RobotHeading h);
// -----------------------

/* ---------- Event (request) sent from other to FSM task ------------- */
enum class FsmEventType {
    StartupDone,
    GameStartReq,
    TeamChangeReq,
    GameTimeout,
    JunctionCrossed,
    BallLoaded,
    BallLaunched,
    BucketEmptyDetected,
    BucketReloadTimeout,    // use a timeout directly for reload done
    IrBeaconChangeDetected, // automatically send
    IrBeaconQueryResponse,  // response to query 
    // === Special event below ===
    UserStateChangeReq
};

const char* eventToString(FsmEventType e);

struct JunctionCrossedInfo {
    bool left;
    bool right;
};

RobotLocation determineNewLocation(
  RobotLocation currLocation, RobotHeading heading, 
  JunctionCrossedInfo junct, RobotTeam team);

struct FsmEventQueueItem {
    FsmEventType type;

    union {
        bool startPressed;
        bool teamChanged;
        bool ballLoaded;
        bool ballLaunched;
        bool bucketReloaded;
        RobotState newState;
        BeaconState newBeaconState;
        JunctionCrossedInfo junctionCrossed;
    } data;
};

// -------------------------------------

void initQueues();

bool sendFsmEventItem(const FsmEventQueueItem& ev);
bool sendFsmEventItemFromISR(
    const FsmEventQueueItem& ev, BaseType_t& xHigherPriorityTaskWoken);
BaseType_t receiveFsmEventItem(
    FsmEventQueueItem& ev, TickType_t xTicksToWait);


#endif
