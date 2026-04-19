#include "fsm.h"
#include <Arduino.h>
#include "globals.h"

static RobotFSM fsm;

void fsmTask(void *parameter);
TaskHandle_t fsmTaskHandle = nullptr;


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
    _team = (_team == RobotTeam::Blue) ? RobotTeam::Red : RobotTeam::Blue;
}

void initFsmTask() {
    // create the Task
    xTaskCreatePinnedToCore(
        fsmTask,          // Task function
        "FsmTask",        // Task name
        8192,             // Stack size (bytes)
        NULL,              // Parameters
        2,                 // Priority
        &fsmTaskHandle,  // Task handle
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
         BaseType_t ok = xQueueSend(g_fsmNotifQueue[i], &notif, 0); // expect return: pdPASS
    }
}

void fsmTask(void *parameter) {
    FsmEventQueueItem ev{};
    for(;;) {
        // block for event queue
        if (xQueueReceive(
                g_fsmEventQueue, 
                &ev, 
                portMAX_DELAY) == pdPASS) {
            
            DEBUG_LEVEL_2("FSM task rcvd a new event");
            switch (ev.type) {
                case FsmEventType::GameStartReq:
                    DEBUG_LEVEL_1("Game start req");
                    // TODO:
                    if (fsm.get_state() == RobotState::Idle) {
                        fsm.set_state(RobotState::Started);
                        auto notif = makeStateChangedNotif(fsm.get_state(), fsm.get_team());
                        broadcastNotif(notif);
                    }
                    break;
                case FsmEventType::GameTimeout:
                    DEBUG_LEVEL_1("Game timeout event");
                    // TODO:
                    if (fsm.get_state() == RobotState::Started) {
                        fsm.set_state(RobotState::Idle);
                        auto notif = makeStateChangedNotif(fsm.get_state(), fsm.get_team());
                        broadcastNotif(notif);
                    }
                    break;
                case FsmEventType::TeamChangeReq:
                    DEBUG_LEVEL_1("Team change req");
                    // TODO:
                    if (fsm.get_state() == RobotState::Idle) {
                        fsm.toggle_team();
                        auto notif = makeTeamChangedNotif(fsm.get_state(), fsm.get_team());
                        broadcastNotif(notif);
                    }
                    break;
                case FsmEventType::BallLaunched:
                    DEBUG_LEVEL_1("Ball launched event");
                    if (fsm.get_state() == RobotState::LaunchingBall) {
                        // TODO: transition to IR beacon polling instead of idle
                        fsm.set_state(RobotState::Idle);
                        auto notif = makeStateChangedNotif(fsm.get_state(), fsm.get_team());
                        broadcastNotif(notif);
                    }
                    break;
                case FsmEventType::BucketEmptyDetected:
                    DEBUG_LEVEL_1("Bucket empty event");
                    if (fsm.get_state() == RobotState::LaunchingBall) {
                        // TODO:
                        fsm.set_state(RobotState::Idle);
                        auto notif = makeStateChangedNotif(fsm.get_state(), fsm.get_team());
                        broadcastNotif(notif);
                    }
                    break;
                case FsmEventType::UserStateChangeReq:
                    DEBUG_LEVEL_1("User state change req");
                    if (fsm.get_state() == RobotState::Idle) {
                        fsm.set_state(ev.data.newState);
                        auto notif = makeStateChangedNotif(fsm.get_state(), fsm.get_team());
                        broadcastNotif(notif);
                    }
                    break;
                default:
                    // TODO:
                    DEBUG_LEVEL_1("[Error] Event not handled by fsm");
                    break;
            }
        
        }

    }
}