#include "team_status.h"

//#define IR_ADC_PIN 7

//volatile int adc_val = 0;


//TaskHandle_t BlinkTaskHandle = NULL;
//TaskHandle_t IRBeaconTaskHandle = NULL;

// void BlinkTask(void *parameter) {
//   for (;;) { // Infinite loop
//     pixels.clear();
//     pixels.setPixelColor(0, pixels.Color(150, 0, 0));
//     pixels.show();   // Send the updated pixel colors to the hardware.
//     //Serial.println("BlinkTask: LED ON");
//     vTaskDelay(500 / portTICK_PERIOD_MS); // 1000ms
//     pixels.clear();
//     pixels.setPixelColor(0, pixels.Color(0, 150, 0));
//     pixels.show();   // Send the updated pixel colors to the hardware.
//     //Serial.println("BlinkTask: LED OFF");
//     vTaskDelay(500 / portTICK_PERIOD_MS);
//     //Serial.print("BlinkTask running on core ");
//     //Serial.println(xPortGetCoreID());
//   }
// }


// void ARDUINO_ISR_ATTR onADCReadTimer() {
//   // Increment the counter and set the time of ISR
//   portENTER_CRITICAL_ISR(&timerMux);
//   adc_val = analogRead(IR_ADC_PIN);
//   portEXIT_CRITICAL_ISR(&timerMux);
//   // Give a semaphore that we can check in the loop
//   xSemaphoreGiveFromISR(ir_read_sem, NULL);
//   // It is safe to use digitalRead/Write here if you want to toggle an output
// }

// void IRBeaconTask(void *parameter) {
//   for (;;) { // Infinite loop
//     Serial.println(adc_val);
//   }
// }

void setup() {
  // put your setup code here, to run once:
  Serial.begin(230400);

  // xTaskCreatePinnedToCore(
  //   BlinkTask,         // Task function
  //   "BlinkTask",       // Task name
  //   10000,             // Stack size (bytes)
  //   NULL,              // Parameters
  //   1,                 // Priority
  //   &BlinkTaskHandle,  // Task handle
  //   1                  // Core 1
  // );

  InitTeamStatusTask();


  pinMode(39, INPUT_PULLUP);

  // xTaskCreatePinnedToCore(
  //   IRBeaconTask,         // Task function
  //   "IRBeaconTask",       // Task name
  //   10000,             // Stack size (bytes)
  //   NULL,              // Parameters
  //   1,                 // Priority
  //   &IRBeaconTaskHandle,  // Task handle
  //   1                  // Core 1
  // );

}

void loop() {
  // put your main code here, to run repeatedly:


}