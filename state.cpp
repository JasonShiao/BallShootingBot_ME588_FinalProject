#include "state.h"
#include "hfsm.h"

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
        return EventResult::transition(RobotState::Idle);
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
    // sync UI first
    UserInterfaceUpdateMsg uiUpdateCmd{};
    uiUpdateCmd.currentState = fsm.getState();
    uiUpdateCmd.team = fsm.getTeam();
    uiUpdateCmd.currentBeaconState = fsm.getBeacon();
    uiUpdateCmd.location = fsm.getLocation();
    uiUpdateCmd.heading = fsm.getHeading();
    sendUserInterfaceUpdate(uiUpdateCmd);

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
        // toggle team
        fsm.toggleTeam();
        // send to team status task
        TeamStatusCtrlCmd cmd{};
        cmd.type = TeamStatusCtrlCmdType::TeamChange;
        cmd.data.team = fsm.getTeam();
        sendTeamStatusCtrlCmd(cmd);
        return EventResult::stayHandled();
    } else if (ev.type == FsmEventType::IrBeaconChangeDetected) {
        fsm.setBeacon(ev.data.newBeaconState);
        return EventResult::stayHandled();
    } else if (ev.type == FsmEventType::UserStateChangeReq) {
        return EventResult::transition(ev.data.newState);
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
        return EventResult::transition(RobotState::Idle);
    } else if (ev.type == FsmEventType::IrBeaconChangeDetected) {
        fsm.setBeacon(ev.data.newBeaconState);
        return EventResult::stayHandled();
    } else if (ev.type == FsmEventType::UserStateChangeReq) {
        return EventResult::transition(ev.data.newState);
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

    // Reset location to home when exiting game activate state
    fsm.setLocation(RobotLocation::Home);
}

EventResult IdleState::handle(RobotFSM& fsm, const FsmEventQueueItem& ev) const {
    if (ev.type == FsmEventType::GameStartReq) {
        return EventResult::transition(RobotState::MoveToNextJunction);
    }
    return EventResult::unhandled();
}

void IdleState::onEnter(RobotFSM& fsm, const FsmEventQueueItem* ev) const {

}

void IdleState::onExit(RobotFSM& fsm, const FsmEventQueueItem* ev) const {

}

void HillInteractionState::onEnter(RobotFSM& fsm, const FsmEventQueueItem* ev) const {
    fsm.resetTryCnt();
}

void HillInteractionState::onExit(RobotFSM& fsm, const FsmEventQueueItem* ev) const {

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
                    fsm.setBeacon(newBeacon);
                    return EventResult::transition(RobotState::MoveToNextJunction);
                } else {
                    fsm.setBeacon(newBeacon);
                    // retry without transition
                    IrBeaconDetectCtrlCmd irBeaconDetectCmd{};
                    irBeaconDetectCmd.queryBeaconState = true;
                    sendIrBeaconDetectCtrlCmd(irBeaconDetectCmd);
                    return EventResult::stayHandled();
                }
            }
            case BeaconState::Beacon750:
            case BeaconState::Beacon1k5: {
                RobotState nextState =
                    isOwnBeacon(fsm.getTeam(), newBeacon)
                        ? RobotState::MoveToNextJunction
                        : RobotState::BallLoading;
                // Reach retry limit -> proceed to next anyway
                if (!isOwnBeacon(fsm.getTeam(), newBeacon) && 
                    fsm.incAndGetTryCnt() >= 4) {
                    nextState = RobotState::MoveToNextJunction;
                }
                fsm.setBeacon(ev.data.newBeaconState);
                return EventResult::transition(nextState);
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

EventResult BallLoadingState::handle(
  RobotFSM& fsm, const FsmEventQueueItem& ev) const {
    if (ev.type == FsmEventType::BallLoaded) {
        return EventResult::transition(RobotState::BallLaunching);
    } else if (ev.type == FsmEventType::BucketEmptyDetected) {
        return EventResult::transition(RobotState::BackHome);
    }

    return EventResult::unhandled();
}

void BallLoadingState::onEnter(
  RobotFSM& fsm, const FsmEventQueueItem* ev) const {
    BallLauncherCtrlCmd cmd{};
    cmd.type = BallLauncherCtrlCmdType::Loadball;
    sendBallLauncherCtrlCmd(cmd);
}

void BallLoadingState::onExit(
  RobotFSM& fsm, const FsmEventQueueItem* ev) const {
    BallLauncherCtrlCmd cmd{};
    cmd.type = BallLauncherCtrlCmdType::Stop;
    sendBallLauncherCtrlCmd(cmd);
}

EventResult BallLaunchingState::handle(
  RobotFSM& fsm, const FsmEventQueueItem& ev) const {
    if (ev.type == FsmEventType::BallLaunched) {
        return EventResult::transition(RobotState::CheckHillLoyalty);
    }

    return EventResult::unhandled();
}

void BallLaunchingState::onEnter(RobotFSM& fsm, const FsmEventQueueItem* ev) const {
    BallLauncherCtrlCmd cmd{};
    cmd.type = BallLauncherCtrlCmdType::Shoot;
    sendBallLauncherCtrlCmd(cmd);
}

void BallLaunchingState::onExit(RobotFSM& fsm, const FsmEventQueueItem* ev) const {
    BallLauncherCtrlCmd cmd{};
    cmd.type = BallLauncherCtrlCmdType::Stop;
    sendBallLauncherCtrlCmd(cmd);
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

EventResult MoveToNextJunctionState::handle(
  RobotFSM& fsm, const FsmEventQueueItem& ev) const {
    // Junction detect event handling
    if (ev.type == FsmEventType::JunctionCrossed) {
        DEBUG_LEVEL_2("Junction Crossed ev rcvd in MoveToNextJunctionState");
        RobotLocation newLocation = determineNewLocation(
            fsm.getLocation(), fsm.getHeading(),
            ev.data.junctionCrossed, fsm.getTeam());
        if (newLocation == fsm.getLocation()) {
            return EventResult::stayHandled();
        }

        // Candidate Hill location (may not exist, need to check IR beacon)
        if ((newLocation == RobotLocation::HomeToJunction1 && 
            fsm.getHeading() == RobotHeading::Backward) ||
            newLocation == RobotLocation::Junction1ToJunction2 ||
            newLocation == RobotLocation::Junction2ToJunction3 ||
            newLocation == RobotLocation::Junction3ToJunction4 ||
            newLocation == RobotLocation::Junction4ToRoadEnd) {
            fsm.setLocation(newLocation);
            return EventResult::transition(RobotState::CheckHillLoyalty);
        }
        // Non-hill location (e.g. Home), keep going (same direction)
        fsm.setLocation(newLocation);
        return EventResult::stayHandled();
    }

    return EventResult::unhandled();
}

void MoveToNextJunctionState::onEnter(
  RobotFSM& fsm, const FsmEventQueueItem* ev) const {
    // Rules for determine new heading direction:
    // 1. Restrict the robot inside the Hills boundary
    // 2. If near Home, forward only
    if (fsm.getLocation() == RobotLocation::HomeToJunction1) {
        if (fsm.getHeading() != RobotHeading::Forward) {
            // change to forward
            fsm.setHeading(RobotHeading::Forward);
        }
    } else if (fsm.getLocation() == RobotLocation::Junction4ToRoadEnd) {
        if (fsm.getHeading() != RobotHeading::Backward) {
            // change to forward
            fsm.setHeading(RobotHeading::Backward);
        }
    } else if (fsm.getLocation() == RobotLocation::Home ||
                fsm.getLocation() == RobotLocation::HomeBorderCrossed1 ||
                fsm.getLocation() == RobotLocation::HomeBorderCrossed2) {
        // Forward only
        if (fsm.getHeading() != RobotHeading::Forward) {
            // change to forward
            fsm.setHeading(RobotHeading::Forward);
        }
    }

    // Activate line follower with combined (heading info & team info)
    // For navigation subsystem:
    //     Team Blue backward = Team Red forward
    //     Team Blue forward = Team Red backward
    NavigationCmd navCmd{};
    navCmd.activateLineFollower = true;
    if (fsm.getTeam() == RobotTeam::Red) {
        navCmd.headingSwapped = (fsm.getHeading() == RobotHeading::Forward);
    } else {
        navCmd.headingSwapped = (fsm.getHeading() == RobotHeading::Backward);
    }
    sendNavigationCmd(navCmd);
}

void MoveToNextJunctionState::onExit(
  RobotFSM& fsm, const FsmEventQueueItem* ev) const {

    NavigationCmd navCmd{};
    navCmd.activateLineFollower = false;
    sendNavigationCmd(navCmd);
}

EventResult BackHomeState::handle(
  RobotFSM& fsm, const FsmEventQueueItem& ev) const {
    // Wait junction detect event
    if (ev.type == FsmEventType::JunctionCrossed) {
        DEBUG_LEVEL_1("Junction Crossed ev rcvd in BackHomeState");

        RobotLocation newLocation = determineNewLocation(
            fsm.getLocation(), fsm.getHeading(),
            ev.data.junctionCrossed, fsm.getTeam());
        if (newLocation == fsm.getLocation()) {
            return EventResult::stayHandled();
        }

        if (newLocation == RobotLocation::Home) {
            fsm.setLocation(newLocation);
            return EventResult::transition(RobotState::WaitBucketReload);
        }
        // Change location but keep in BackHome state
        fsm.setLocation(newLocation);
        return EventResult::stayHandled();
    }

    return EventResult::unhandled();
}

void BackHomeState::onEnter(
  RobotFSM& fsm, const FsmEventQueueItem* ev) const {
    // Backward to home
    fsm.setHeading(RobotHeading::Backward);

    // Activate line follower with combined (heading info & team info)
    // For navigation subsystem:
    //     Team Blue backward = Team Red forward
    //     Team Blue forward = Team Red backward
    NavigationCmd navCmd{};
    navCmd.activateLineFollower = true;
    if (fsm.getTeam() == RobotTeam::Red) {
        navCmd.headingSwapped = (fsm.getHeading() == RobotHeading::Forward);
    } else {
        navCmd.headingSwapped = (fsm.getHeading() == RobotHeading::Backward);
    }
    sendNavigationCmd(navCmd);
}

void BackHomeState::onExit(
  RobotFSM& fsm, const FsmEventQueueItem* ev) const {

    NavigationCmd navCmd{};
    navCmd.activateLineFollower = false;
    sendNavigationCmd(navCmd);
}

EventResult WaitBucketReloadState::handle(
  RobotFSM& fsm, const FsmEventQueueItem& ev) const {
    // Wait for Ball reload timeout event
    if (ev.type == FsmEventType::BucketReloadTimeout) {
        // transit to move to next junction
        return EventResult::transition(RobotState::MoveToNextJunction);
    }
    return EventResult::unhandled();
}

void WaitBucketReloadState::onEnter(
  RobotFSM& fsm, const FsmEventQueueItem* ev) const {
    // Start a timer for bucket reload timeout
    BallLauncherCtrlCmd cmd{};
    cmd.type = BallLauncherCtrlCmdType::StartBucketReload;
    sendBallLauncherCtrlCmd(cmd);
}

void WaitBucketReloadState::onExit(
  RobotFSM& fsm, const FsmEventQueueItem* ev) const {
    // Stop the timer if it's still running
    BallLauncherCtrlCmd cmd{};
    cmd.type = BallLauncherCtrlCmdType::Stop;
    sendBallLauncherCtrlCmd(cmd);
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
