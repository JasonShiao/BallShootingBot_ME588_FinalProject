#ifndef HFSM_H
#define HFSM_H

#include "globals.h"
#include "state.h"

class RobotFSM {
public:
    RobotFSM();

    void dispatch(const FsmEventQueueItem& ev);
    void transitionTo(RobotState next, const FsmEventQueueItem* ev);

    RobotState getState() const { return current_->id(); }
    RobotTeam getTeam() const { return team_; }
    BeaconState getBeacon() const { return beacon_; }

    void setTeam(RobotTeam t) { team_ = t; }
    void toggleTeam() { team_ = (team_ == RobotTeam::Blue) ? RobotTeam::Red : RobotTeam::Blue; }
    RobotTeam getOpponentTeam() const {
        return (team_ == RobotTeam::Blue) ? RobotTeam::Red : RobotTeam::Blue;
    }

    void setBeacon(BeaconState b) { beacon_ = b; }

    // worker-command helpers
    // void sendGameStatus(bool start, bool enableBtn);
    // void sendNavigationEnable(bool enable);
    // void sendBeaconQuery();
    // void sendBallLoadCmd();
    // void sendBallStopCmd();
    // void sendShootCmd();
    // void sendManualControlEnable(bool enable);

    void resetTryCnt() {
        tryCnt_ = 0;
    }
    int incAndGetTryCnt() {
        return ++tryCnt_;
    }

private:
    const State* current_;
    RobotTeam team_ = RobotTeam::Red;
    BeaconState beacon_ = BeaconState::Unknown;
    int tryCnt_ = 0; // try count for any process
};

void initFsmTask();

#endif