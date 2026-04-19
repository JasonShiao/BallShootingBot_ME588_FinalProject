#include "game_status.h"
#include "FreeRTOS.h"
#include "queue.h"
#include <Arduino.h>
#include "globals.h"

// declare
void GameStatusTask(void *parameter);

TaskHandle_t GameStatusTaskHandle = NULL;

hw_timer_t* gameTimer = nullptr;
static const uint32_t GAME_DURATION_MS = 20000; // 150000// 2min30s = 150s

static volatile uint32_t last_start_isr_us = 0; // for debounce
volatile bool game_started = false;
//static volatile RobotState curr_state = RobotState::IDLE; 


void ARDUINO_ISR_ATTR onGameStartBtnInterrupt() {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    uint32_t now = micros();

    if ((now - last_start_isr_us) > 50000) {  // 50 ms debounce
        last_start_isr_us = now;

        if (!game_started) {
            FsmEventQueueItem ev{};
            ev.type = FsmEventType::GameStartReq;
            ev.data.startPressed = true;
            BaseType_t ok = xQueueSendFromISR(g_FsmEventQueue, &ev, &xHigherPriorityTaskWoken);
        }
    }
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void ARDUINO_ISR_ATTR onGameTimeoutInterrupt() {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (game_started) {
        FsmEventQueueItem ev{};
        ev.type = FsmEventType::GameTimeoutReq;
        ev.data.startPressed = false;
        BaseType_t ok = xQueueSendFromISR(g_FsmEventQueue, &ev, &xHigherPriorityTaskWoken);
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}


void InitGameStatusTask() {

    // init game start button input
    pinMode(GAME_START_INPUT_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(GAME_START_INPUT_PIN), onGameStartBtnInterrupt, FALLING);

    // init LED status output pin
    pinMode(GAME_STARTED_LED_PIN, OUTPUT);
    digitalWrite(GAME_STARTED_LED_PIN, LOW);

#ifdef TEST_BALL_LAUNCH_DISPLAY
    // for preliminary test only here, should be controlled by ball launcher task
    pinMode(FIRING_LED_PIN, OUTPUT);
    digitalWrite(FIRING_LED_PIN, HIGH);
#endif

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
        GameStatusTask,         // Task function
        "GameStatusTask",       // Task name
        4096,             // Stack size (bytes)
        NULL,              // Parameters
        2,                 // Priority
        &GameStatusTaskHandle,  // Task handle
        1                  // Core 1
    );
}


void GameStatusTask(void *parameter) {
    FsmNotifQueueItem notif_item;

    for(;;) {
        if (xQueueReceive(
                g_FsmNotifQueue[ToIndex(TaskId::GameStatus)], 
                &notif_item, 
                portMAX_DELAY) == pdPASS) {

            DEBUG_LEVEL_1("notif from fsm rcvd by GameStatus");

            switch (notif_item.type) {
                case FsmNotifType::StateChanged:
                    if (!game_started && (notif_item.data.state != RobotState::IDLE)) {
                        game_started = true;
                        digitalWrite(GAME_STARTED_LED_PIN, HIGH);
                        // reset and start timer
                        timerRestart(gameTimer);
                        timerStart(gameTimer);

                        DEBUG_LEVEL_1("Game started.");
                    } else if(game_started && (notif_item.data.state == RobotState::IDLE)) {
                        game_started = false;
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
