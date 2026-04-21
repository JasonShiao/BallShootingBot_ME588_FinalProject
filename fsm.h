#ifndef FSM_H
#define FSM_H

#include "globals.h"

#define MAX_NOTIF_QUEUE_NUM 10


class RobotFSM {
  public:
    RobotState getState();
    void setState(RobotState);
    RobotTeam getTeam();
    void toggleTeam();
    void setBeacon(BeaconState);
    BeaconState getBeacon();
  private:
    RobotState _state = RobotState::Idle;
    RobotTeam _team = RobotTeam::Red;
    BeaconState _beacon = BeaconState::Unknown;
};

void initFsmTask();


#endif