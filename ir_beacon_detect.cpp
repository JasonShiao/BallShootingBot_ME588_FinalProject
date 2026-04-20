#include "ir_beacon_detect.h"
#include <Arduino.h>
#include "driver/pcnt.h"
#include "FreeRTOS.h"
#include <stdint.h>
#include "globals.h"

TaskHandle_t irBeaconDetectTaskHandle = NULL;
TimerHandle_t periodicReportTimerHandle = NULL; // report detected frequency periodically
void irBeaconDetectTask(void *parameter);
void setupPCNT();
int detectedFreq = 0;

hw_timer_t* irBeaconDetectTimer = nullptr; // used for a fixed period of timeout

void onPeriodicReportTimeoutCallback(TimerHandle_t xTimer) {
    DEBUG_LEVEL_1("freq: %d", detectedFreq);
}

void ARDUINO_ISR_ATTR onIrBeaconDetectTimeoutInterrupt() {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    int16_t pulseCount = 0;
    pcnt_get_counter_value(IR_BEACON_PCNT_UNIT, &pulseCount);
    detectedFreq = pulseCount * 1000 / IR_BEACON_DETECT_PERIOD_MS;
    pcnt_counter_pause(IR_BEACON_PCNT_UNIT);
    pcnt_counter_clear(IR_BEACON_PCNT_UNIT);
    pcnt_counter_resume(IR_BEACON_PCNT_UNIT);

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void initIrBeaconDetect() {
    pinMode(IR_BEACON_PCNT_INPUT_PIN, INPUT);

    setupPCNT();

    // Create hw timer for generating precise fixed period of timeout
    irBeaconDetectTimer = timerBegin(1000000); // API v3.0, (freq: 1Mhz -> 1us per counter tick)
    if (irBeaconDetectTimer == nullptr) {
        DEBUG_LEVEL_1("Failed to create timer.");
    }
    timerAttachInterrupt(irBeaconDetectTimer, &onIrBeaconDetectTimeoutInterrupt);
    timerAlarm(irBeaconDetectTimer, IR_BEACON_DETECT_PERIOD_MS * 1000ULL, true, 0); // periodic timeout alarm
    timerStop(irBeaconDetectTimer);

    // Create software timer for periodic report
    periodicReportTimerHandle = xTimerCreate( 
        "PeriodicReportTimerHandle",
        pdMS_TO_TICKS(IR_BEACON_FREQ_PERIODIC_REPORT_PERIOD),
        pdTRUE,
        (void *) 0,
        onPeriodicReportTimeoutCallback);


    xTaskCreatePinnedToCore(
        irBeaconDetectTask,      // Task function
        "IrBeaconDetectTask",    // Task name
        4096,                // Stack size (bytes)
        NULL,                // Parameters
        1,                   // Priority
        &irBeaconDetectTaskHandle,  // Task handle
        1                  // Core 1
    );
}


void irBeaconDetectTask(void *parameter) {
    FsmNotifQueueItem notif_item;

    for (;;) {
        if (xQueueReceive(
                g_fsmNotifQueue[toIndex(TaskId::IrBeaconDetector)], 
                &notif_item, 
                portMAX_DELAY) == pdPASS) {
            
            DEBUG_LEVEL_2("notif from fsm rcvd by IrBeaconDetector");

            switch (notif_item.type) {
                case FsmNotifType::StateChanged:
                    //DEBUG_LEVEL_1("IR beacon count: %d", count);
                    if (notif_item.data.state == RobotState::CheckHillLoyalty) {
                        timerStart(irBeaconDetectTimer);
                        xTimerStart(periodicReportTimerHandle, 0);
                    } else {
                        // ... 
                        // stop the timer
                        timerStop(irBeaconDetectTimer);
                        xTimerStop(periodicReportTimerHandle, 0);
                    }
                    break;
                default:
                    break;
            }
        }

    }
}


void setupPCNT() {
  pcnt_config_t pcnt_config = {};

  pcnt_config.pulse_gpio_num = IR_BEACON_PCNT_INPUT_PIN;
  pcnt_config.ctrl_gpio_num = PCNT_PIN_NOT_USED;
  pcnt_config.channel = IR_BEACON_PCNT_CHANNEL;
  pcnt_config.unit = IR_BEACON_PCNT_UNIT;

  // Count only rising = num of pulse
  pcnt_config.pos_mode = PCNT_COUNT_INC;  // rising edge
  pcnt_config.neg_mode = PCNT_COUNT_DIS;  // falling edge

  pcnt_config.lctrl_mode = PCNT_MODE_KEEP;
  pcnt_config.hctrl_mode = PCNT_MODE_KEEP;

  pcnt_config.counter_h_lim = 32767;
  pcnt_config.counter_l_lim = -32768;

  pcnt_unit_config(&pcnt_config);

  pcnt_set_filter_value(IR_BEACON_PCNT_UNIT, 100);  // 80MHz -> 100 -> 100/80MHz ~ 1.25us (noise below such time width is reject)
  pcnt_filter_enable(IR_BEACON_PCNT_UNIT);

  pcnt_counter_pause(IR_BEACON_PCNT_UNIT);
  pcnt_counter_clear(IR_BEACON_PCNT_UNIT);
  pcnt_counter_resume(IR_BEACON_PCNT_UNIT);
}
