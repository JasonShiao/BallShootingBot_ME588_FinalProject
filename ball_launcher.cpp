#include "ball_launcher.h"
#include <ESP32Servo.h>
#include <Arduino.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "globals.h"

void ballLauncherTask(void *parameter);
static TaskHandle_t ballLauncherTaskHandle = nullptr;
static TimerHandle_t ballLauncherTimerHandle = nullptr;
static QueueHandle_t ballLauncherCmdQueue = nullptr;

static const uint32_t BALL_LAUNCH_DURATION_MS = 3000; // 3s
Servo ballLoadServo;
Servo ballShootServo;

static volatile bool isLaunchingBall = false;

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
        BaseType_t ok = sendFsmEventItem(ev);
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

    // servo for ball load, servo for ball shoot
    ballLoadServo.setPeriodHertz(50);    // standard 50 hz servo
    ballShootServo.setPeriodHertz(50);    // standard 50 hz servo
    ballLoadServo.attach(BALL_LOADING_SERVO_PIN, 500, 2500);
    ballShootServo.attach(BALL_SHOOTING_SERVO_PIN, 500, 2500);

    ballLauncherCmdQueue = xQueueCreate(10, sizeof(BallLauncherCtrlCmd));
    if (ballLauncherCmdQueue == nullptr) {
        DEBUG_LEVEL_1("Ball Launcher command queue created failed");
    }

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

/**
 * Ball launcher procedure:
 *      Load cmd: detect whether ball is already loaded first,
 *                if not loaded, start loading (turn on white LED, move servo to load position)
 *                if loaded, do nothing send event to fsm to notify ball loaded
 *                if failed, send bucket empty event to fsm
 *     Shoot cmd: if ball is loaded, start shooting procedure (move servo to shoot position
 *     Stop cmd: stop any ongoing ball launching procedure
 */
void ballLauncherTask(void *parameter) {
    BallLauncherCtrlCmd cmd;

    for(;;) {
        if (xQueueReceive(
                ballLauncherCmdQueue, 
                &cmd, 
                portMAX_DELAY) == pdPASS) {
            
            DEBUG_LEVEL_2("Cmd rcvd by BallLauncher");

            switch (cmd.type) {
                case BallLauncherCtrlCmdType::Loadball:
                    // stop shooting procedure timer in case it's still running
                    isLaunchingBall = false;
                    digitalWrite(SHOOTING_LED_PIN, HIGH);
                    // 1. Detect ball first
                    if (digitalRead(BALL_BUCKET_SENSOR_PIN) == LOW) {
                        // breakbeam blocked -> NPN closed -> ball detected
                        // 2. if loaded, send event, break
                        FsmEventQueueItem ev{};
                        ev.type = FsmEventType::BallLoaded;
                        ev.data.ballLoaded = true;
                        BaseType_t ok = sendFsmEventItem(ev);

                        DEBUG_LEVEL_1("Ball loaded.");
                        break;
                    }
                    // 3. if not loaded, (start loading procedure timer?)
                    DEBUG_LEVEL_1("Loading ball...");
                    ballLoadServo.writeMicroseconds(servoAngleToMicroseconds(SERVO_MAX_ANGLE));
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    if (digitalRead(BALL_BUCKET_SENSOR_PIN) == LOW) {
                        // ball loaded successfully
                        ballLoadServo.writeMicroseconds(servoAngleToMicroseconds(0));
                        digitalWrite(SHOOTING_LED_PIN, LOW);
                        FsmEventQueueItem ev{};
                        ev.type = FsmEventType::BallLoaded;
                        ev.data.ballLoaded = true;
                        BaseType_t ok = sendFsmEventItem(ev);

                        DEBUG_LEVEL_1("Ball loaded.");
                    } else {
                        // failed to load ball, send bucket empty event to fsm
                        ballLoadServo.writeMicroseconds(servoAngleToMicroseconds(0)); 
                        digitalWrite(SHOOTING_LED_PIN, LOW);
                        FsmEventQueueItem ev{};
                        ev.type = FsmEventType::BucketEmptyDetected;
                        ev.data.ballLoaded = false;
                        BaseType_t ok = sendFsmEventItem(ev);

                        DEBUG_LEVEL_1("Bucket empty. Not launching ball.");
                    }
                    break;
                case BallLauncherCtrlCmdType::Shoot:
                    digitalWrite(SHOOTING_LED_PIN, HIGH);
                    // 1. detect ball first (optional)
                    // stop loading procedure timer in case it's still running
                    // 2. start shooting procedure timer
                    ballShootServo.writeMicroseconds(servoAngleToMicroseconds(0));
                    vTaskDelay(pdMS_TO_TICKS(800));
                    ballShootServo.writeMicroseconds(servoAngleToMicroseconds(SERVO_MAX_ANGLE));
                    vTaskDelay(pdMS_TO_TICKS(800));
                    ballShootServo.writeMicroseconds(servoAngleToMicroseconds(0));
                    vTaskDelay(pdMS_TO_TICKS(500));

                    isLaunchingBall = true;
                    // start timer
                    if (xTimerStart(ballLauncherTimerHandle, 0) != pdPASS) {
                        // error handling
                        DEBUG_LEVEL_1("[Error] Ball Launch timer start failed");
                    }
                    DEBUG_LEVEL_1("Ball should've been launched in the air. Wait for 3 sec...");
                    break;
                case BallLauncherCtrlCmdType::Stop:
                    // stop shooting or loading procedure timer
                    digitalWrite(SHOOTING_LED_PIN, LOW);
                    isLaunchingBall = false;
                    // stop timer in case
                    xTimerStop(ballLauncherTimerHandle, 0);
                    ballLoadServo.writeMicroseconds(servoAngleToMicroseconds(0));
                    ballShootServo.writeMicroseconds(servoAngleToMicroseconds(0));

                    DEBUG_LEVEL_1("Ball load/launch stopped.");
                    break;
                default:
                    break;
            }
        }
    }

}

bool sendBallLauncherCtrlCmd(const BallLauncherCtrlCmd& cmd) {
    if (ballLauncherCmdQueue == nullptr) {
        DEBUG_LEVEL_1("Ball Launcher command queue not initialized");
        return false;
    }
    return xQueueSend(ballLauncherCmdQueue, &cmd, 0) == pdPASS;
}
