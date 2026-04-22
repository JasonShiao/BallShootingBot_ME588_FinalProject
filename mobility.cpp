#include "mobility.h"
#include <Arduino.h>

#define MOTOR_PWM_FREQ 10000    // 10kHz
#define MOTOR_PWM_RESOLUTION 8  // 0–255
#define MOTOR_PWM_MAX ((1 << MOTOR_PWM_RESOLUTION) - 1)

static void setMotorSpeedPrivate(int speedA, int speedB);
static void setSingleMotor(int speed, int in1Pin, int in2Pin, int enPin);


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
    setMotorSpeed(0, 0, false);
}

void setMotorSpeed(int left, int right, bool swap_heading) {
    if (!swap_heading) {
        // A as left, B as right
        setMotorSpeedPrivate(left, right);
    } else {
        // A as right, B as left -> also *(-1) to reverse the forward direction
        setMotorSpeedPrivate(-right, -left);
    }
}

static void setSingleMotor(int speed, int in1Pin, int in2Pin, int enPin) {
    int absSpeed = constrain(abs(speed), 0, 255);
    if (speed < 0) {
        digitalWrite(in1Pin, LOW);
        digitalWrite(in2Pin, HIGH);
    } else if (speed == 0) {
        digitalWrite(in1Pin, LOW);
        digitalWrite(in2Pin, LOW);
    } else {
        digitalWrite(in1Pin, HIGH);
        digitalWrite(in2Pin, LOW);
    }
    ledcWrite(enPin, absSpeed);
}

/**
 *  when speed == 0, coast instead of force brake
 */
static void setMotorSpeedPrivate(int speedA, int speedB) {
    setSingleMotor(speedA, MOTOR_DRIVER_IN1_PIN, MOTOR_DRIVER_IN2_PIN, MOTOR_DRIVER_EN_A_PIN);
    setSingleMotor(speedB, MOTOR_DRIVER_IN3_PIN, MOTOR_DRIVER_IN4_PIN, MOTOR_DRIVER_EN_B_PIN);
}

void forceMotorBrake() {
    digitalWrite(MOTOR_DRIVER_IN1_PIN, LOW);
    digitalWrite(MOTOR_DRIVER_IN2_PIN, LOW);
    digitalWrite(MOTOR_DRIVER_IN3_PIN, LOW);
    digitalWrite(MOTOR_DRIVER_IN4_PIN, LOW);

    ledcWrite(MOTOR_DRIVER_EN_A_PIN, MOTOR_PWM_MAX);
    ledcWrite(MOTOR_DRIVER_EN_B_PIN, MOTOR_PWM_MAX);
}

