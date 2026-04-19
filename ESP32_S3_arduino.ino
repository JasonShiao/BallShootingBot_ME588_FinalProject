#include "globals.h"
#include "team_status.h"
#include "game_status.h"
//#include "line_follower.h"
#include "fsm.h"

void setup() {
  // put your setup code here, to run once:
  Serial.begin(230400);

  initQueues(); // for inter-task communications

  //InitLineFollowerTask();
  InitTeamStatusTask(); // RED, BLUE team selection and RGB LED indicator
  InitGameStatusTask(); // game status: idle, start. green LED indicator
  
  InitFsmTask();
}

void loop() {

}
