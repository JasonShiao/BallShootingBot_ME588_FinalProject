#include "mobility.h"
#include <Arduino.h>

#define MOTOR_PWM_FREQ 10000    // 10kHz
#define MOTOR_PWM_RESOLUTION 8  // 0–255
#define MOTOR_PWM_MAX ((1 << MOTOR_PWM_RESOLUTION) - 1)

static void brake();

void initMobility() {
    // Setup DIR pins
    pinMode(MOTOR_DRIVER_IN1_PIN, OUTPUT);
    pinMode(MOTOR_DRIVER_IN2_PIN, OUTPUT);
    pinMode(MOTOR_DRIVER_IN3_PIN, OUTPUT);
    pinMode(MOTOR_DRIVER_IN4_PIN, OUTPUT);

    // Setup PWM output for motor EN
    pinMode(MOTOR_DRIVER_EN_A_PIN, OUTPUT);
    pinMode(MOTOR_DRIVER_EN_B_PIN, OUTPUT);
    ledcAttach(MOTOR_DRIVER_EN_A_PIN, MOTOR_PWM_FREQ, MOTOR_PWM_RESOLUTION);
    ledcAttach(MOTOR_DRIVER_EN_B_PIN, MOTOR_PWM_FREQ, MOTOR_PWM_RESOLUTION);

    // Init with coast
    setMotorSpeed(0, 0);
}

/**
 *  when speed == 0, coast instead of force brake
 * 
 */
void setMotorSpeed(int speedA, int speedB) {
    int absSpeedA = constrain(abs(speedA), 0, 255);
    int absSpeedB = constrain(abs(speedB), 0, 255);
    
    if (speedA < 0) {
        digitalWrite(MOTOR_DRIVER_IN1_PIN, LOW);
        digitalWrite(MOTOR_DRIVER_IN2_PIN, HIGH);
    } else if (speedA == 0) {
        digitalWrite(MOTOR_DRIVER_IN1_PIN, LOW);
        digitalWrite(MOTOR_DRIVER_IN2_PIN, LOW);
    } else {
        digitalWrite(MOTOR_DRIVER_IN1_PIN, HIGH);
        digitalWrite(MOTOR_DRIVER_IN2_PIN, LOW);
    }
    if (speedB < 0) {
        digitalWrite(MOTOR_DRIVER_IN3_PIN, LOW);
        digitalWrite(MOTOR_DRIVER_IN4_PIN, HIGH);
    } else if (speedB == 0) {
        digitalWrite(MOTOR_DRIVER_IN3_PIN, LOW);
        digitalWrite(MOTOR_DRIVER_IN4_PIN, LOW);
    } else {
        digitalWrite(MOTOR_DRIVER_IN3_PIN, HIGH);
        digitalWrite(MOTOR_DRIVER_IN4_PIN, LOW);
    }
    ledcWrite(MOTOR_DRIVER_EN_A_PIN, absSpeedA);
    ledcWrite(MOTOR_DRIVER_EN_B_PIN, absSpeedB);
}

void forceMotorBrake() {
    digitalWrite(MOTOR_DRIVER_IN1_PIN, LOW);
    digitalWrite(MOTOR_DRIVER_IN2_PIN, LOW);
    digitalWrite(MOTOR_DRIVER_IN3_PIN, LOW);
    digitalWrite(MOTOR_DRIVER_IN4_PIN, LOW);

    ledcWrite(MOTOR_DRIVER_EN_A_PIN, MOTOR_PWM_MAX);
    ledcWrite(MOTOR_DRIVER_EN_B_PIN, MOTOR_PWM_MAX);
}

