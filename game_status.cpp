#include "game_status.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include <Arduino.h>

// declare
void GameStatusTask(void *parameter);

bool game_started = false;
SemaphoreHandle_t start_btn_sem = nullptr;
SemaphoreHandle_t game_timeout_sem = nullptr;

volatile bool start_btn_triggered = false;
volatile bool game_timeout_triggered = false;

TaskHandle_t GameStatusTaskHandle = NULL;

hw_timer_t* gameTimer = nullptr;
static const uint32_t GAME_DURATION_MS = 20000; // 150000// 2min30s = 150s

void ARDUINO_ISR_ATTR onGPIO39Interrupt() {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (!game_started) {
        if (digitalRead(GAME_START_INPUT_PIN) == LOW) {
            start_btn_triggered = true;
            xSemaphoreGiveFromISR(start_btn_sem, &xHigherPriorityTaskWoken);
        }
    }
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void ARDUINO_ISR_ATTR onGameTimeoutInterrupt() {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    game_timeout_triggered = true;
    xSemaphoreGiveFromISR(game_timeout_sem, &xHigherPriorityTaskWoken);

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}


void InitGameStatusTask() {
    // create semaphores first
    start_btn_sem = xSemaphoreCreateBinary();
    game_timeout_sem = xSemaphoreCreateBinary();
    if (start_btn_sem == nullptr || game_timeout_sem == nullptr) {
        Serial.println("Failed to create semaphore.");
        while (true) {
            delay(1000);
        }
    }

    // init game start button input
    pinMode(GAME_START_INPUT_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(GAME_START_INPUT_PIN), onGPIO39Interrupt, FALLING);

    // init LED status output pin
    pinMode(GAME_STARTED_LED_PIN, OUTPUT);
    digitalWrite(GAME_STARTED_LED_PIN, LOW);

    pinMode(FIRING_LED_PIN, OUTPUT);
    digitalWrite(FIRING_LED_PIN, HIGH);

    // init timer for game count down
    gameTimer = timerBegin(1000000); // API v3.0, (freq: 1Mhz -> 1us)
    if (gameTimer == nullptr) {
        Serial.println("Failed to create timer.");
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
    for(;;) {
        if (!game_started) { // idle -> start
            xSemaphoreTake(start_btn_sem, portMAX_DELAY); // block here until semaphore is given
            // debounce
            vTaskDelay(30 / portTICK_PERIOD_MS);

            if (start_btn_triggered) {
                game_started = true;
                digitalWrite(GAME_STARTED_LED_PIN, HIGH);
                Serial.println("Game started.");

                // reset and start timer
                timerRestart(gameTimer);
                timerStart(gameTimer);

                // reset btn trigger flag
                start_btn_triggered = false;
            }

        } else { // wait for game count down finish
            xSemaphoreTake(game_timeout_sem, portMAX_DELAY); // block here until semaphore is given
            // debounce
            vTaskDelay(30 / portTICK_PERIOD_MS);

            if (game_timeout_triggered) {
                game_started = false;
                digitalWrite(GAME_STARTED_LED_PIN, LOW);
                Serial.println("Game timeout.");

                // stop timer in case
                timerStop(gameTimer);

                // reset timeout flag
                game_timeout_triggered = false;
            }

        }

    }
}