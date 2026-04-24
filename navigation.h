#ifndef NAVIGATION_H
#define NAVIGATION_H

#include "stdint.h"

#define LINE_FOLLOWER_LINE_COUNT 4

#define LINE_FOLLOWER_PIN_1 1
#define LINE_FOLLOWER_PIN_2 2
#define LINE_FOLLOWER_PIN_3 42
#define LINE_FOLLOWER_PIN_4 41

#define JUNCTION_DETECTOR_PIN_1 4
#define JUNCTION_DETECTOR_PIN_2 5

void initNavigation();

uint16_t getLinePosition();

// TODO: add a new command for blindly forward for certain duration 
// (for back home state / cross junction ...)
struct NavigationCmd {
    bool activateLineFollower;
};

bool sendNavigationCmd(NavigationCmd cmd);

#endif
