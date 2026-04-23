#ifndef USER_INTERFACE_H
#define USER_INTERFACE_H

void initUserInterface();

struct UserInterfaceUpdateMsg {
    RobotState currentState;
    RobotTeam team;
    BeaconState currentBeaconState;
};

bool sendUserInterfaceUpdate(const UserInterfaceUpdateMsg& msg);

#endif
