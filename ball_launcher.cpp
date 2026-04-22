#include "ball_launcher.h"
#include <ESP32Servo.h>
#include <Arduino.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "globals.h"

void ballLauncherTask(void *parameter);
TaskHandle_t ballLauncherTaskHandle = NULL;
TimerHandle_t ballLauncherTimerHandle = NULL;

static const uint32_t BALL_LAUNCH_DURATION_MS = 3000; // 3s
Servo ballLoadServo;
Servo ballShootServo;

volatile bool isLaunchingBall = false;

int servoAngleToMicroseconds(int angle) {
    if (angle < 0) angle = 0;
    if (angle > SERVO_MAX_ANGLE) angle = SERVO_MAX_ANGLE;

    // map from 0-270 to 500-2500
    int micros = 500 + (angle - 0) * 2000 / SERVO_MAX_ANGLE;
    return micros;
}

void onBallLaunchTimeoutCallback(TimerHandle_t xTimer) {
    if (isLaunchingBall) {
        FsmEventQueueItem ev{};
        ev.type = FsmEventType::BallLaunched;
        ev.data.ballLaunched = true;
        BaseType_t ok = xQueueSend(g_fsmEventQueue, &ev, 0);
        if (ok != pdPASS) {
            // queue full, handle error if needed
        }
    }
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

    // create timer for ball launched (wait for ball in the air)
    ballLauncherTimerHandle = xTimerCreate( 
        "BallLauncherTimerHandle",
        pdMS_TO_TICKS(BALL_LAUNCH_DURATION_MS),
        pdFALSE,
        (void *) 0,
        onBallLaunchTimeoutCallback);

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
                    if (!isLaunchingBall && (notif_item.data.state == RobotState::BallLaunching)) {
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
                            ballShootServo.writeMicroseconds(servoAngleToMicroseconds(0));
                            vTaskDelay(pdMS_TO_TICKS(800));
                            ballShootServo.writeMicroseconds(servoAngleToMicroseconds(SERVO_MAX_ANGLE));
                            vTaskDelay(pdMS_TO_TICKS(800));
                            ballShootServo.writeMicroseconds(servoAngleToMicroseconds(0));
                            vTaskDelay(pdMS_TO_TICKS(500));

                            // start timer
                            if (xTimerStart(ballLauncherTimerHandle, 0) != pdPASS) {
                                // error handling
                                DEBUG_LEVEL_1("[Error] Ball Launch timer start failed");
                            }
                            DEBUG_LEVEL_1("Ball should've been launched in the air. Wait for 3 sec...");
                        }
                    } else if (isLaunchingBall && (notif_item.data.state != RobotState::BallLaunching)) {
                        isLaunchingBall = false;
                        digitalWrite(SHOOTING_LED_PIN, LOW);

                        // stop timer in case
                        xTimerStop(ballLauncherTimerHandle, 0);

                        DEBUG_LEVEL_1("Ball launch stopped.");
                    }
                    break;
                default:
                    break;
            }

        }
    }


}