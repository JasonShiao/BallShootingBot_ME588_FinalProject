#include <Arduino.h>
#include <QTRSensors.h>
#include "line_follower.h"
#include "FreeRTOS.h"


QTRSensors qtr;
uint16_t sensor_values[LINE_FOLLOWER_LINE_COUNT];

// Shared result
volatile uint16_t line_position = 0;

TaskHandle_t lineReadTaskHandle = NULL;

// FreeRTOS task: periodically read line position
void lineReadTask(void *parameter) {
  TickType_t lastWakeTime = xTaskGetTickCount();
  const TickType_t period = pdMS_TO_TICKS(200); // 200ms for demo and testing only // TODO: set to 10-20ms for fast response!

  for (;;) {
    line_position = qtr.readLineBlack(sensor_values);

    Serial.print(line_position);
    Serial.print(",");

    for (uint8_t i = 0; i < LINE_FOLLOWER_LINE_COUNT; i++) {
      Serial.print(sensor_values[i]);
      if (i == LINE_FOLLOWER_LINE_COUNT - 1) {
        Serial.println();
      } else {
        Serial.print(",");
      }
    }

    vTaskDelayUntil(&lastWakeTime, period);
  }
}

void InitLineFollowerTask() {

    qtr.setTypeRC();
    qtr.setSensorPins((const uint8_t[]){
      LINE_FOLLOWER_PIN_1, LINE_FOLLOWER_PIN_2, 
      LINE_FOLLOWER_PIN_3, LINE_FOLLOWER_PIN_4}, LINE_FOLLOWER_LINE_COUNT);

    Serial.println("calibrating line follower...");
    // Calibration
    for (uint16_t i = 0; i < 30; i++) {
        qtr.calibrate();
    }

    // Print calibration minimum values
    for (uint8_t i = 0; i < LINE_FOLLOWER_LINE_COUNT; i++) {
        Serial.print(qtr.calibrationOn.minimum[i]);
        Serial.print(' ');
    }
    Serial.println();

    // Print calibration maximum values
    for (uint8_t i = 0; i < LINE_FOLLOWER_LINE_COUNT; i++) {
        Serial.print(qtr.calibrationOn.maximum[i]);
        Serial.print(' ');
    }
    Serial.println();
    Serial.println();

    // Create FreeRTOS task
    xTaskCreatePinnedToCore(
        lineReadTask,        // Task function
        "LineReadTask",      // Task name
        4096,                // Stack size
        NULL,                // Parameter
        2,                   // Priority
        &lineReadTaskHandle, // Task handle
        0                    // Core (0 or 1)
    );
}
