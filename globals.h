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
  UserInterface,
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
    Started,
    LaunchingBall,
    ForceStopped,
    Error
};

enum class RobotTeam {
  Blue,
  Red
};

const char* stateToString(RobotState s);
bool stringToState(const String& str, RobotState& out);
// -----------------------

/* ---------- Event (request) sent from other to FSM task ------------- */
enum class FsmEventType {
    GameStartReq,
    GameTimeout,
    TeamChangeReq,
    BallLaunched,
    BucketEmptyDetected,
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
        // LineDetectedData lineDetected;
    } data;
};

// -------------------------------------

/* ---------- (State/Team changed) Notif sent from FSM task to worker tasks ------------- */
enum class FsmNotifType {
  StateChanged,
  TeamChanged
};

struct FsmNotifData {
  RobotState state;
  RobotTeam team;
};

struct FsmNotifQueueItem {
    FsmNotifType type;
    FsmNotifData data;
};

void initQueues();



#endif