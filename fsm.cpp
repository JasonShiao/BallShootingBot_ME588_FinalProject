#include "fsm.h"
#include <Arduino.h>
#include "globals.h"

static RobotFSM fsm;

void fsmTask(void *parameter);
TaskHandle_t fsmTaskHandle = nullptr;


RobotState RobotFSM::getState() {
  return _state;
}

void RobotFSM::setState(RobotState new_state) {
    _state = new_state;
}

RobotTeam RobotFSM::getTeam() {
  return _team;
}

void RobotFSM::toggleTeam() {
    _team = (_team == RobotTeam::Blue) ? RobotTeam::Red : RobotTeam::Blue;
}

void RobotFSM::setBeacon(BeaconState newState) {
    _beacon = newState;
}

BeaconState RobotFSM::getBeacon() {
    return _beacon;
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

inline FsmNotifQueueItem makeChangedNotif(FsmNotifType notifType) {
    
    FsmNotifQueueItem n{};
    n.type = notifType;
    n.data.state = fsm.getState();
    n.data.team = fsm.getTeam();
    n.data.beacon = fsm.getBeacon();
    
    return n;
}

void broadcastNotif(FsmNotifQueueItem notif) {
    for (int i = 0; i < NUM_TASK; i++) {
         BaseType_t ok = xQueueSend(g_fsmNotifQueue[i], &notif, 0); // expect return: pdPASS
    }
}

void fsmTask(void *parameter) {
    FsmEventQueueItem ev{};
    FsmNotifQueueItem notif{};
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
                    if (fsm.getState() == RobotState::Idle) {
                        fsm.setState(RobotState::MoveToNextJunction);
                        notif = makeChangedNotif(FsmNotifType::StateChanged);
                        broadcastNotif(notif);
                    }
                    break;
                case FsmEventType::GameTimeout:
                    DEBUG_LEVEL_1("Game timeout event");
                    // TODO:
                    if (isGameStartedState(fsm.getState())) {
                        fsm.setState(RobotState::Idle);
                        notif = makeChangedNotif(FsmNotifType::StateChanged);
                        broadcastNotif(notif);
                    }
                    break;
                case FsmEventType::TeamChangeReq:
                    DEBUG_LEVEL_1("Team change req");
                    // TODO:
                    if (fsm.getState() == RobotState::Idle) {
                        fsm.toggleTeam();
                        notif = makeChangedNotif(FsmNotifType::TeamChanged);
                        broadcastNotif(notif);
                    }
                    break;
                case FsmEventType::BallLaunched:
                    DEBUG_LEVEL_1("Ball launched event");
                    if (fsm.getState() == RobotState::BallLaunching) {
                        fsm.setState(RobotState::WaitLoyaltyChange);
                        notif = makeChangedNotif(FsmNotifType::StateChanged);
                        broadcastNotif(notif);
                    }
                    break;
                case FsmEventType::BucketEmptyDetected:
                    DEBUG_LEVEL_1("Bucket empty event");
                    if (fsm.getState() == RobotState::BallLaunching) {
                        // TODO:
                        fsm.setState(RobotState::BackHome);
                        notif = makeChangedNotif(FsmNotifType::StateChanged);
                        broadcastNotif(notif);
                    }
                    break;
                case FsmEventType::IrBeaconChangeDetected:
                    DEBUG_LEVEL_1("IR beacon changed event: %s", beaconStateToString(ev.data.newBeaconState));
                    fsm.setBeacon(ev.data.newBeaconState);
                    notif = makeChangedNotif(FsmNotifType::BeaconChanged);
                    broadcastNotif(notif);
                    break;
                case FsmEventType::UserStateChangeReq:
                    DEBUG_LEVEL_1("User state change req");
                    fsm.setState(ev.data.newState);
                    notif = makeChangedNotif(FsmNotifType::StateChanged);
                    broadcastNotif(notif);
                    
                    // immediately from force stopped to idle
                    if (ev.data.newState == RobotState::ForceStopped) {
                        fsm.setState(RobotState::Idle);
                        notif = makeChangedNotif(FsmNotifType::StateChanged);
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