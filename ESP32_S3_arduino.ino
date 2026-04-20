#include "globals.h"
#include "team_status.h"
#include "game_status.h"
#include "ball_launcher.h"
#include "ir_beacon_detect.h"
#include "user_interface.h"
//#include "line_follower.h"
#include "fsm.h"

void setup() {
  // put your setup code here, to run once:
  Serial.begin(230400);

  initQueues(); // for inter-task communications

  //initLineFollowerTask();
  initTeamStatusTask(); // RED, BLUE team loyalty and RGB LED indicator
  initGameStatusTask(); // game status: idle, start. green LED indicator
  initBallLauncherTask();
  initIrBeaconDetect();

  initUserInterface();
  
  initFsmTask();
}

void loop() {

}
