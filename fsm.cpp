#include "fsm.h"
#include <Arduino.h>

static RobotFSM fsm;

void FsmTask(void *parameter);
TaskHandle_t FsmTaskHandle = nullptr;


RobotState RobotFSM::get_state() {
  return _state;
}

void RobotFSM::set_state(RobotState new_state) {
    _state = new_state;
}

RobotTeam RobotFSM::get_team() {
  return _team;
}

void RobotFSM::toggle_team() {
    _team = (_team == RobotTeam::BLUE) ? RobotTeam::RED : RobotTeam::BLUE;
}

void InitFsmTask() {
    // create the Task
    xTaskCreatePinnedToCore(
        FsmTask,          // Task function
        "FsmTask",        // Task name
        8192,             // Stack size (bytes)
        NULL,              // Parameters
        2,                 // Priority
        &FsmTaskHandle,  // Task handle
        1                  // Core 1
    );
}

void broadcastNotif(FsmNotifQueueItem notif) {
    for (int i = 0; i < NUM_TASK; i++) {
         BaseType_t ok = xQueueSend(g_FsmNotifQueue[i], &notif, 0); // expect return: pdPASS
    }
}

void FsmTask(void *parameter) {
    FsmEventQueueItem ev{};
    for(;;) {
        // block for event queue
        if (xQueueReceive(
                g_FsmEventQueue, 
                &ev, 
                portMAX_DELAY) == pdPASS) {
            
            Serial.println("FSM task rcvd a new event");
            switch (ev.type) {
                case FsmEventType::GameStartReq:
                    Serial.println("Game start req");
                    // TODO:
                    if (fsm.get_state() == RobotState::IDLE) {
                        fsm.set_state(RobotState::STARTED);
                        FsmNotifQueueItem notif{};
                        notif.type = FsmNotifType::StateChanged;
                        notif.data.state = fsm.get_state();
                        notif.data.team = fsm.get_team();
                        broadcastNotif(notif);
                    }
                    break;
                case FsmEventType::GameTimeoutReq:
                    Serial.println("Game timeout req");
                    // TODO:
                    break;
                case FsmEventType::TeamChangeReq:
                    Serial.println("Team change req");
                    // TODO:
                    if (fsm.get_state() == RobotState::IDLE) {
                        fsm.toggle_team();
                        FsmNotifQueueItem notif{};
                        notif.type = FsmNotifType::TeamChanged;
                        notif.data.state = fsm.get_state();
                        notif.data.team = fsm.get_team();
                        broadcastNotif(notif);
                    }
                    break;
                default:
                    // TODO:
                    break;
            }
        
        }

    }
}