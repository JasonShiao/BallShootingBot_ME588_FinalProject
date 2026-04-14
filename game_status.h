#define GAME_START_INPUT_PIN 39
#define GAME_STARTED_LED_PIN 40

enum class GameStatus {
  IDLE,
  START
};

void InitGameStatusTask();
void GameStatusTask(void *parameter);