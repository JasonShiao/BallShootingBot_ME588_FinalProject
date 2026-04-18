#define GAME_START_INPUT_PIN 39
#define GAME_STARTED_LED_PIN 40

#define FIRING_LED_PIN 18

enum class GameStatus {
  IDLE,
  START
};

void InitGameStatusTask();
