
enum class RobotState {
    IDLE,
    STARTED;
};

class RobotFSM {
  public:
    RobotState get_state();
  private:
    RobotState _state = RobotState::IDLE;
}

extern RobotFSM fsm;