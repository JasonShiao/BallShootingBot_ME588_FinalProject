#include <Arduino.h>
#include <QTRSensors.h>


QTRSensors qtr;
const uint8_t sensor_count = 4;
uint16_t sensor_values[sensor_count];

// Shared result
volatile uint16_t line_position = 0;

TaskHandle_t LineReadTaskHandle = NULL;

// FreeRTOS task: periodically read line position
void LineReadTask(void *parameter) {
  TickType_t lastWakeTime = xTaskGetTickCount();
  const TickType_t period = pdMS_TO_TICKS(200); // 200ms for demo and testing only // TODO: set to 10-20ms for fast response!

  for (;;) {
    line_position = qtr.readLineBlack(sensor_values);

    Serial.print(line_position);
    Serial.print(",");

    for (uint8_t i = 0; i < sensor_count; i++) {
      Serial.print(sensor_values[i]);
      if (i == sensor_count - 1) {
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
    qtr.setSensorPins((const uint8_t[]){1, 2, 42, 41}, sensor_count);

    Serial.println("calibrating line follower...");
    // Calibration
    for (uint16_t i = 0; i < 30; i++) {
        qtr.calibrate();
    }

    // Print calibration minimum values
    for (uint8_t i = 0; i < sensor_count; i++) {
        Serial.print(qtr.calibrationOn.minimum[i]);
        Serial.print(' ');
    }
    Serial.println();

    // Print calibration maximum values
    for (uint8_t i = 0; i < sensor_count; i++) {
        Serial.print(qtr.calibrationOn.maximum[i]);
        Serial.print(' ');
    }
    Serial.println();
    Serial.println();

    // Create FreeRTOS task
    xTaskCreatePinnedToCore(
        LineReadTask,        // Task function
        "LineReadTask",      // Task name
        4096,                // Stack size
        NULL,                // Parameter
        2,                   // Priority
        &LineReadTaskHandle, // Task handle
        0                    // Core (0 or 1)
    );
}
