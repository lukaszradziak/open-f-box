#include <Arduino.h>
#include <stdio.h>
#include "SerialConfig.h"

#include "VehicleState.h"
#include "Payload.h"
#include "CanLink.h"

#define CAN_ID_DATA      0x2B4
#define DATA_INTERVAL_MS 1000

static uint32_t lastDataMs = 0;
static bool sendingEnabled = true;

static void printHelp() {
    Serial.println();
    Serial.println("Commands (no Enter required):");
    Serial.println("  h - show this help");
    Serial.println("  1 - toggle 0x2B4 transmission ON/OFF");
    Serial.print("0x2B4 transmission: ");
    Serial.println(sendingEnabled ? "ON" : "OFF");
}

static void pollConsole() {
    while (Serial.available()) {
        const char command = (char)Serial.read();

        if (command == 'h' || command == 'H') {
            printHelp();
        } else if (command == '1') {
            sendingEnabled = !sendingEnabled;
            if (sendingEnabled) lastDataMs = millis();
            Serial.println(sendingEnabled
                               ? "0x2B4 transmission: ON"
                               : "0x2B4 transmission: OFF");
        } else if (command != '\r' && command != '\n') {
            Serial.print("Unknown command: ");
            Serial.println(command);
            Serial.println("Press h for help.");
        }
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("ford-box - HS/CAN1 500000 bps (OBD 3/11)");
    Serial.println("0x2B4 transmission: ON (press h for help)");

    canLinkBegin();
    lastDataMs = millis();
}

void loop() {
    pollConsole();

    uint32_t now = millis();
    if (sendingEnabled && now - lastDataMs >= DATA_INTERVAL_MS) {
        lastDataMs = now;
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
}
