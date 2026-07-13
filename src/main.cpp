#include <Arduino.h>
#include <stdio.h>
#include "SerialConfig.h"

#include "CanLink.h"
#include "Console.h"
#include "Payload.h"
#include "VehicleState.h"

#define CAN_ID_DATA      0x2B4
#define DATA_INTERVAL_MS 1000

static uint32_t lastDataMs = 0;

static void sendVehicleState() {
    updateVehicleState();

    uint8_t payload[PAYLOAD_LEN];
    buildPayload(payload);
    sendIsoTp(CAN_ID_DATA, payload, PAYLOAD_LEN);

    // USB CDC is more reliable when a complete line is submitted in one
    // write instead of many consecutive Serial.print() calls.
    char status[112];
    const int length = snprintf(
        status, sizeof(status),
        "TX 0x2B4  TPMS=%u/%u/%u/%u  speed=%u  rpm=%u  fuel=%u%%\r\n",
        state.tpmsFrontLeft, state.tpmsFrontRight,
        state.tpmsRearRight, state.tpmsRearLeft,
        state.speed, state.rpm, state.fuelPercent);
    if (length > 0) Serial.write((const uint8_t *)status, (size_t)length);
}

void setup() {
    consoleBegin();
    canLinkBegin();
    lastDataMs = millis();
}

void loop() {
    consolePoll();
    canLoggerPoll();

    if (consoleTakeTransmissionRestartRequest()) lastDataMs = millis();

    const uint32_t now = millis();
    if (consoleTransmissionEnabled() &&
        now - lastDataMs >= DATA_INTERVAL_MS) {
        lastDataMs = now;
        sendVehicleState();
    }
}
