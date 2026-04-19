#include "game_status.h"
#include "FreeRTOS.h"
#include "queue.h"
#include <Arduino.h>
#include "globals.h"

// declare
void gameStatusTask(void *parameter);
TaskHandle_t gameStatusTaskHandle = NULL;

hw_timer_t* gameTimer = nullptr;
static const uint32_t GAME_DURATION_MS = 150000; // 150000// 2min30s = 150s

static volatile uint32_t lastStartIsr_us = 0; // for debounce
volatile bool gameStarted = false;


void ARDUINO_ISR_ATTR onGameStartBtnInterrupt() {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    uint32_t now = micros();

    if ((now - lastStartIsr_us) > 50000) {  // 50 ms debounce
        lastStartIsr_us = now;

        if (!gameStarted) {
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

    if (gameStarted) {
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
    FsmNotifQueueItem notif_item;

    for(;;) {
        if (xQueueReceive(
                g_fsmNotifQueue[toIndex(TaskId::GameStatus)], 
                &notif_item, 
                portMAX_DELAY) == pdPASS) {

            DEBUG_LEVEL_2("notif from fsm rcvd by GameStatus");

            switch (notif_item.type) {
                case FsmNotifType::StateChanged:
                    if (!gameStarted && (notif_item.data.state != RobotState::Idle)) {
                        gameStarted = true;
                        digitalWrite(GAME_STARTED_LED_PIN, HIGH);
                        // reset and start timer
                        timerRestart(gameTimer);
                        timerStart(gameTimer);

                        DEBUG_LEVEL_1("Game started.");
                    } else if (gameStarted && (notif_item.data.state == RobotState::Idle)) {
                        gameStarted = false;
                        digitalWrite(GAME_STARTED_LED_PIN, LOW);
                        // stop timer in case
                        timerStop(gameTimer);

                        DEBUG_LEVEL_1("Game stopped / returned to IDLE.");
                    }
                    break;
                default:
                    break;
            }
        }
    }
}
