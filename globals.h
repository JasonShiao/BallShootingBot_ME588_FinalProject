#ifndef GLOBALS_H
#define GLOBALS_H

#include "FreeRTOS.h"
#include "queue.h"
#include <stdint.h>

enum class TaskId {
  GameStatus = 0,
  TeamStatus,
  Count
};

constexpr size_t NUM_TASK =
    static_cast<size_t>(TaskId::Count);

constexpr size_t ToIndex(TaskId id) {
    return static_cast<size_t>(id);
}

extern QueueHandle_t g_FsmNotifQueue[NUM_TASK]; // fsm task -> other task, created in each task
extern QueueHandle_t g_FsmEventQueue;    // other -> fsm task

// -----------------------
enum class RobotState {
    IDLE,
    STARTED
};

enum class RobotTeam {
  BLUE,
  RED
};
// -----------------------

/* ---------- Event (request) sent from other to FSM task ------------- */
enum class FsmEventType {
    GameStartReq,
    GameTimeoutReq,
    TeamChangeReq
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