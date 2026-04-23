#ifndef TEAM_STATUS_H
#define TEAM_STATUS_H

#include "globals.h"

#define TEAM_SELECT_INPUT_PIN 21

void initTeamStatusTask();

struct TeamStatusCtrlCmd {
    RobotTeam team;
    bool enableBtnEvent;
};

bool sendTeamStatusCtrlCmd(const TeamStatusCtrlCmd& cmd);

#endif
