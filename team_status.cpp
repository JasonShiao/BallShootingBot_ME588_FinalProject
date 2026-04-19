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
void TeamStatusTask(void *parameter);
TaskHandle_t TeamStatusTaskHandle = NULL;

Adafruit_NeoPixel pixels(NUMPIXELS, NeoPixel_PIN, NEO_GRB + NEO_KHZ800);

static volatile uint32_t last_btn_isr_us = 0; // for debounce
// keep a copy of state inside (might be older then latest fsm state for a short time)
static volatile RobotState curr_state = RobotState::IDLE; 
static RobotTeam curr_team = RobotTeam::RED;


void ARDUINO_ISR_ATTR onTeamSwitchBtnInterrupt() {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    uint32_t now = micros();
    if ((now - last_btn_isr_us) > 50000) {  // 50 ms debounce
        last_btn_isr_us = now;
        
        if (curr_state == RobotState::IDLE) {
        //if ((curr_state == RobotState::IDLE) && (digitalRead(TEAM_SELECT_INPUT_PIN) == HIGH)) {
            // Send team change to queue
            FsmEventQueueItem ev{};
            ev.type = FsmEventType::TeamChangeReq;
            ev.data.teamChanged = true;
            BaseType_t ok = xQueueSendFromISR(g_FsmEventQueue, &ev, &xHigherPriorityTaskWoken);
        }
    }
    
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}


void InitTeamStatusTask() {
    pixels.begin();
    pixels.clear();
    pixels.show();

    // init team select input (SW) pin
    pinMode(TEAM_SELECT_INPUT_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(TEAM_SELECT_INPUT_PIN), onTeamSwitchBtnInterrupt, RISING);

    pixels.clear();
    if (curr_team == RobotTeam::RED) {
        pixels.setPixelColor(0, pixels.Color(150, 0, 0));
    } else if (curr_team == RobotTeam::BLUE) {
        pixels.setPixelColor(0, pixels.Color(0, 0, 150));
    } else {
        pixels.setPixelColor(0, pixels.Color(150, 150, 150));
    }
    pixels.show(); 

    xTaskCreatePinnedToCore(
        TeamStatusTask,         // Task function
        "TeamStatusTask",       // Task name
        2048,             // Stack size (bytes)
        NULL,              // Parameters
        1,                 // Priority
        &TeamStatusTaskHandle,  // Task handle
        1                  // Core 1
    );

}

void TeamStatusTask(void *parameter) {
    FsmNotifQueueItem notif_item;

    for (;;) { // Infinite loop
        if (xQueueReceive(
                g_FsmNotifQueue[ToIndex(TaskId::TeamStatus)], 
                &notif_item, 
                portMAX_DELAY) == pdPASS) {
        
            Serial.println("notif from fsm rcvd by TeamStatus");

            switch (notif_item.type) {
                case FsmNotifType::TeamChanged:
                    if (notif_item.data.team == RobotTeam::RED) {
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
                    curr_team = notif_item.data.team;
                    break;
                case FsmNotifType::StateChanged:
                    curr_state = notif_item.data.state;
                    break;
                default:
                    break;
            }
        }
    }
}
