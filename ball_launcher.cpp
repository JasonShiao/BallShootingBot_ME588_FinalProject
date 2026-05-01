#include "ball_launcher.h"
#include <ESP32Servo.h>
#include <Arduino.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "globals.h"

enum class BallShootingStage {
    PullbackSlingshot,     //
    LoadBallIntoSlingshot, // ball loader from bottom to top
    SlingshotReleased
};


void ballLauncherTask(void *parameter);
static TaskHandle_t ballLauncherTaskHandle = nullptr;
static TimerHandle_t ballLauncherTimerHandle = nullptr;
static QueueHandle_t ballLauncherCmdQueue = nullptr;
static volatile BallLauncherCtrlCmdType currCmdType = BallLauncherCtrlCmdType::Stop;

static const uint32_t BALL_LAUNCH_DURATION_MS = 3000; // 3s
static const uint32_t BUCKET_RELOAD_LED_FLASH_PERIOD_MS = 500; // 0.5s
static const uint32_t BUCKET_RELOAD_TIMER_CNT_MAX = 8;  // 8*0.5s=4s
static const uint32_t BALL_LOADING_TOP_POS = 53;
#if OPEN_LOOP_CONTROL == 1
static const uint32_t BALL_SHOOT_PULLOFF_POS = 180;
#else
static const uint32_t BALL_SHOOT_PULLOFF_POS = 120;
#endif
static const uint32_t BALL_SHOOT_RELEASE_POS = 0;
static uint32_t timeout_cnt = 0;
BallShootingStage shooting_stage = BallShootingStage::SlingshotReleased;

Servo ballLoadServo;
Servo ballShootServo;

int servoAngleToMicroseconds(int angle) {
    if (angle < 0) angle = 0;
    if (angle > SERVO_MAX_ANGLE) angle = SERVO_MAX_ANGLE;

    // map from 0-270 to 500-2500
    int micros = 500 + (angle - 0) * 2000 / SERVO_MAX_ANGLE;
    return micros;
}

void onBallLaunchTimeoutCallback(TimerHandle_t xTimer) {
    FsmEventQueueItem ev{};
    BaseType_t ok;
    switch (currCmdType) {
        case BallLauncherCtrlCmdType::Loadball:
#if OPEN_LOOP_CONTROL == 1
            digitalWrite(SHOOTING_LED_PIN, LOW);
            ev.type = FsmEventType::BallLoaded;
            ev.data.ballLoaded = true;
            ok = sendFsmEventItem(ev);
            DEBUG_LEVEL_1("Ball loaded.");
#else
            if (digitalRead(BALL_BUCKET_SENSOR_PIN) == LOW) {
                // ball loaded successfully
                digitalWrite(SHOOTING_LED_PIN, LOW);
                ev.type = FsmEventType::BallLoaded;
                ev.data.ballLoaded = true;
                ok = sendFsmEventItem(ev);

                DEBUG_LEVEL_1("Ball loaded.");
            } else {
                // failed to load ball, send bucket empty event to fsm
                ballLoadServo.writeMicroseconds(servoAngleToMicroseconds(0)); 
                digitalWrite(SHOOTING_LED_PIN, LOW);
                ev.type = FsmEventType::BucketEmptyDetected;
                ev.data.ballLoaded = false;
                ok = sendFsmEventItem(ev);

                DEBUG_LEVEL_1("Bucket empty. Not launching ball.");
            }
#endif
            break;
        case BallLauncherCtrlCmdType::Shoot:
            switch (shooting_stage) {
                case BallShootingStage::PullbackSlingshot:
                    shooting_stage = BallShootingStage::LoadBallIntoSlingshot;
                    ballLoadServo.writeMicroseconds(servoAngleToMicroseconds(BALL_LOADING_TOP_POS));
                    // TODO: Add a check for ball getting into shooter (optional)
                    // set and start timer (one-shot)
                    xTimerChangePeriod(ballLauncherTimerHandle, 
                        pdMS_TO_TICKS(800), 0);
                    xTimerStart(ballLauncherTimerHandle, 0);
                    DEBUG_LEVEL_2("pulled back slingshot");
                    break;
                case BallShootingStage::LoadBallIntoSlingshot:
                    shooting_stage = BallShootingStage::SlingshotReleased;
                    ballShootServo.writeMicroseconds(servoAngleToMicroseconds(BALL_SHOOT_RELEASE_POS));
                    xTimerChangePeriod(ballLauncherTimerHandle, 
                        pdMS_TO_TICKS(800), 0);
                    xTimerStart(ballLauncherTimerHandle, 0);
                    DEBUG_LEVEL_2("loaded ball into slingshot");
                    break;
                case BallShootingStage::SlingshotReleased:
                    shooting_stage = BallShootingStage::LoadBallIntoSlingshot;
                    ev.type = FsmEventType::BallLaunched;
                    ev.data.ballLaunched = true;
                    ok = sendFsmEventItem(ev);
                    DEBUG_LEVEL_2("released slingshot");
                    break;
                default:
                    break;
            }
            break;
        case BallLauncherCtrlCmdType::StartBucketReload:
            if (timeout_cnt >= BUCKET_RELOAD_TIMER_CNT_MAX) {
                ev.type = FsmEventType::BucketReloadTimeout;
                ev.data.bucketReloaded = true;
                ok = sendFsmEventItem(ev);
            }
            digitalWrite(SHOOTING_LED_PIN, timeout_cnt % 2);
            break;
        default:
            break;
    }
    timeout_cnt += 1;
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

    ballLoadServo.writeMicroseconds(servoAngleToMicroseconds(BALL_LOADING_TOP_POS)); // 0 for top (close to shooter)

    ballLauncherCmdQueue = xQueueCreate(10, sizeof(BallLauncherCtrlCmd));
    if (ballLauncherCmdQueue == nullptr) {
        DEBUG_LEVEL_1("Ball Launcher command queue created failed");
    }

    // create timer for ball launched (wait for ball in the air)
    ballLauncherTimerHandle = xTimerCreate( 
        "BallLauncherTimerHandle",
        pdMS_TO_TICKS(BALL_LAUNCH_DURATION_MS),
        pdFALSE,     // single shot timer
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
                    digitalWrite(SHOOTING_LED_PIN, HIGH);
                    // 1. load ball at low position (start loading procedure timer?)
                    DEBUG_LEVEL_1("Loading ball...");
                    ballShootServo.writeMicroseconds(servoAngleToMicroseconds(BALL_SHOOT_PULLOFF_POS));
                    ballLoadServo.writeMicroseconds(servoAngleToMicroseconds(270)); // bottom to load from hopper
                    vTaskDelay(pdMS_TO_TICKS(500));
                    ballLoadServo.writeMicroseconds(servoAngleToMicroseconds(260)); // bottom to load from hopper
                    vTaskDelay(pdMS_TO_TICKS(500));
                    ballLoadServo.writeMicroseconds(servoAngleToMicroseconds(270)); // bottom to load from hopper
                    vTaskDelay(pdMS_TO_TICKS(500));
                    ballLoadServo.writeMicroseconds(servoAngleToMicroseconds(260)); // bottom to load from hopper
                    vTaskDelay(pdMS_TO_TICKS(500));
                    ballLoadServo.writeMicroseconds(servoAngleToMicroseconds(270)); // bottom to load from hopper
                    // Timer for triggering next stage
                    vTimerSetReloadMode(ballLauncherTimerHandle, pdFALSE);
                    xTimerChangePeriod(ballLauncherTimerHandle,
                        pdMS_TO_TICKS(500), 0);
                    xTimerStart(ballLauncherTimerHandle, 0);
                    break;
                case BallLauncherCtrlCmdType::Shoot:
                    digitalWrite(SHOOTING_LED_PIN, HIGH);
                    // 1. detect ball first (optional)
                    // stop loading procedure timer in case it's still running
                    // 2. start shooting procedure timer
                    ballShootServo.writeMicroseconds(servoAngleToMicroseconds(BALL_SHOOT_PULLOFF_POS));
                    shooting_stage = BallShootingStage::PullbackSlingshot;
                    vTimerSetReloadMode(ballLauncherTimerHandle, pdFALSE);
                    xTimerChangePeriod(ballLauncherTimerHandle, 
                        pdMS_TO_TICKS(800), 0);
                    if (xTimerStart(ballLauncherTimerHandle, 0) != pdPASS) {
                        // error ...
                    }
                    break;
                case BallLauncherCtrlCmdType::Stop:
                    // stop shooting or loading procedure timer
                    digitalWrite(SHOOTING_LED_PIN, LOW);
                    // stop timer in case
                    xTimerStop(ballLauncherTimerHandle, 0);
                    timeout_cnt = 0;
                    ballLoadServo.writeMicroseconds(servoAngleToMicroseconds(BALL_LOADING_TOP_POS));
                    ballShootServo.writeMicroseconds(servoAngleToMicroseconds(0));

                    DEBUG_LEVEL_1("Ball load/launch stopped.");
                    break;
                case BallLauncherCtrlCmdType::StartBucketReload:
                    digitalWrite(SHOOTING_LED_PIN, HIGH);
                    // start a timer (autoreload periodic)
                    timeout_cnt = 0;
                    vTimerSetReloadMode(ballLauncherTimerHandle, pdTRUE);
                    xTimerChangePeriod(ballLauncherTimerHandle, 
                        pdMS_TO_TICKS(BUCKET_RELOAD_LED_FLASH_PERIOD_MS), 0);
                    if (xTimerStart(ballLauncherTimerHandle, 0) != pdPASS) {
                        // error ...
                    }
                    break;
                default:
                    break;
            }

            currCmdType = cmd.type;
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
