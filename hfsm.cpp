#include "hfsm.h"
#include <Arduino.h>
#include "globals.h"
#include "game_status.h"
#include "team_status.h"
#include "ball_launcher.h"
#include "ir_beacon_detect.h"
#include "user_interface.h"
#include "manual_control.h"

#define CHANGE_STATE_BITMASK (1 << 0)
#define CHANGE_TEAM_BITMASK (1 << 1)
#define CHANGE_BEACON_BITMASK (1 << 2)

void fsmTask(void *parameter);
static TaskHandle_t fsmTaskHandle = nullptr;

static RobotFSM fsm;

// Bubble up the event from the bottom (leaf) state until handled or no parent
EventResult dispatchToHierarchy(const State* s, RobotFSM& fsm, const FsmEventQueueItem& ev) {
    for (const State* p = s; p != nullptr; p = parentOf(p)) {
        DEBUG_LEVEL_2(
            "Dispatch event %s to state %s",
            eventToString(ev.type),
            stateToString(p->id())
        );
        EventResult r = p->handle(fsm, ev);
        if (r.handled) {
            DEBUG_LEVEL_2(
                "Event %s handled by %s",
                eventToString(ev.type),
                stateToString(p->id())
            );
            return r;
        }
    }
    DEBUG_LEVEL_2("Event %s unhandled", eventToString(ev.type));
    return EventResult::unhandled();
}

// Build a state chain from the leaf state to the root (base) state
static int buildPathToRoot(const State* s, const State* path[], int maxDepth) {
    int n = 0;
    while (s && n < maxDepth) {
        path[n++] = s;
        s = parentOf(s);
    }
    return n;
}


RobotFSM::RobotFSM()
    : current_(stateFromId(RobotState::Startup))
{
}

void RobotFSM::begin() {
    current_ = stateFromId(RobotState::Startup);

    const State* path[8];
    int n = buildPathToRoot(current_, path, 8);

    for (int k = n - 1; k >= 0; --k) {
        path[k]->onEnter(*this, nullptr);
    }
}


/**
 *  Process an event by dispatching it to the current state and its hierarchy. 
 *  If the event is handled and a state transition is requested, perform the transition.
 */
void RobotFSM::dispatch(const FsmEventQueueItem& ev) {
    EventResult r = dispatchToHierarchy(current_, *this, ev);

    if (!r.handled) {
        return;
    }

    if (r.changeTeam) {
        setTeam(r.nextTeam);
    }

    if (r.changeBeacon) {
        setBeacon(r.nextBeacon);
    }

    if (r.requestTransition) {
        transitionTo(r.nextState, &ev);
    }
}

/**
 *  Execute the exit from child state until the common parent state of old and new state
 *  then Execute the entry from the common parent state to the new (leaf) state
 */
void RobotFSM::transitionTo(RobotState nextId, const FsmEventQueueItem* ev) {
    const State* next = stateFromId(nextId);
    if (next == current_) return;

    const State* oldPath[8];
    const State* newPath[8];
    int oldN = buildPathToRoot(current_, oldPath, 8);
    int newN = buildPathToRoot(next, newPath, 8);

    int i = oldN - 1;
    int j = newN - 1;
    while (i >= 0 && j >= 0 && oldPath[i] == newPath[j]) {
        --i;
        --j;
    }

    for (int k = 0; k <= i; ++k) {
        DEBUG_LEVEL_2("onExit state %s", stateToString(oldPath[k]->id()));
        oldPath[k]->onExit(*this, ev);
    }

    current_ = next;

    for (int k = j; k >= 0; --k) {
        DEBUG_LEVEL_2("onEnter state %s", stateToString(newPath[k]->id()));
        newPath[k]->onEnter(*this, ev);
    }
}


void initFsmTask() {
    // create the Task
    xTaskCreatePinnedToCore(
        fsmTask,          // Task function
        "FsmTask",        // Task name
        8192,             // Stack size (bytes)
        NULL,              // Parameters
        2,                 // Priority
        &fsmTaskHandle,  // Task handle
        1                  // Core 1
    );
}

void fsmTask(void *parameter) {
    fsm.begin();

    FsmEventQueueItem ev{};
    UserInterfaceUpdateMsg uiUpdateCmd{};

    for (;;) {
        if (receiveFsmEventItem(ev, portMAX_DELAY)) {
            DEBUG_LEVEL_2("FSM task rcvd a new event");

            RobotState oldState = fsm.getState();
            RobotTeam oldTeam = fsm.getTeam();
            BeaconState oldBeacon = fsm.getBeacon();

            fsm.dispatch(ev);

            if (oldState != fsm.getState() ||
                oldTeam != fsm.getTeam() ||
                oldBeacon != fsm.getBeacon()) {

                uiUpdateCmd.currentState = fsm.getState();
                uiUpdateCmd.team = fsm.getTeam();
                uiUpdateCmd.currentBeaconState = fsm.getBeacon();
                sendUserInterfaceUpdate(uiUpdateCmd);
            }
        }
    }
}
