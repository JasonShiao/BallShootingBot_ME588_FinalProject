#ifndef FSM_H
#define FSM_H

#include "globals.h"

#define MAX_NOTIF_QUEUE_NUM 10

struct NewStates {
    RobotState state;
    RobotTeam team;
    BeaconState beacon;
};

class RobotFSM {
  public:
    RobotState getState();
    void setState(RobotState);
    RobotTeam getTeam();
    RobotTeam getOpponentTeam() {
        return (getTeam() == RobotTeam::Blue) ? RobotTeam::Red : RobotTeam::Blue;
    }
    void toggleTeam();
    void setBeacon(BeaconState);
    BeaconState getBeacon();

    void resetTryCnt() {
        _try_cnt = 0;
    }
    uint16_t incAndGetTryCnt() {
        return ++_try_cnt;
    }
  private:
    RobotState _state = RobotState::Idle;
    RobotTeam _team = RobotTeam::Red;
    BeaconState _beacon = BeaconState::Unknown;
    uint16_t _try_cnt = 0;
};

void initFsmTask();


#endif