#include "team_status.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include <Adafruit_NeoPixel.h>
#include <Arduino.h>

#define NeoPixel_PIN  48 //
#define NUMPIXELS  1 // Popular NeoPixel ring size

// declare
void TeamStatusTask(void *parameter);

Adafruit_NeoPixel pixels(NUMPIXELS, NeoPixel_PIN, NEO_GRB + NEO_KHZ800);

TeamSelect team_sel = TeamSelect::RED;
volatile bool team_change_triggered = false;
SemaphoreHandle_t team_sem = nullptr;

TaskHandle_t TeamStatusTaskHandle = NULL;

void ARDUINO_ISR_ATTR onGPIO21Interrupt() {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (digitalRead(TEAM_SELECT_INPUT_PIN) == HIGH) {
        team_change_triggered = true;
        xSemaphoreGiveFromISR(team_sem, &xHigherPriorityTaskWoken);
    }
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}


void InitTeamStatusTask() {
    pixels.begin();
    pixels.clear();
    pixels.show();

    team_sem = xSemaphoreCreateBinary();
    if (team_sem == nullptr) {
        Serial.println("Failed to create semaphore.");
        while (true) {
            delay(1000);
        }
    }

    // init team select input (SW) pin
    pinMode(TEAM_SELECT_INPUT_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(TEAM_SELECT_INPUT_PIN), onGPIO21Interrupt, RISING);

    pixels.clear();
    if (team_sel == TeamSelect::RED) {
        pixels.setPixelColor(0, pixels.Color(150, 0, 0));
    } else if (team_sel == TeamSelect::BLUE) {
        pixels.setPixelColor(0, pixels.Color(0, 0, 150));
    } else {
        pixels.setPixelColor(0, pixels.Color(0, 0, 0));
    }
    pixels.show(); 

    xTaskCreatePinnedToCore(
        TeamStatusTask,         // Task function
        "TeamStatusTask",       // Task name
        4096,             // Stack size (bytes)
        NULL,              // Parameters
        1,                 // Priority
        &TeamStatusTaskHandle,  // Task handle
        1                  // Core 1
    );

}

void TeamStatusTask(void *parameter) {
    for (;;) { // Infinite loop
        xSemaphoreTake(team_sem, portMAX_DELAY); // block here until semaphore is given
        
        // debounce wait
        vTaskDelay(50 / portTICK_PERIOD_MS);

        if (team_change_triggered) {
            if (team_sel == TeamSelect::RED) {
                team_sel = TeamSelect::BLUE;
            } else {
                team_sel = TeamSelect::RED;
            }

            if (team_sel == TeamSelect::RED) {
                pixels.clear();
                pixels.setPixelColor(0, pixels.Color(150, 0, 0));
                pixels.show();   // Send the updated pixel colors to the hardware.
                Serial.println("Team RED");
            } else {
                pixels.clear();
                pixels.setPixelColor(0, pixels.Color(0, 0, 150));
                pixels.show();   // Send the updated pixel colors to the hardware.
                Serial.println("Team BLUE");
            }

            team_change_triggered = false;
        }
    }
}