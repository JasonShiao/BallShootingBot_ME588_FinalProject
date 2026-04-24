#include "globals.h"
#include "team_status.h"
#include "game_status.h"
#include "ball_launcher.h"
#include "ir_beacon_detect.h"
#include "mobility.h"
//#include "line_follower.h"
#include "navigation.h"
#if FLAT_FSM
#include "fsm.h"
#else
#include "hfsm.h"
#endif
#ifndef FULLY_AUTONOMOUS
  #include "manual_control.h"
  #include "user_interface.h"
#endif

void setup() {
  // put your setup code here, to run once:
  Serial.begin(230400);

  initQueues(); // for inter-task communications

  initNavigation();
  initTeamStatusTask(); // RED, BLUE team loyalty and RGB LED indicator
  initGameStatusTask(); // game status: idle, start. green LED indicator
  initBallLauncherTask();
  initIrBeaconDetect();
  initMobility(); // for motor ctrl

#ifndef FULLY_AUTONOMOUS
  initManualControl();
  initUserInterface();
#endif
  
  initFsmTask(); // WARNING: Fsm MUST be the last init
}

void loop() {

}
