#ifndef IR_BEACON_DETECT_H
#define IR_BEACON_DETECT_H

#define IR_BEACON_PCNT_INPUT_PIN  7   // your input pin
#define IR_BEACON_PCNT_UNIT        PCNT_UNIT_0
#define IR_BEACON_PCNT_CHANNEL     PCNT_CHANNEL_0

void initIrBeaconDetect();

struct IrBeaconDetectCtrlCmd {
    bool queryBeaconState;
};

bool sendIrBeaconDetectCtrlCmd(const IrBeaconDetectCtrlCmd& cmd);

void enableRealtimeIrBeaconDetect(bool enable_update, bool enable_report);

#endif
