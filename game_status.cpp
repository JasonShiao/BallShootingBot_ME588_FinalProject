#include "game_status.h"
#include "FreeRTOS.h"
#include "queue.h"
#include <Arduino.h>
#include "globals.h"

// declare
void gameStatusTask(void *parameter);
static TaskHandle_t gameStatusTaskHandle = nullptr;
static QueueHandle_t gameStatusCmdQueue = nullptr; // other -> fsm task

hw_timer_t* gameTimer = nullptr;
static const uint32_t GAME_DURATION_MS = 150000; // 150000// 2min30s = 150s

static volatile uint32_t lastStartBtnIsr_us = 0; // for debounce
static volatile bool gameStarted = false;
static volatile bool btnEventEnabled = true;


void ARDUINO_ISR_ATTR onGameStartBtnInterrupt() {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    uint32_t now = micros();

    if ((now - lastStartBtnIsr_us) > 50000) {  // 50 ms debounce
        lastStartBtnIsr_us = now;

        if (btnEventEnabled && g_fsmEventQueue != nullptr) {
            FsmEventQueueItem ev{};
            ev.type = FsmEventType::GameStartReq;
            ev.data.startPressed = true;
            BaseType_t ok = xQueueSendFromISR(g_fsmEventQueue, &ev, &xHigherPriorityTaskWoken);
        }
    }
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void ARDUINO_ISR_ATTR onGameTimeoutInterrupt() {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (gameStarted && g_fsmEventQueue != nullptr) {
        FsmEventQueueItem ev{};
        ev.type = FsmEventType::GameTimeout;
        ev.data.startPressed = false;
        BaseType_t ok = xQueueSendFromISR(g_fsmEventQueue, &ev, &xHigherPriorityTaskWoken);
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}


void initGameStatusTask() {

    // init game start button input
    pinMode(GAME_START_INPUT_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(GAME_START_INPUT_PIN), onGameStartBtnInterrupt, FALLING);

    // init LED status output pin
    pinMode(GAME_STARTED_LED_PIN, OUTPUT);
    digitalWrite(GAME_STARTED_LED_PIN, LOW);

    // init timer for game count down
    gameTimer = timerBegin(1000000); // API v3.0, (freq: 1Mhz -> 1us per counter tick)
    if (gameTimer == nullptr) {
        DEBUG_LEVEL_1("Failed to create timer.");
        while (true) {
            delay(1000);
        }
    }
    timerAttachInterrupt(gameTimer, &onGameTimeoutInterrupt);
    timerAlarm(gameTimer, GAME_DURATION_MS * 1000ULL, false, 0); // one-shot timeout alarm
    timerStop(gameTimer);

    gameStatusCmdQueue = xQueueCreate(5, sizeof(GameStatusCtrlCmd));
    if(gameStatusCmdQueue == nullptr) {
        DEBUG_LEVEL_1("Game status cmd queue created failed");
    }

    // create the Task
    xTaskCreatePinnedToCore(
        gameStatusTask,         // Task function
        "GameStatusTask",       // Task name
        4096,             // Stack size (bytes)
        NULL,              // Parameters
        2,                 // Priority
        &gameStatusTaskHandle,  // Task handle
        1                  // Core 1
    );
}


void gameStatusTask(void *parameter) {
    GameStatusCtrlCmd cmd;

    for(;;) {
        if (xQueueReceive(
                gameStatusCmdQueue, 
                &cmd, 
                portMAX_DELAY) == pdPASS) {

            DEBUG_LEVEL_2("Cmd rcvd by GameStatus");

            btnEventEnabled = cmd.enableBtnEvent;
            if (cmd.startGame) {
                gameStarted = true;
                digitalWrite(GAME_STARTED_LED_PIN, HIGH);
                // reset and start timer
                timerRestart(gameTimer);
                timerStart(gameTimer);
            } else {
                gameStarted = false;
                digitalWrite(GAME_STARTED_LED_PIN, LOW);
                // stop timer
                timerStop(gameTimer);
            }

        }
    }
}

bool sendGameStatusCtrlCmd(const GameStatusCtrlCmd& cmd) {
    if (gameStatusCmdQueue == nullptr) {
        return false;
    }
    return xQueueSend(gameStatusCmdQueue, &cmd, 0) == pdPASS;
}
