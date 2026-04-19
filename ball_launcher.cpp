#include "ball_launcher.h"
#include <ESP32Servo.h>
#include <Arduino.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "globals.h"

void ballLauncherTask(void *parameter);
TaskHandle_t ballLauncherTaskHandle = NULL;

hw_timer_t* ballLaunchTimer = nullptr;
static const uint32_t BALL_LAUNCH_DURATION_MS = 3000; // 3s
Servo ballLoadServo;
Servo ballShootServo;

volatile bool isLaunchingBall = false;

int servoAngleToMicroseconds(int angle) {
    if (angle < 0) angle = 0;
    if (angle > 270) angle = 270;

    // map from 0-270 to 500-2500
    int micros = 500 + (angle - 0) * 2000 / 270;
    return micros;
}

// n seconds of wait after shooting, notify FSM to switch to IR beacon detection state
void ARDUINO_ISR_ATTR onBallLaunchTimeoutInterrupt() {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (isLaunchingBall) {
        FsmEventQueueItem ev{};
        ev.type = FsmEventType::BallLaunched;
        ev.data.ballLaunched = true;
        BaseType_t ok = xQueueSendFromISR(g_fsmEventQueue, &ev, &xHigherPriorityTaskWoken);
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void initBallLauncherTask() {
    // setup pins and ISRs
    pinMode(SHOOTING_LED_PIN, OUTPUT);
    digitalWrite(SHOOTING_LED_PIN, LOW);

    pinMode(BALL_BUCKET_SENSOR_PIN, INPUT_PULLUP);
    //attachInterrupt(digitalPinToInterrupt(BALL_BUCKET_SENSOR_PIN), onBallBucketSensorInterrupt, FALLING);

    // TODO: servo for ball load, servo for ball shoot
    ballLoadServo.setPeriodHertz(50);    // standard 50 hz servo
    ballShootServo.setPeriodHertz(50);    // standard 50 hz servo
    ballLoadServo.attach(BALL_LOADING_SERVO_PIN, 500, 2500);
    ballShootServo.attach(BALL_SHOOTING_SERVO_PIN, 500, 2500);

    // create timer for ball launched
    ballLaunchTimer = timerBegin(1000000); // API v3.0, (freq: 1Mhz -> 1us per counter tick)
    if (ballLaunchTimer == nullptr) {
        DEBUG_LEVEL_1("Failed to create timer.");
        while (true) {
            delay(1000);
        }
    }
    timerAttachInterrupt(ballLaunchTimer, &onBallLaunchTimeoutInterrupt);
    timerAlarm(ballLaunchTimer, BALL_LAUNCH_DURATION_MS * 1000ULL, false, 0); // one-shot timeout alarm
    timerStop(ballLaunchTimer);

    // create and register task
    xTaskCreatePinnedToCore(
        ballLauncherTask,            // Task function
        "BallLauncherTask",    // Task name
        4096,             // Stack size (bytes)
        NULL,              // Parameters
        2,                 // Priority
        &ballLauncherTaskHandle,  // Task handle
        1                  // Core 1
    );

}

void ballLauncherTask(void *parameter) {
    FsmNotifQueueItem notif_item;

    for(;;) {
        if (xQueueReceive(
                g_fsmNotifQueue[toIndex(TaskId::BallLauncher)], 
                &notif_item, 
                portMAX_DELAY) == pdPASS) {
            
            DEBUG_LEVEL_2("notif from fsm rcvd by BallLauncher");

            switch (notif_item.type) {
                case FsmNotifType::StateChanged:
                    if (!isLaunchingBall && (notif_item.data.state == RobotState::LaunchingBall)) {
                        isLaunchingBall = true;
                        // TODO: 

                        // launching procedure: 
                        // 0. turn on white LED
                        digitalWrite(SHOOTING_LED_PIN, HIGH);
                        // 1. load ball
                        //ballLoadServo.writeMicroseconds(servoAngleToMicroseconds(...));
                        // 2. detect if ball not loaded, notify fsm
                        if (digitalRead(BALL_BUCKET_SENSOR_PIN) == HIGH) { // breakbeam not blocked -> NPN open -> no ball detected
                            // reset isLaunchingBall to false
                            // turn off white LED
                            // send event 
                            isLaunchingBall = false;
                            digitalWrite(SHOOTING_LED_PIN, LOW);
                            FsmEventQueueItem ev{};
                            ev.type = FsmEventType::BucketEmptyDetected;
                            ev.data.ballLaunched = false;
                            BaseType_t ok = xQueueSend(g_fsmEventQueue, &ev, 0);

                            DEBUG_LEVEL_1("Bucket empty. Not launching ball.");
                        } else {
                            // 3. shoot
                            //ballShootServo.writeMicroseconds(servoAngleToMicroseconds(...));
                            //ballShootServo.writeMicroseconds(servoAngleToMicroseconds(...));

                            // reset and start timer
                            timerRestart(ballLaunchTimer);
                            timerStart(ballLaunchTimer);

                            DEBUG_LEVEL_1("Ball should've been launched in the air. Wait for 3 sec...");
                        }
                    } else if (isLaunchingBall && (notif_item.data.state != RobotState::LaunchingBall)) {
                        isLaunchingBall = false;
                        digitalWrite(SHOOTING_LED_PIN, LOW);

                        // stop timer in case
                        timerStop(ballLaunchTimer);

                        DEBUG_LEVEL_1("Ball launch stopped.");
                    }
                    break;
                default:
                    break;
            }

        }
    }


}