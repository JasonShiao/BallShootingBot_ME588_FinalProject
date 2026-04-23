#include "fsm.h"
#include <Arduino.h>
#include "globals.h"
#include "game_status.h"
#include "team_status.h"
#include "ball_launcher.h"
#include "ir_beacon_detect.h"
#include "user_interface.h"
#include "manual_control.h"

#define CHANGE_STATE_BITMASK (1 << 0)
#define CHANGE_TEAM_BITMASK (1 << 1)
#define CHANGE_BEACON_BITMASK (1 << 2)

void fsmTask(void *parameter);
static TaskHandle_t fsmTaskHandle = nullptr;

static RobotFSM fsm;

uint32_t determineStateTransition(
    RobotFSM& fsm, FsmEventQueueItem& ev, NewStates& newStates);
void transitionToState(RobotFSM& fsm, RobotState newState, FsmEventQueueItem& ev);
void handleEventWithCurrentState(RobotFSM& fsm, FsmEventQueueItem& ev);

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

// inline FsmNotifQueueItem makeChangedNotif(FsmNotifType notifType) {
    
//     FsmNotifQueueItem n{};
//     n.type = notifType;
//     n.data.state = fsm.getState();
//     n.data.team = fsm.getTeam();
//     n.data.beacon = fsm.getBeacon();
    
//     return n;
// }

// void broadcastNotif(FsmNotifQueueItem notif) {
//     for (int i = 0; i < NUM_TASK; i++) {
//          BaseType_t ok = xQueueSend(g_fsmNotifQueue[i], &notif, 0); // expect return: pdPASS
//     }
// }

void fsmTask(void *parameter) {
    FsmEventQueueItem ev{};
    TeamStatusCtrlCmd teamStatusCmd{};
    UserInterfaceUpdateMsg uiUpdateCmd{};

    NewStates newStates{};
    //FsmNotifQueueItem notif{};
    for(;;) {
        // block for event queue

        // bit-0 for state change, 
        // bit-1 for team change, 
        // bit-2 for beacon change
        uint32_t fsmChangeBitMask = 0; 
        if (xQueueReceive(
                g_fsmEventQueue, 
                &ev, 
                portMAX_DELAY) == pdPASS) {
            
            DEBUG_LEVEL_2("FSM task rcvd a new event");

            // 1. determine the new state based on current state and event
            fsmChangeBitMask = determineStateTransition(fsm, ev, newStates);

            // 2. team and beacon changes are independent
            if (fsmChangeBitMask & CHANGE_TEAM_BITMASK) {
                fsm.toggleTeam();
                teamStatusCmd.type = TeamStatusCtrlCmdType::TeamChange;
                teamStatusCmd.data.team = fsm.getTeam();
                sendTeamStatusCtrlCmd(teamStatusCmd);
            }
            if (fsmChangeBitMask & CHANGE_BEACON_BITMASK) {
                fsm.setBeacon(newStates.beacon);
            }

            // 3. exit and entry action based on state transition
            if (fsmChangeBitMask & CHANGE_STATE_BITMASK) {
                transitionToState(fsm, newStates.state, ev);
            } else {
                // normal handling of event with current state
                handleEventWithCurrentState(fsm, ev);
            }

            // 4. send status change to UI (e.g. update status on webpage)
            if (fsmChangeBitMask != 0) {
                uiUpdateCmd.currentState = fsm.getState();
                uiUpdateCmd.team = fsm.getTeam();
                uiUpdateCmd.currentBeaconState = fsm.getBeacon();
                sendUserInterfaceUpdate(uiUpdateCmd);
            }
        
        }

    }
}


// ===============================================
uint32_t determineStateTransition(
        RobotFSM& fsm, FsmEventQueueItem& ev, NewStates& newStates) {
    
    uint32_t fsmChangeBitMask = 0; 

    switch (fsm.getState()) {
        case RobotState::Idle:
            if (ev.type == FsmEventType::GameStartReq) {
                newStates.state = RobotState::MoveToNextJunction;
                fsmChangeBitMask |= CHANGE_STATE_BITMASK;
            } else if (ev.type == FsmEventType::TeamChangeReq) {
                newStates.team = fsm.getOpponentTeam();
                fsmChangeBitMask |= CHANGE_TEAM_BITMASK;
            } else if (ev.type == FsmEventType::IrBeaconChangeDetected) {
                newStates.beacon = ev.data.newBeaconState;
                fsmChangeBitMask |= CHANGE_BEACON_BITMASK;
            } else if (ev.type == FsmEventType::UserStateChangeReq) {
                newStates.state = ev.data.newState;
                fsmChangeBitMask |= CHANGE_STATE_BITMASK;
            }
            break;
        case RobotState::MoveToNextJunction:
            if (ev.type == FsmEventType::GameTimeout) {
                newStates.state = RobotState::Idle;
                fsmChangeBitMask |= CHANGE_STATE_BITMASK;
            } else if (ev.type == FsmEventType::IrBeaconChangeDetected) {
                newStates.beacon = ev.data.newBeaconState;
                fsmChangeBitMask |= CHANGE_BEACON_BITMASK;
            } else if (ev.type == FsmEventType::UserStateChangeReq) {
                newStates.state = ev.data.newState;
                fsmChangeBitMask |= CHANGE_STATE_BITMASK;
            }
            // TODO: Upon junction detect event, switch to CheckHillLoyalty state
            break;
        case RobotState::CheckHillLoyalty:
            if (ev.type == FsmEventType::GameTimeout) {
                newStates.state = RobotState::Idle;
                fsmChangeBitMask |= CHANGE_STATE_BITMASK;
            } else if (ev.type == FsmEventType::IrBeaconChangeDetected) {
                newStates.beacon = ev.data.newBeaconState;
                fsmChangeBitMask |= CHANGE_BEACON_BITMASK;
            } else if (ev.type == FsmEventType::UserStateChangeReq) {
                newStates.state = ev.data.newState;
                fsmChangeBitMask |= CHANGE_STATE_BITMASK;
            } else if (ev.type == FsmEventType::IrBeaconQueryResponse) {
                newStates.beacon = ev.data.newBeaconState;
                fsmChangeBitMask |= CHANGE_BEACON_BITMASK;
                if (fsm.getTeam() == RobotTeam::Red) {
                    if (ev.data.newBeaconState == BeaconState::Beacon1k5) {
                        // matched -> move to next hill
                        newStates.state = RobotState::MoveToNextJunction;
                        fsmChangeBitMask |= CHANGE_STATE_BITMASK;
                    } else if (ev.data.newBeaconState == BeaconState::Beacon750) {
                        // opposite team -> launch ball
                        newStates.state = RobotState::BallLaunching;
                        fsmChangeBitMask |= CHANGE_STATE_BITMASK;
                    } else { // unknown -> might not be any hill here
                        fsm.incAndGetTryCnt();
                        if (fsm.incAndGetTryCnt() >= 3) {
                            // tried 3 times but still unknown -> move on
                            newStates.state = RobotState::MoveToNextJunction;
                            fsmChangeBitMask |= CHANGE_STATE_BITMASK;
                        }
                    }
                } else { // team Blue
                    if (ev.data.newBeaconState == BeaconState::Beacon750) {
                        // matched -> move to next hill
                        newStates.state = RobotState::MoveToNextJunction;
                        fsmChangeBitMask |= CHANGE_STATE_BITMASK;
                    } else if (ev.data.newBeaconState == BeaconState::Beacon1k5) {
                        // opposite team -> launch ball
                        newStates.state = RobotState::BallLaunching;
                        fsmChangeBitMask |= CHANGE_STATE_BITMASK;
                    } else { // unknown -> might not be any hill here
                        fsm.incAndGetTryCnt();
                        if (fsm.incAndGetTryCnt() >= 3) {
                            // tried 3 times but still unknown -> move on
                            newStates.state = RobotState::MoveToNextJunction;
                            fsmChangeBitMask |= CHANGE_STATE_BITMASK;
                        }
                    }
                }
            }
            break;
        case RobotState::BallLaunching:
            if (ev.type == FsmEventType::GameTimeout) {
                newStates.state = RobotState::Idle;
                fsmChangeBitMask |= CHANGE_STATE_BITMASK;
            } else if (ev.type == FsmEventType::UserStateChangeReq) {
                newStates.state = ev.data.newState;
                fsmChangeBitMask |= CHANGE_STATE_BITMASK;
            } else if (ev.type == FsmEventType::IrBeaconChangeDetected) {
                newStates.beacon = ev.data.newBeaconState;
                fsmChangeBitMask |= CHANGE_BEACON_BITMASK;
            } else if (ev.type == FsmEventType::BallLoaded) {
                // stay at current state, trigger shooting
            } else if (ev.type == FsmEventType::BallLaunched) {
                newStates.state = RobotState::WaitLoyaltyChange;
                fsmChangeBitMask |= CHANGE_STATE_BITMASK;
            } else if (ev.type == FsmEventType::BucketEmptyDetected) {
                newStates.state = RobotState::BackHome;
                fsmChangeBitMask |= CHANGE_STATE_BITMASK;
            }
            break;
        case RobotState::WaitLoyaltyChange:
            if (ev.type == FsmEventType::GameTimeout) {
                newStates.state = RobotState::Idle;
                fsmChangeBitMask |= CHANGE_STATE_BITMASK;
            } else if (ev.type == FsmEventType::UserStateChangeReq) {
                newStates.state = ev.data.newState;
                fsmChangeBitMask |= CHANGE_STATE_BITMASK;
            } else if (ev.type == FsmEventType::IrBeaconChangeDetected) {
                newStates.beacon = ev.data.newBeaconState;
                fsmChangeBitMask |= CHANGE_BEACON_BITMASK;
                // loyalty matched
                if (((fsm.getTeam() == RobotTeam::Red) && 
                    (ev.data.newBeaconState == BeaconState::Beacon1k5)) || 
                    ((fsm.getTeam() == RobotTeam::Blue) && 
                    (ev.data.newBeaconState == BeaconState::Beacon750))) {
                     // loyalty matched -> move to next hill
                    newStates.state = RobotState::MoveToNextJunction;
                    fsmChangeBitMask |= CHANGE_STATE_BITMASK;
                }
            } else if (ev.type == FsmEventType::IrBeaconQueryResponse) {
                newStates.beacon = ev.data.newBeaconState;
                fsmChangeBitMask |= CHANGE_BEACON_BITMASK;
                if (fsm.getTeam() == RobotTeam::Red) {
                    if (ev.data.newBeaconState == BeaconState::Beacon1k5) {
                        // loyalty matched -> move to next hill
                        newStates.state = RobotState::MoveToNextJunction;
                        fsmChangeBitMask |= CHANGE_STATE_BITMASK;
                    } else {
                        // TODO: launch again or ...?
                        newStates.state = RobotState::MoveToNextJunction;
                        fsmChangeBitMask |= CHANGE_STATE_BITMASK;
                        DEBUG_LEVEL_1("IrBeaconQueryResponse in WaitLoyaltyChange state, unhandled case");
                    }
                } else { // team Blue
                    if (ev.data.newBeaconState == BeaconState::Beacon750) {
                        // loyalty matched -> move to next hill
                        newStates.state = RobotState::MoveToNextJunction;
                        fsmChangeBitMask |= CHANGE_STATE_BITMASK;
                    } else {
                        // TODO: launch again or ...?
                        newStates.state = RobotState::MoveToNextJunction;
                        fsmChangeBitMask |= CHANGE_STATE_BITMASK;
                        DEBUG_LEVEL_1("IrBeaconQueryResponse in WaitLoyaltyChange state, unhandled case");
                    }
                }
            }
            break;
        case RobotState::BackHome:
            if (ev.type == FsmEventType::GameTimeout) {
                newStates.state = RobotState::Idle;
                fsmChangeBitMask |= CHANGE_STATE_BITMASK;
            } else if (ev.type == FsmEventType::UserStateChangeReq) {
                newStates.state = ev.data.newState;
                fsmChangeBitMask |= CHANGE_STATE_BITMASK;
            } else if (ev.type == FsmEventType::IrBeaconChangeDetected) {
                newStates.beacon = ev.data.newBeaconState;
                fsmChangeBitMask |= CHANGE_BEACON_BITMASK;
            }
            break;
        case RobotState::WaitBallReload:
            // wait for ball reload timeout or user override
             if (ev.type == FsmEventType::GameTimeout) {
                newStates.state = RobotState::Idle;
                fsmChangeBitMask |= CHANGE_STATE_BITMASK;
            } else if (ev.type == FsmEventType::UserStateChangeReq) {
                newStates.state = ev.data.newState;
                fsmChangeBitMask |= CHANGE_STATE_BITMASK;
            } else if (ev.type == FsmEventType::BallReloadTimeout) {
                newStates.state = RobotState::MoveToNextJunction;
                fsmChangeBitMask |= CHANGE_STATE_BITMASK;
            }
            DEBUG_LEVEL_1("In WaitBallReload state, reload timeout timer not implemented yet");
            break;
        case RobotState::ForceStopped:
            newStates.state = RobotState::Idle;
            fsmChangeBitMask |= CHANGE_STATE_BITMASK;
            DEBUG_LEVEL_1("In ForceStopped state, event handling not implemented yet");
            break;
        case RobotState::ManualControl:
            if (ev.type == FsmEventType::UserStateChangeReq) {
                newStates.state = ev.data.newState;
                fsmChangeBitMask |= CHANGE_STATE_BITMASK;
            } else if (ev.type == FsmEventType::IrBeaconChangeDetected) {
                newStates.beacon = ev.data.newBeaconState;
                fsmChangeBitMask |= CHANGE_BEACON_BITMASK;
            } else if (ev.type == FsmEventType::TeamChangeReq) {
                newStates.team = fsm.getOpponentTeam();
                fsmChangeBitMask |= CHANGE_TEAM_BITMASK;
            }
            break;
        case RobotState::Error:
            DEBUG_LEVEL_1("In Error state, event handling not implemented yet");
            break;
        default:
            DEBUG_LEVEL_1("[Error] Invalid state in FSM");
            break;
    }

    return fsmChangeBitMask;
}

void transitionToState(RobotFSM& fsm, RobotState newState, FsmEventQueueItem& ev) {
    GameStatusCtrlCmd gameStatusCmd{};
    BallLauncherCtrlCmd ballLauncherCmd{};
    IrBeaconDetectCtrlCmd irBeaconDetectCmd{};
    ManualControlCmd manualControlCmd{};

    // execute exit and entry action here based on current state and newState
    RobotState oldState = fsm.getState();

    fsm.resetTryCnt(); // reset try count for any state
    switch (oldState) {
        case RobotState::Idle:
            switch (newState) {
                case RobotState::MoveToNextJunction:
                    // 0. change game status
                    gameStatusCmd.startGame = true;
                    gameStatusCmd.enableBtnEvent = false;
                    sendGameStatusCtrlCmd(gameStatusCmd);
                    // 2. activate navigation task
                    // TODO: 
                    break;
                case RobotState::ManualControl:
                    manualControlCmd.type = ManualControlCmdType::CtrlCmd;
                    manualControlCmd.data.enable = true;
                    sendManualControlCmd(manualControlCmd);
                    break;
#if ENABLE_FORCE_STATE_TRANSITION
                // ===== below For force state transition (not well maintained) =====
                case RobotState::BallLaunching:
                    // 0. change game status
                    gameStatusCmd.startGame = true;
                    gameStatusCmd.enableBtnEvent = false;
                    sendGameStatusCtrlCmd(gameStatusCmd);
                    // 1. Request ball loading
                    ballLauncherCmd.type = BallLauncherCtrlCmdType::Loadball;
                    sendBallLauncherCtrlCmd(ballLauncherCmd);
                    break;
                case RobotState::BackHome:
                    // 0. change game status
                    gameStatusCmd.startGame = true;
                    gameStatusCmd.enableBtnEvent = false;
                    sendGameStatusCtrlCmd(gameStatusCmd);
                    // TODO:
                    break;
                case RobotState::CheckHillLoyalty:
                    gameStatusCmd.startGame = true;
                    gameStatusCmd.enableBtnEvent = false;
                    sendGameStatusCtrlCmd(gameStatusCmd);
                    break;
                case RobotState::WaitBallReload:
                    gameStatusCmd.startGame = true;
                    gameStatusCmd.enableBtnEvent = false;
                    sendGameStatusCtrlCmd(gameStatusCmd);
                    break;
                case RobotState::WaitLoyaltyChange:
                    // 1. query beacon state
                    irBeaconDetectCmd.queryBeaconState = true;
                    sendIrBeaconDetectCtrlCmd(irBeaconDetectCmd);
                    break;
#endif
                default:
                    DEBUG_LEVEL_1("[Error] Invalid state transition from Idle");
                    break;
            }
            break;
        case RobotState::MoveToNextJunction:
            switch (newState) {
                case RobotState::Idle:  // timeout
                    gameStatusCmd.startGame = false;
                    gameStatusCmd.enableBtnEvent = true;
                    sendGameStatusCtrlCmd(gameStatusCmd);
                    // 2. deactivate navigation task
                    // TODO:
                    break;
                case RobotState::CheckHillLoyalty:
                    // TODO:
                    // 1. query beacon state
                    irBeaconDetectCmd.queryBeaconState = true;
                    sendIrBeaconDetectCtrlCmd(irBeaconDetectCmd);
                    break;
#if ENABLE_FORCE_STATE_TRANSITION
                case RobotState::ManualControl:
                    break;
                ....
#endif
                default:
                    DEBUG_LEVEL_1("[Error] Invalid state transition from MoveToNextJunction");
                    break;
            }
            break;
        case RobotState::CheckHillLoyalty:
            switch (newState) {
                case RobotState::Idle:  // timeout
                    gameStatusCmd.startGame = false;
                    gameStatusCmd.enableBtnEvent = true;
                    sendGameStatusCtrlCmd(gameStatusCmd);
                    break;
                case RobotState::MoveToNextJunction:
                    // TODO: activate navigation task to move to next junction
                    break;
                case RobotState::BallLaunching:
                    // 1. Request ball loading
                    ballLauncherCmd.type = BallLauncherCtrlCmdType::Loadball;
                    sendBallLauncherCtrlCmd(ballLauncherCmd);
                    break;
#if ENABLE_FORCE_STATE_TRANSITION
                case RobotState::ManualControl:
                    break;
                    ...
#endif
                default:
                    DEBUG_LEVEL_1("[Error] Invalid state transition from CheckHillLoyalty");
                    break;
            }
            break;
        case RobotState::BallLaunching:
            switch (newState) {
                case RobotState::Idle:  // timeout
                    gameStatusCmd.startGame = false;
                    gameStatusCmd.enableBtnEvent = true;
                    sendGameStatusCtrlCmd(gameStatusCmd);
                    // 1. stop ball launcher if it's still shooting
                    ballLauncherCmd.type = BallLauncherCtrlCmdType::Stop;
                    sendBallLauncherCtrlCmd(ballLauncherCmd);
                    break;
                case RobotState::WaitLoyaltyChange:
                    // send query
                    irBeaconDetectCmd.queryBeaconState = true;
                    sendIrBeaconDetectCtrlCmd(irBeaconDetectCmd);
                    break;
                case RobotState::BackHome:
                    // Activate navigation task to go back home
                    break;
#if ENABLE_FORCE_STATE_TRANSITION
                case RobotState::ManualControl:
                    break;
                    ...
#endif
                default:
                    DEBUG_LEVEL_1("[Error] Invalid state transition from BallLaunching");
                    break;
            }
            break;
        case RobotState::WaitLoyaltyChange:
            switch (newState) {
                case RobotState::Idle:  // timeout
                    gameStatusCmd.startGame = false;
                    gameStatusCmd.enableBtnEvent = true;
                    sendGameStatusCtrlCmd(gameStatusCmd);
                    break;
                case RobotState::MoveToNextJunction:
                    // TODO: Activate navigation task to move to next junction
                    break;
                case RobotState::BallLaunching:
                    // 1. Request ball loading
                    ballLauncherCmd.type = BallLauncherCtrlCmdType::Loadball;
                    sendBallLauncherCtrlCmd(ballLauncherCmd);
                    break;
#if ENABLE_FORCE_STATE_TRANSITION
                case RobotState::ManualControl:
                    break;
                    ...
#endif
                default:
                    DEBUG_LEVEL_1("[Error] Invalid state transition from WaitLoyaltyChange");
                    break;
            }
            break;
        case RobotState::BackHome:
            switch (newState) {
                case RobotState::Idle: // timeout
                    gameStatusCmd.startGame = false;
                    gameStatusCmd.enableBtnEvent = true;
                    sendGameStatusCtrlCmd(gameStatusCmd);
                    // TODO: stop navigation task
                    break;
#if ENABLE_FORCE_STATE_TRANSITION
                case RobotState::ManualControl:
                    break;
                    ...
#endif
                default:
                    DEBUG_LEVEL_1("[Error] Invalid state transition from BackHome");
                    break;
            }
            break;
        case RobotState::WaitBallReload:
            switch (newState) {
                case RobotState::Idle: // timeout
                    gameStatusCmd.startGame = false;
                    gameStatusCmd.enableBtnEvent = true;
                    sendGameStatusCtrlCmd(gameStatusCmd);
                    break;
                case RobotState::MoveToNextJunction:
                    // TODO: Activate navigation task to move to next junction
                    break;
#if ENABLE_FORCE_STATE_TRANSITION
                case RobotState::ManualControl:
                    break;
                    ...
#endif
                default:
                    DEBUG_LEVEL_1("[Error] Invalid state transition from WaitBallReload");
                    break;
            }
            break;
        case RobotState::ForceStopped:
            break;
        case RobotState::ManualControl:
            switch (newState) {
                case RobotState::Idle:
                case RobotState::ForceStopped:
                    manualControlCmd.type = ManualControlCmdType::CtrlCmd;
                    manualControlCmd.data.enable = false;
                    sendManualControlCmd(manualControlCmd);
                    
                    // Special handling: ForceStopped -> Idle immediately
                    newState = RobotState::Idle; // force to Idle
                    break;
#if ENABLE_FORCE_STATE_TRANSITION
                ....
#endif
                default:
                    DEBUG_LEVEL_1("[Error] Invalid state transition from ManualControl");
                    break;
            }
            break;
        case RobotState::Error:
            DEBUG_LEVEL_1("In Error state, state transition not implemented yet");
            break;
        // Add other state transitions as needed
        default:
            DEBUG_LEVEL_1("[Error] Invalid old state in transition");
            break;
    }

    fsm.setState(newState);
}

void handleEventWithCurrentState(RobotFSM& fsm, FsmEventQueueItem& ev) {
    GameStatusCtrlCmd gameStatusCmd{};
    IrBeaconDetectCtrlCmd irBeaconDetectCmd{};
    BallLauncherCtrlCmd ballLauncherCmd{};

    switch (fsm.getState()) {
        case RobotState::Idle:
            break;
        case RobotState::MoveToNextJunction:
            break;
        case RobotState::CheckHillLoyalty:
            if (ev.data.newBeaconState == BeaconState::Unknown) {
                // retry
                irBeaconDetectCmd.queryBeaconState = true;
                sendIrBeaconDetectCtrlCmd(irBeaconDetectCmd);
            }
            break;
        case RobotState::BallLaunching:
            if (ev.type == FsmEventType::BallLoaded) {
                // shoot cmd
                ballLauncherCmd.type = BallLauncherCtrlCmdType::Shoot;
                sendBallLauncherCtrlCmd(ballLauncherCmd);
            }
            break;
        case RobotState::WaitLoyaltyChange:
            break;
        case RobotState::BackHome:
            break;
        case RobotState::WaitBallReload:
            break;
        case RobotState::ForceStopped:
            break;
        case RobotState::ManualControl:
            break;
        case RobotState::Error:
            break;
        default:
            DEBUG_LEVEL_1("[Error] Invalid state in event handling");
            break;
    }
}

// ===============================================
// |                                             |
// |           RobotFSM member functions         |
// |                                             |
// ===============================================
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
