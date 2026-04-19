#ifndef FSM_H
#define FSM_H

#include "globals.h"

#define MAX_NOTIF_QUEUE_NUM 10


class RobotFSM {
  public:
    RobotState get_state();
    void set_state(RobotState);
    RobotTeam get_team();
    void toggle_team();
  private:
    RobotState _state = RobotState::Idle;
    RobotTeam _team = RobotTeam::Red;
};

void initFsmTask();


#endif