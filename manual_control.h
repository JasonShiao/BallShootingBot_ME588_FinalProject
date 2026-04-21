#ifndef MANUAL_CONTROL_H
#define MANUAL_CONTROL_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "queue.h"

extern QueueHandle_t g_manualControlQueue; // send for manual control task only

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

struct ManualControlMotionData {
    MotionDir dir;
    int speed;
};

struct ManualControlMotionQueueItem {
    ManualControlMotionData data;
};

bool stringToDir(const char* str, MotionDir& dir);

#endif
