#ifndef GLOBALS_H
#define GLOBALS_H

#include "FreeRTOS.h"
#include "queue.h"
#include <stdint.h>
#include "config.h"
#include <Arduino.h>

enum class TaskId {
  GameStatus = 0,
  TeamStatus,
  BallLauncher,
  IrBeaconDetector,
#ifndef FULLY_AUTONOMOUS
  UserInterface,
  ManualControl,
#endif
  Count
};

constexpr size_t NUM_TASK =
    static_cast<size_t>(TaskId::Count);

constexpr size_t toIndex(TaskId id) {
    return static_cast<size_t>(id);
}

constexpr size_t EVENT_QUEUE_SIZE = 20;
constexpr size_t NOTIF_QUEUE_SIZE = 10;
extern QueueHandle_t g_fsmNotifQueue[NUM_TASK]; // fsm task -> other task, created in each task
extern QueueHandle_t g_fsmEventQueue;    // other -> fsm task

// -----------------------
enum class RobotState {
    Idle,
    MoveToNextJunction,
    CheckHillLoyalty,
    BallLaunching,
    WaitLoyaltyChange,
    BackHome,
    WaitBallReload,
    ForceStopped,
    ManualControl,
    Error
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

bool isGameStartedState(RobotState s);
bool isRobotMoving(RobotState s);
const char* stateToString(RobotState s);
bool stringToState(const char* str, RobotState& out);
const char* teamToString(RobotTeam t);
const char* beaconStateToString(BeaconState s);
// -----------------------

/* ---------- Event (request) sent from other to FSM task ------------- */
enum class FsmEventType {
    GameStartReq,
    GameTimeout,
    TeamChangeReq,
    BallLaunched,
    BucketEmptyDetected,
    IrBeaconChangeDetected,
    UserStateChangeReq
};

// struct LineDetectedData {
//     uint16_t position;
//     uint16_t left;
//     uint16_t right;
// };

struct FsmEventQueueItem {
    FsmEventType type;

    union {
        bool startPressed;
        bool teamChanged;
        bool ballLaunched;
        RobotState newState;
        BeaconState newBeaconState;
        // LineDetectedData lineDetected;
    } data;
};

// -------------------------------------

/* ---------- (State/Team changed) Notif sent from FSM task to worker tasks ------------- */
enum class FsmNotifType {
  StateChanged,
  TeamChanged,
  BeaconChanged
};

struct FsmNotifData {
  RobotState state;
  RobotTeam team;
  BeaconState beacon;
};

struct FsmNotifQueueItem {
    FsmNotifType type;
    FsmNotifData data;
};

void initQueues();



#endif