#ifndef MANUAL_CONTROL_H
#define MANUAL_CONTROL_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "queue.h"

void initManualControl();

/* ---------- Manual Control ------------- */
enum class MotionDir {
    Forward,
    ForwardRight,
    ForwardLeft,
    Backward,
    BackwardRight,
    BackwardLeft,
    RotateCW,
    RotateCCW
};

enum class ManualControlCmdType {
    CtrlCmd,
    MotionCmd
};

struct ManualControlMotionData {
    MotionDir dir;
    int speed;
};

struct ManualControlCmd {
    ManualControlCmdType type;
    union {
        bool enable; // for CtrlCmd
        ManualControlMotionData motion; // for MotionCmd
    } data;
};

bool stringToDir(const char* str, MotionDir& dir);

bool sendManualControlCmd(const ManualControlCmd& item);

#endif
