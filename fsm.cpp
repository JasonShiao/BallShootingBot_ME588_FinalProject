#include "fsm.h"

RobotFSM fsm;

RobotState RobotFSM::get_state() {
  return _state;
}
