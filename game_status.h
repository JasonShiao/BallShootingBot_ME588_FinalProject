#ifndef GAME_STATUS_H
#define GAME_STATUS_H

#define GAME_START_INPUT_PIN 39
#define GAME_STARTED_LED_PIN 40

void initGameStatusTask();

struct GameStatusCtrlCmd {
    bool startGame; // true to start, false to stop
    bool enableBtnEvent; 
};
bool sendGameStatusCtrlCmd(const GameStatusCtrlCmd& cmd);    // other -> fsm task




#endif
