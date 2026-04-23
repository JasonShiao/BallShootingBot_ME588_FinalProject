#ifndef USER_INTERFACE_H
#define USER_INTERFACE_H

#include "globals.h"

void initUserInterface();

struct UserInterfaceUpdateMsg {
    RobotState currentState;
    RobotTeam team;
    BeaconState currentBeaconState;
};

bool sendUserInterfaceUpdate(const UserInterfaceUpdateMsg& msg);

#endif
