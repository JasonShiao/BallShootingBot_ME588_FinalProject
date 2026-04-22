#ifndef MOTOR_H
#define MOTOR_H

#define MOTOR_DRIVER_IN1_PIN 46
#define MOTOR_DRIVER_IN2_PIN 10
#define MOTOR_DRIVER_EN_A_PIN 11
#define MOTOR_DRIVER_EN_B_PIN 12
#define MOTOR_DRIVER_IN3_PIN 13
#define MOTOR_DRIVER_IN4_PIN 14

void initMobility();
void setMotorSpeed(int left, int right, bool swap_heading);
void forceMotorBrake();

#endif