#include "ir_beacon_detect.h"
#include <Arduino.h>
#include "driver/pcnt.h"
#include "FreeRTOS.h"
#include <stdint.h>
#include "globals.h"

static TaskHandle_t irBeaconDetectTaskHandle = nullptr;
static TimerHandle_t periodicReportTimerHandle = nullptr; // report detected frequency periodically
static QueueHandle_t irBeaconDetectCmdQueue = nullptr; 
void irBeaconDetectTask(void *parameter);
void setupPCNT();
int detectedFreq = 0;
static BeaconState beaconState = BeaconState::Unknown;

hw_timer_t* irBeaconDetectTimer = nullptr; // used for a fixed period of timeout

void onPeriodicReportTimeoutCallback(TimerHandle_t xTimer) {
    DEBUG_LEVEL_1("freq: %d", detectedFreq);
}

void ARDUINO_ISR_ATTR onIrBeaconDetectTimeoutInterrupt() {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    BeaconState newState = BeaconState::Unknown;
    FsmEventQueueItem ev{};

    int16_t pulseCount = 0;
    pcnt_get_counter_value(IR_BEACON_PCNT_UNIT, &pulseCount);
    detectedFreq = pulseCount * 1000 / IR_BEACON_DETECT_PERIOD_MS;
    if (detectedFreq > 700 && detectedFreq < 800) {
        newState = BeaconState::Beacon750;
    } else if (detectedFreq > 1400 && detectedFreq < 1600) {
        newState = BeaconState::Beacon1k5;
    } else {
        newState = BeaconState::Unknown;
    }

    if (newState != beaconState) {
        ev.type = FsmEventType::IrBeaconChangeDetected;
        ev.data.newBeaconState = newState;
        sendFsmEventItemFromISR(ev, xHigherPriorityTaskWoken);
    }
    beaconState = newState;

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

    irBeaconDetectCmdQueue = xQueueCreate(10, sizeof(IrBeaconDetectCtrlCmd));
    if (irBeaconDetectCmdQueue == nullptr) {
        DEBUG_LEVEL_1("Failed to create IrBeaconDetectCmdQueue.");
    }

    // Create software timer for periodic report
    periodicReportTimerHandle = xTimerCreate( 
        "PeriodicReportTimerHandle",
        pdMS_TO_TICKS(IR_BEACON_FREQ_PERIODIC_REPORT_PERIOD),
        pdTRUE,
        (void *) 0,
        onPeriodicReportTimeoutCallback);

    enableRealtimeIrBeaconDetect(true, false);

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
    IrBeaconDetectCtrlCmd cmd;

    for (;;) {
        if (xQueueReceive(
                irBeaconDetectCmdQueue, 
                &cmd, 
                portMAX_DELAY) == pdPASS) {
            
            DEBUG_LEVEL_2("Cmd rcvd by IrBeaconDetector");

            if (cmd.queryBeaconState) {
                // wait a while (~ 3 times of detect period)
                // response with event
                vTaskDelay(pdMS_TO_TICKS(IR_BEACON_DETECT_PERIOD_MS*3));
                FsmEventQueueItem ev{};
                ev.type = FsmEventType::IrBeaconQueryResponse;
                ev.data.newBeaconState = beaconState;
                sendFsmEventItem(ev);
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

  // Count only rising = num of pulses
  pcnt_config.pos_mode = PCNT_COUNT_INC;  // rising edge
  pcnt_config.neg_mode = PCNT_COUNT_DIS;  // falling edge

  pcnt_config.lctrl_mode = PCNT_MODE_KEEP;
  pcnt_config.hctrl_mode = PCNT_MODE_KEEP;

  pcnt_config.counter_h_lim = 32767;
  pcnt_config.counter_l_lim = -32768;

  pcnt_unit_config(&pcnt_config);

  pcnt_set_filter_value(IR_BEACON_PCNT_UNIT, 100);  // APB_CLK 80MHz -> 100 -> 100/80MHz ~ 1.25us (noise below such time width is reject)
  pcnt_filter_enable(IR_BEACON_PCNT_UNIT);

  pcnt_counter_pause(IR_BEACON_PCNT_UNIT);
  pcnt_counter_clear(IR_BEACON_PCNT_UNIT);
  pcnt_counter_resume(IR_BEACON_PCNT_UNIT);
}

bool sendIrBeaconDetectCtrlCmd(const IrBeaconDetectCtrlCmd& cmd) {
    if (irBeaconDetectCmdQueue == nullptr) {
        DEBUG_LEVEL_1("irBeaconDetectCmdQueue is not created");
        return false;
    }
    return xQueueSend(irBeaconDetectCmdQueue, &cmd, 0) == pdPASS;
}

void enableRealtimeIrBeaconDetect(bool enable_update, bool enable_report) {
     if (enable_update) {
        timerStart(irBeaconDetectTimer);
        if (periodicReportTimerHandle != nullptr) {
            if (enable_report) {
                xTimerStart(periodicReportTimerHandle, 0);
            } else {
                xTimerStop(periodicReportTimerHandle, 0);
            }
        }
    } else {
        timerStop(irBeaconDetectTimer);
        if (periodicReportTimerHandle != nullptr) {
            xTimerStop(periodicReportTimerHandle, 0);
        }
    }
}
