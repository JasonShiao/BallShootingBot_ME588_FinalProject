#include "team_status.h"
#include "FreeRTOS.h"
#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include "fsm.h"
#include "queue.h"
#include "globals.h"


#define NeoPixel_PIN  48 //
#define NUMPIXELS  1 // Popular NeoPixel ring size

// declare
void teamStatusTask(void *parameter);
TaskHandle_t teamStatusTaskHandle = NULL;

Adafruit_NeoPixel pixels(NUMPIXELS, NeoPixel_PIN, NEO_GRB + NEO_KHZ800);

static volatile uint32_t lastBtnIsr_us = 0; // for debounce
// keep a copy of state inside (might be older then latest fsm state for a short time)
static volatile RobotState currState = RobotState::Idle; 
static RobotTeam currTeam = RobotTeam::Red;


void ARDUINO_ISR_ATTR onTeamSwitchBtnInterrupt() {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    uint32_t now = micros();
    if ((now - lastBtnIsr_us) > 50000) {  // 50 ms debounce
        lastBtnIsr_us = now;
        
        if (currState == RobotState::Idle) {
            // Send team change to queue
            FsmEventQueueItem ev{};
            ev.type = FsmEventType::TeamChangeReq;
            ev.data.teamChanged = true;
            BaseType_t ok = xQueueSendFromISR(g_fsmEventQueue, &ev, &xHigherPriorityTaskWoken);
        }
    }
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}


void initTeamStatusTask() {
    pixels.begin();
    pixels.clear();
    pixels.show();

    // init team select input (SW) pin
    pinMode(TEAM_SELECT_INPUT_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(TEAM_SELECT_INPUT_PIN), onTeamSwitchBtnInterrupt, RISING);

    pixels.clear();
    if (currTeam == RobotTeam::Red) {
        pixels.setPixelColor(0, pixels.Color(150, 0, 0));
    } else if (currTeam == RobotTeam::Blue) {
        pixels.setPixelColor(0, pixels.Color(0, 0, 150));
    } else {
        pixels.setPixelColor(0, pixels.Color(150, 150, 150));
    }
    pixels.show(); 

    xTaskCreatePinnedToCore(
        teamStatusTask,         // Task function
        "TeamStatusTask",       // Task name
        2048,             // Stack size (bytes)
        NULL,              // Parameters
        1,                 // Priority
        &teamStatusTaskHandle,  // Task handle
        1                  // Core 1
    );

}

void teamStatusTask(void *parameter) {
    FsmNotifQueueItem notif_item;

    for (;;) { // Infinite loop
        if (xQueueReceive(
                g_fsmNotifQueue[toIndex(TaskId::TeamStatus)], 
                &notif_item, 
                portMAX_DELAY) == pdPASS) {
        
            DEBUG_LEVEL_2("notif from fsm rcvd by TeamStatus");

            switch (notif_item.type) {
                case FsmNotifType::TeamChanged:
                    if (notif_item.data.team == RobotTeam::Red) {
                        pixels.clear();
                        pixels.setPixelColor(0, pixels.Color(150, 0, 0));
                        pixels.show();   // Send the updated pixel colors to the hardware.
                        
                        DEBUG_LEVEL_1("Team RED");
                    } else {
                        pixels.clear();
                        pixels.setPixelColor(0, pixels.Color(0, 0, 150));
                        pixels.show();   // Send the updated pixel colors to the hardware.

                        DEBUG_LEVEL_1("Team BLUE");
                    }
                    currTeam = notif_item.data.team;
                    break;
                case FsmNotifType::StateChanged:
                    currState = notif_item.data.state;
                    break;
                default:
                    break;
            }
        }
    }
}
