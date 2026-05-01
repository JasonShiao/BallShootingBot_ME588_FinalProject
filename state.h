#ifndef STATE_H
#define STATE_H

#include "globals.h"

struct EventResult {
    bool handled = false;
    RobotState nextState;
    bool requestTransition = false;

    // static helper functions work without an instance
    static EventResult unhandled() { return {}; }

    static EventResult stayHandled() {
        EventResult r;
        r.handled = true;
        return r;
    }

    static EventResult transition(RobotState s) {
        EventResult r;
        r.handled = true;
        r.requestTransition = true;
        r.nextState = s;
        return r;
    }
};

class RobotFSM;

class State {
 public:
    virtual ~State() = default;

    virtual RobotState id() const = 0;

    virtual void onEnter(RobotFSM& fsm, const FsmEventQueueItem* ev) const {}
    virtual void onExit(RobotFSM& fsm, const FsmEventQueueItem* ev) const {}

    virtual EventResult handle(RobotFSM& fsm, const FsmEventQueueItem& ev) const {
        return EventResult::unhandled();
    }
};

// ==================================================================
// |                                                                |
// |                                                                |
// |                     Intermediate states                        |
// |                                                                |
// |                                                                |
// ==================================================================
class GameInactiveState : public State {
 public:
    RobotState id() const override { return RobotState::GameInactive; }

    EventResult handle(RobotFSM& fsm, const FsmEventQueueItem& ev) const override;
    void onEnter(RobotFSM& fsm, const FsmEventQueueItem* ev) const override;
    void onExit(RobotFSM& fsm, const FsmEventQueueItem* ev) const override;
};

class GameActiveState : public State {
 public:
    RobotState id() const override { return RobotState::GameActive; }

    EventResult handle(RobotFSM& fsm, const FsmEventQueueItem& ev) const override;

    void onEnter(RobotFSM& fsm, const FsmEventQueueItem* ev) const override;
    void onExit(RobotFSM& fsm, const FsmEventQueueItem* ev) const override;
};

class NavigationState : public GameActiveState {
 public:
    RobotState id() const override { return RobotState::Navigation; }

    EventResult handle(RobotFSM& fsm, const FsmEventQueueItem& ev) const override {
        return EventResult::unhandled();
    }
    void onEnter(RobotFSM& fsm, const FsmEventQueueItem* ev) const override {
    }
    void onExit(RobotFSM& fsm, const FsmEventQueueItem* ev) const override {
    }
};

class HillInteractionState : public GameActiveState {
 public:
    RobotState id() const override { return RobotState::HillInteraction; }

    //EventResult handle(RobotFSM& fsm, const FsmEventQueueItem& ev) const override;
    void onEnter(RobotFSM& fsm, const FsmEventQueueItem* ev) const override;
    void onExit(RobotFSM& fsm, const FsmEventQueueItem* ev) const override;
};

// ==================================================================
// |                                                                |
// |                                                                |
// |                    Concrete leaf states                        |
// |                                                                |
// |                                                                |
// ==================================================================
class StartupState : public State {
 public:
    RobotState id() const override { return RobotState::Startup; }

    EventResult handle(RobotFSM& fsm, const FsmEventQueueItem& ev) const override;
    void onEnter(RobotFSM& fsm, const FsmEventQueueItem* ev) const override;
    void onExit(RobotFSM& fsm, const FsmEventQueueItem* ev) const override;
};

class IdleState : public GameInactiveState {
  public:
    RobotState id() const override { return RobotState::Idle; }

    EventResult handle(RobotFSM& fsm, const FsmEventQueueItem& ev) const override;

    void onEnter(RobotFSM& fsm, const FsmEventQueueItem* ev) const override;
    void onExit(RobotFSM& fsm, const FsmEventQueueItem* ev) const override;
};

class ManualControlState : public GameInactiveState {
  public:
    RobotState id() const override { return RobotState::ManualControl; }

    //EventResult handle(RobotFSM& fsm, const FsmEventQueueItem& ev) const override;

    void onEnter(RobotFSM& fsm, const FsmEventQueueItem* ev) const override;
    void onExit(RobotFSM& fsm, const FsmEventQueueItem* ev) const override;
};

class ErrorState : public GameInactiveState {
  public:
    RobotState id() const override { return RobotState::Error; }

    //EventResult handle(RobotFSM& fsm, const FsmEventQueueItem& ev) const override;
    void onEnter(RobotFSM& fsm, const FsmEventQueueItem* ev) const override {
        DEBUG_LEVEL_1("Entered Error state");
    }
    void onExit(RobotFSM& fsm, const FsmEventQueueItem* ev) const override {
        DEBUG_LEVEL_1("Exiting Error state");
    }
};


class MoveToNextJunctionState : public NavigationState {
  public:
    RobotState id() const override { return RobotState::MoveToNextJunction; }

    EventResult handle(RobotFSM& fsm, const FsmEventQueueItem& ev) const override;

    void onEnter(RobotFSM& fsm, const FsmEventQueueItem* ev) const override;
    void onExit(RobotFSM& fsm, const FsmEventQueueItem* ev) const override;
};

class BackHomeState : public GameActiveState {
  public:
    RobotState id() const override { return RobotState::BackHome; }

    EventResult handle(RobotFSM& fsm, const FsmEventQueueItem& ev) const override;

    void onEnter(RobotFSM& fsm, const FsmEventQueueItem* ev) const override;
    void onExit(RobotFSM& fsm, const FsmEventQueueItem* ev) const override;
};

class CheckHillLoyaltyState : public HillInteractionState {
  public:
    RobotState id() const override { return RobotState::CheckHillLoyalty; }

    EventResult handle(RobotFSM& fsm, const FsmEventQueueItem& ev) const override;

    void onEnter(RobotFSM& fsm, const FsmEventQueueItem* ev) const override;
    void onExit(RobotFSM& fsm, const FsmEventQueueItem* ev) const override;
};

class BallLoadingState: public HillInteractionState {
 public:
    RobotState id() const override { return RobotState::BallLoading; }

    EventResult handle(RobotFSM& fsm, const FsmEventQueueItem& ev) const override;

    void onEnter(RobotFSM& fsm, const FsmEventQueueItem* ev) const override;
    void onExit(RobotFSM& fsm, const FsmEventQueueItem* ev) const override;
};

class BallLaunchingState: public HillInteractionState {
 public:
    RobotState id() const override { return RobotState::BallLaunching; }

    EventResult handle(RobotFSM& fsm, const FsmEventQueueItem& ev) const override;

    void onEnter(RobotFSM& fsm, const FsmEventQueueItem* ev) const override;
    void onExit(RobotFSM& fsm, const FsmEventQueueItem* ev) const override;
};

class WaitBucketReloadState : public GameActiveState {
  public:
    RobotState id() const override { return RobotState::WaitBucketReload; }

    EventResult handle(RobotFSM& fsm, const FsmEventQueueItem& ev) const override;

    void onEnter(RobotFSM& fsm, const FsmEventQueueItem* ev) const override;
    void onExit(RobotFSM& fsm, const FsmEventQueueItem* ev) const override;
};


const State* stateFromId(RobotState s);
const State* parentOf(const State* s);
const char* stateToString(RobotState id);

#endif
