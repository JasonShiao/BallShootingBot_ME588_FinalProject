#include "state.h"
#if FLAT_FSM
#include "fsm.h"
#else
#include "hfsm.h"
#endif

#include "game_status.h"
#include "team_status.h"
#include "ball_launcher.h"
#include "ir_beacon_detect.h"
#include "user_interface.h"
#include "manual_control.h"
#include "navigation.h"

StartupState g_startupState;
GameInactiveState g_gameInactiveState;
IdleState g_idleState;
ManualControlState g_manualControlState;
ErrorState g_errorState;
// Active states
GameActiveState g_gameActiveState;
WaitBucketReloadState g_waitBucketReloadState;
//    Navigation states
NavigationState g_navigationState;
MoveToNextJunctionState g_moveToNextJunctionState;
BackHomeState g_backHomeState;
//   Hill interaction states
HillInteractionState g_hillInteractionState;
CheckHillLoyaltyState g_checkHillLoyaltyState;
BallLoadingState g_ballLoadingState;
BallLaunchingState g_ballLaunchingState;


EventResult StartupState::handle(RobotFSM& fsm, const FsmEventQueueItem& ev) const {
    if (ev.type == FsmEventType::StartupDone) {
        EventResult r = EventResult::transition(RobotState::Idle);
        return r;
    } else {
        return EventResult::unhandled();
    }
}

void StartupState::onEnter(RobotFSM& fsm, const FsmEventQueueItem* ev) const {
    // send a StartupDone event to trigger transition to Idle 
    FsmEventQueueItem evNext{};
    evNext.type = FsmEventType::StartupDone;
    sendFsmEventItem(evNext);
}

void StartupState::onExit(RobotFSM& fsm, const FsmEventQueueItem* ev) const {
    // sync team status
    TeamStatusCtrlCmd teamCmd{};
    teamCmd.type = TeamStatusCtrlCmdType::TeamChange;
    teamCmd.data.team = fsm.getTeam();
    sendTeamStatusCtrlCmd(teamCmd);
    teamCmd.type = TeamStatusCtrlCmdType::BtnEventEnableChange;
    teamCmd.data.enableBtnEvent = true;
    sendTeamStatusCtrlCmd(teamCmd);

    // sync game status
    GameStatusCtrlCmd gameCmd{};
    gameCmd.enableBtnEvent = true;
    gameCmd.startGame = false;

    // .... If anything need to sync
}


EventResult GameInactiveState::handle(RobotFSM& fsm, const FsmEventQueueItem& ev) const {
    if (ev.type == FsmEventType::TeamChangeReq) {
        EventResult r = EventResult::stayHandled();
        r.changeTeam = true;
        r.nextTeam = fsm.getOpponentTeam();
        // send to team status task
        TeamStatusCtrlCmd cmd{};
        cmd.type = TeamStatusCtrlCmdType::TeamChange;
        cmd.data.team = r.nextTeam;
        sendTeamStatusCtrlCmd(cmd);
        return r;
    } else if (ev.type == FsmEventType::IrBeaconChangeDetected) {
        EventResult r = EventResult::stayHandled();
        r.changeBeacon = true;
        r.nextBeacon = ev.data.newBeaconState;
        return r;
    } else if (ev.type == FsmEventType::UserStateChangeReq) {
        EventResult r = EventResult::transition(ev.data.newState);
        return r;
    }
    return EventResult::unhandled();
}

void GameInactiveState::onEnter(RobotFSM& fsm, const FsmEventQueueItem* ev) const {
    // 1. turn off game status (LED) and enable Button
    GameStatusCtrlCmd gameStatusCmd{};
    gameStatusCmd.startGame = false;
    gameStatusCmd.enableBtnEvent = true;
    sendGameStatusCtrlCmd(gameStatusCmd);

    // 3. Disable team button
    TeamStatusCtrlCmd teamCmd{};
    teamCmd.type = TeamStatusCtrlCmdType::BtnEventEnableChange;
    teamCmd.data.enableBtnEvent = true;
    sendTeamStatusCtrlCmd(teamCmd);
}

void GameInactiveState::onExit(RobotFSM& fsm, const FsmEventQueueItem* ev) const {
  // do nothing
}

EventResult GameActiveState::handle(
  RobotFSM& fsm, const FsmEventQueueItem& ev) const {
    if (ev.type == FsmEventType::GameTimeout) {
        EventResult r = EventResult::transition(RobotState::Idle);
        return r;
    } else if (ev.type == FsmEventType::IrBeaconChangeDetected) {
        EventResult r = EventResult::stayHandled();
        r.changeBeacon = true;
        r.nextBeacon = ev.data.newBeaconState;
        return r;
    } else if (ev.type == FsmEventType::UserStateChangeReq) {
        EventResult r = EventResult::transition(ev.data.newState);
        return r;
    }
    return EventResult::unhandled();
}


void GameActiveState::onEnter(
  RobotFSM& fsm, const FsmEventQueueItem* ev) const {
    // 1. turn on game status (LED) and disable Button (in the same cmd)
    GameStatusCtrlCmd gameStatusCmd{};
    gameStatusCmd.startGame = true;
    gameStatusCmd.enableBtnEvent = false;
    sendGameStatusCtrlCmd(gameStatusCmd);

    // 2. disable team status Btn 
    TeamStatusCtrlCmd teamCmd{};
    teamCmd.type = TeamStatusCtrlCmdType::BtnEventEnableChange;
    teamCmd.data.enableBtnEvent = false;
    sendTeamStatusCtrlCmd(teamCmd);
}

void GameActiveState::onExit(
  RobotFSM& fsm, const FsmEventQueueItem* ev) const {

}

void HillInteractionState::onEnter(RobotFSM& fsm, const FsmEventQueueItem* ev) const {
    fsm.resetTryCnt();
}

EventResult CheckHillLoyaltyState::handle(RobotFSM& fsm, const FsmEventQueueItem& ev) const {
    if (ev.type == FsmEventType::IrBeaconQueryResponse) {
        const BeaconState newBeacon = ev.data.newBeaconState;

        switch (newBeacon) {
            case BeaconState::Unknown: {
                // Retry mechanism,
                // Reason of unable to detect:
                //  1. no hill
                //  2. IR sensor signal blocked/interfered
                if (fsm.incAndGetTryCnt() >= 4) {
                    // after retrying for 3 times, treat it as no hill and move on
                    EventResult r = EventResult::transition(RobotState::MoveToNextJunction);
                    if (fsm.getBeacon() != ev.data.newBeaconState) {
                        r.changeBeacon = true;
                        r.nextBeacon = newBeacon;
                    }
                    return r;
                } else {
                    // retry without transition
                    IrBeaconDetectCtrlCmd irBeaconDetectCmd{};
                    irBeaconDetectCmd.queryBeaconState = true;
                    sendIrBeaconDetectCtrlCmd(irBeaconDetectCmd);

                    EventResult r = EventResult::stayHandled();
                    if (fsm.getBeacon() != ev.data.newBeaconState) {
                        r.changeBeacon = true;
                        r.nextBeacon = newBeacon;
                    }
                    return r;
                }
            }
            case BeaconState::Beacon750:
            case BeaconState::Beacon1k5: {
                RobotState nextState =
                    isOwnBeacon(fsm.getTeam(), newBeacon)
                        ? RobotState::MoveToNextJunction
                        : RobotState::BallLaunching;
                // Reach retry limit -> proceed to next anyway
                if (!isOwnBeacon(fsm.getTeam(), newBeacon) && 
                    fsm.incAndGetTryCnt() >= 4) {
                    nextState = RobotState::MoveToNextJunction;
                }
                EventResult r = EventResult::transition(nextState);
                if (fsm.getBeacon() != ev.data.newBeaconState) {
                    r.changeBeacon = true;
                    r.nextBeacon = newBeacon;
                }
                return r;
            }
            default:
                DEBUG_LEVEL_1("Invalid beacon state detected in CheckHillLoyaltyState");
                return EventResult::unhandled();
        }
    } else {
        return EventResult::unhandled();
    }
}

void CheckHillLoyaltyState::onEnter(
  RobotFSM& fsm, const FsmEventQueueItem* ev) const {
    IrBeaconDetectCtrlCmd irBeaconDetectCmd{};
    irBeaconDetectCmd.queryBeaconState = true;
    sendIrBeaconDetectCtrlCmd(irBeaconDetectCmd);
}

void CheckHillLoyaltyState::onExit(
  RobotFSM& fsm, const FsmEventQueueItem* ev) const {

}

void ManualControlState::onEnter(
  RobotFSM& fsm, const FsmEventQueueItem* ev) const {
    ManualControlCmd manualControlCmd{};
    manualControlCmd.type = ManualControlCmdType::CtrlCmd;
    manualControlCmd.data.enable = true;
    sendManualControlCmd(manualControlCmd);
}

void ManualControlState::onExit(
  RobotFSM& fsm, const FsmEventQueueItem* ev) const {
    ManualControlCmd manualControlCmd{};
    manualControlCmd.type = ManualControlCmdType::CtrlCmd;
    manualControlCmd.data.enable = false;
    sendManualControlCmd(manualControlCmd);
}

void MoveToNextJunctionState::onEnter(
  RobotFSM& fsm, const FsmEventQueueItem* ev) const {
    // TODO:
    NavigationCmd navCmd{};
    navCmd.activateLineFollower = true;
    sendNavigationCmd(navCmd);
}

void MoveToNextJunctionState::onExit(
  RobotFSM& fsm, const FsmEventQueueItem* ev) const {
    // TODO:
    NavigationCmd navCmd{};
    navCmd.activateLineFollower = false;
    sendNavigationCmd(navCmd);
}

void BackHomeState::onEnter(
  RobotFSM& fsm, const FsmEventQueueItem* ev) const {
    // TODO:
    NavigationCmd navCmd{};
    navCmd.activateLineFollower = true;
    sendNavigationCmd(navCmd);
}

void BackHomeState::onExit(
  RobotFSM& fsm, const FsmEventQueueItem* ev) const {
    // TODO:
    NavigationCmd navCmd{};
    navCmd.activateLineFollower = false;
    sendNavigationCmd(navCmd);
}

// ================== Helper Functions ==================
struct StateMeta {
    RobotState id;
    const State* instance;
    RobotState parent;
    const char* name;
};

static constexpr RobotState NoParent = RobotState::Unknown;

const StateMeta stateTable[] = {
    { RobotState::Startup,            &g_startupState,            NoParent,                    "Startup"},
    { RobotState::GameInactive,       &g_gameInactiveState,       NoParent,                    "GameInactive" },
    { RobotState::Idle,               &g_idleState,               RobotState::GameInactive,    "Idle" },
    { RobotState::ManualControl,      &g_manualControlState,      RobotState::GameInactive,    "ManualControl" },
    { RobotState::Error,              &g_errorState,              RobotState::GameInactive,    "Error" },

    { RobotState::GameActive,         &g_gameActiveState,         NoParent,                    "GameActive" },
    { RobotState::Navigation,         &g_navigationState,         RobotState::GameActive,      "Navigation" },
    { RobotState::MoveToNextJunction, &g_moveToNextJunctionState, RobotState::Navigation,      "MoveToNextJunction" },
    { RobotState::BackHome,           &g_backHomeState,           RobotState::GameActive,      "BackHome" },

    { RobotState::HillInteraction,    &g_hillInteractionState,    RobotState::GameActive,      "HillInteraction" },
    { RobotState::CheckHillLoyalty,   &g_checkHillLoyaltyState,   RobotState::HillInteraction, "CheckHillLoyalty" },
    { RobotState::BallLoading,        &g_ballLoadingState,        RobotState::HillInteraction, "BallLoading" },
    { RobotState::BallLaunching,      &g_ballLaunchingState,      RobotState::HillInteraction, "BallLaunching" },

    { RobotState::WaitBucketReload,   &g_waitBucketReloadState,   RobotState::GameActive,      "WaitBucketReload" },
};

const StateMeta* getStateMeta(RobotState id) {
    for (const auto& s : stateTable) {
        if (s.id == id) return &s;
    }
    return nullptr;
}

const State* stateFromId(RobotState s) {
    const StateMeta* meta = getStateMeta(s);
    return meta ? meta->instance : nullptr;
}

const State* parentOf(const State* s) {
    RobotState id = s->id();
    const StateMeta* meta = getStateMeta(id);

    if (!meta || meta->parent == NoParent) return nullptr;

    const StateMeta* parentMeta = getStateMeta(meta->parent);
    return parentMeta ? parentMeta->instance : nullptr;
}

const char* stateToString(RobotState id) {
    const StateMeta* meta = getStateMeta(id);
    return meta ? meta->name : "Unknown";
}
