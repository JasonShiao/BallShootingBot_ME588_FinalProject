#ifndef TEAM_STATUS_H
#define TEAM_STATUS_H

#include "globals.h"

#define TEAM_SELECT_INPUT_PIN 21

void initTeamStatusTask();

enum class TeamStatusCtrlCmdType {
    TeamChange,
    BtnEventEnableChange
};

struct TeamStatusCtrlCmd {
    TeamStatusCtrlCmdType type;
    union {
        RobotTeam team; // for TeamChange
        bool enableBtnEvent; // for BtnEventEnableChange
    } data;
};

bool sendTeamStatusCtrlCmd(const TeamStatusCtrlCmd& cmd);

#endif
