#include "fsm.h"
#include <Arduino.h>

static RobotFSM fsm;

void FsmTask(void *parameter);
TaskHandle_t FsmTaskHandle = nullptr;


RobotState RobotFSM::get_state() {
  return _state;
}

void RobotFSM::set_state(RobotState new_state) {
    _state = new_state;
}

RobotTeam RobotFSM::get_team() {
  return _team;
}

void RobotFSM::toggle_team() {
    _team = (_team == RobotTeam::BLUE) ? RobotTeam::RED : RobotTeam::BLUE;
}

void InitFsmTask() {
    // create the Task
    xTaskCreatePinnedToCore(
        FsmTask,          // Task function
        "FsmTask",        // Task name
        8192,             // Stack size (bytes)
        NULL,              // Parameters
        2,                 // Priority
        &FsmTaskHandle,  // Task handle
        1                  // Core 1
    );
}

inline FsmNotifQueueItem makeStateChangedNotif(RobotState state, RobotTeam team) {
    FsmNotifQueueItem n{};
    n.type = FsmNotifType::StateChanged;
    n.data.state = state;
    n.data.team = team;
    return n;
}

inline FsmNotifQueueItem makeTeamChangedNotif(RobotState state, RobotTeam team) {
    FsmNotifQueueItem n{};
    n.type = FsmNotifType::TeamChanged;
    n.data.state = state;
    n.data.team = team;
    return n;
}

void broadcastNotif(FsmNotifQueueItem notif) {
    for (int i = 0; i < NUM_TASK; i++) {
         BaseType_t ok = xQueueSend(g_FsmNotifQueue[i], &notif, 0); // expect return: pdPASS
    }
}

void FsmTask(void *parameter) {
    FsmEventQueueItem ev{};
    for(;;) {
        // block for event queue
        if (xQueueReceive(
                g_FsmEventQueue, 
                &ev, 
                portMAX_DELAY) == pdPASS) {
            
            DEBUG_LEVEL_1("FSM task rcvd a new event");
            switch (ev.type) {
                case FsmEventType::GameStartReq:
                    DEBUG_LEVEL_1("Game start req");
                    // TODO:
                    if (fsm.get_state() == RobotState::IDLE) {
                        fsm.set_state(RobotState::STARTED);
                        auto notif = makeStateChangedNotif(fsm.get_state(), fsm.get_team());
                        broadcastNotif(notif);
                    }
                    break;
                case FsmEventType::GameTimeoutReq:
                    DEBUG_LEVEL_1("Game timeout req");
                    // TODO:
                    if (fsm.get_state() == RobotState::STARTED) {
                        fsm.set_state(RobotState::IDLE);
                        auto notif = makeStateChangedNotif(fsm.get_state(), fsm.get_team());
                        broadcastNotif(notif);
                    }
                    break;
                case FsmEventType::TeamChangeReq:
                    DEBUG_LEVEL_1("Team change req");
                    // TODO:
                    if (fsm.get_state() == RobotState::IDLE) {
                        fsm.toggle_team();
                        auto notif = makeTeamChangedNotif(fsm.get_state(), fsm.get_team());
                        broadcastNotif(notif);
                    }
                    break;
                default:
                    // TODO:
                    break;
            }
        
        }

    }
}