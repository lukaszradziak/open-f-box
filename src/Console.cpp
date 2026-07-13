#include "Console.h"
#include "CanLink.h"
#include "DfuBootloader.h"
#include "SerialConfig.h"
#include <Arduino.h>
#include <stdlib.h>
#include <string.h>

static bool sendingEnabled = true;
static bool transmissionRestartRequested = false;
static char lineCommand[20];
static uint8_t lineCommandLength = 0;
static bool readingLineCommand = false;

static void printLoggerFilter() {
    Serial.print("CAN logger filter: ");
    if (!canLoggerHasFilter()) {
        Serial.println("ALL");
        return;
    }

    Serial.print(canLoggerFilterLo(), HEX);
    Serial.print("-");
    Serial.println(canLoggerFilterHi(), HEX);
}

static void printLoggerState() {
    const CanLoggerBus bus = canLoggerActiveBus();
    Serial.print("CAN logger: ");
    Serial.print(canLoggerBusName(bus));
    if (bus != CanLoggerBus::NONE) {
        Serial.print(" @ ");
        Serial.print(canLoggerBaudRate(bus));
        Serial.println(" bps");
    } else {
        Serial.println();
    }
    printLoggerFilter();
}

static void printHelp() {
    Serial.println();
    Serial.println("Commands (no Enter required):");
    Serial.println("  h - show this help");
    Serial.println("  1 - toggle 0x2B4 transmission ON/OFF");
    Serial.println("  u - reboot into USB DFU firmware-update mode");
    Serial.println("Commands completed with Enter:");
    Serial.println("  l.can1 / l.can2 / l.can3 - start CAN logger");
    Serial.println("  l or l.off - stop CAN logger");
    Serial.println("  f.<id> - filter one standard hex CAN ID");
    Serial.println("  f.<lo>.<hi> - filter inclusive hex ID range");
    Serial.println("  f - clear filter (accept all IDs)");
    Serial.print("0x2B4 transmission: ");
    Serial.println(sendingEnabled ? "ON" : "OFF");
    printLoggerState();
    Serial.print("Factory ROM bootloader: v");
    Serial.print(dfuBootloaderVersion());
    Serial.print(" (vector 0x");
    Serial.print(dfuBootloaderResetVector(), HEX);
    Serial.println(")");
}

static CanLoggerBus parseLoggerBus(const char *line) {
    if (strcmp(line, "l.can1") == 0) return CanLoggerBus::PORT1;
    if (strcmp(line, "l.can2") == 0) return CanLoggerBus::PORT2;
    if (strcmp(line, "l.can3") == 0) return CanLoggerBus::PORT3;
    return CanLoggerBus::NONE;
}

static void handleLoggerCommand(const char *line) {
    if (strcmp(line, "l") == 0 || strcmp(line, "l.off") == 0) {
        canLoggerStop();
        Serial.println("CAN logger: OFF");
        Serial.print("0x2B4 transmission: ");
        Serial.println(sendingEnabled ? "ON" : "OFF (press 1 to enable)");
        return;
    }

    const CanLoggerBus bus = parseLoggerBus(line);
    if (bus == CanLoggerBus::NONE) {
        Serial.println("ERROR: use l.can1, l.can2, l.can3, l or l.off");
        return;
    }

    sendingEnabled = false;
    transmissionRestartRequested = false;
    if (!canLoggerStart(bus)) {
        Serial.println("ERROR: failed to start CAN logger");
        return;
    }

    Serial.println("0x2B4 transmission: OFF (logger active)");
    printLoggerState();
}

static void handleFilterCommand(const char *line) {
    if (strcmp(line, "f") == 0) {
        canLoggerClearFilter();
        Serial.println("CAN logger filter: ALL");
        return;
    }

    if (strncmp(line, "f.", 2) != 0) {
        Serial.println("ERROR: use f, f.<id> or f.<lo>.<hi>");
        return;
    }

    char *end = nullptr;
    const unsigned long lo = strtoul(line + 2, &end, 16);
    if (end == line + 2 || lo > 0x7FF) {
        Serial.println("ERROR: CAN ID must be hex 000-7FF");
        return;
    }

    unsigned long hi = lo;
    if (*end == '.') {
        char *rangeEnd = nullptr;
        hi = strtoul(end + 1, &rangeEnd, 16);
        if (rangeEnd == end + 1 || *rangeEnd != '\0') {
            Serial.println("ERROR: use f.<lo>.<hi>, e.g. f.200.2ff");
            return;
        }
    } else if (*end != '\0') {
        Serial.println("ERROR: use f.<id> or f.<lo>.<hi>");
        return;
    }

    if (hi < lo || hi > 0x7FF) {
        Serial.println("ERROR: filter requires 000 <= lo <= hi <= 7FF");
        return;
    }

    canLoggerSetFilter((uint16_t)lo, (uint16_t)hi);
    printLoggerFilter();
}

static void handleLineCommand() {
    lineCommand[lineCommandLength] = '\0';

    // Command names are case-insensitive; hexadecimal digits remain valid.
    for (uint8_t i = 0; i < lineCommandLength; ++i) {
        if (lineCommand[i] >= 'A' && lineCommand[i] <= 'Z') {
            lineCommand[i] = (char)(lineCommand[i] - 'A' + 'a');
        }
    }

    if (lineCommand[0] == 'l') {
        handleLoggerCommand(lineCommand);
    } else if (lineCommand[0] == 'f') {
        handleFilterCommand(lineCommand);
    }
}

static void beginLineCommand(char firstCharacter) {
    readingLineCommand = true;
    lineCommandLength = 0;
    lineCommand[lineCommandLength++] = firstCharacter;
}

static void appendLineCommand(char character) {
    if (character == '\r' || character == '\n') {
        handleLineCommand();
        readingLineCommand = false;
        lineCommandLength = 0;
    } else if (character == '\b' || character == 0x7F) {
        if (lineCommandLength > 0) lineCommandLength--;
    } else if (lineCommandLength < sizeof(lineCommand) - 1) {
        lineCommand[lineCommandLength++] = character;
    } else {
        Serial.println("ERROR: command too long");
        readingLineCommand = false;
        lineCommandLength = 0;
    }
}

static void toggleTransmission() {
    if (canLoggerActiveBus() != CanLoggerBus::NONE) {
        Serial.println("ERROR: stop CAN logger before enabling 0x2B4");
        return;
    }

    sendingEnabled = !sendingEnabled;
    if (sendingEnabled) transmissionRestartRequested = true;
    Serial.println(sendingEnabled
                       ? "0x2B4 transmission: ON"
                       : "0x2B4 transmission: OFF");
}

static void enterDfu() {
    Serial.println("Entering USB DFU bootloader...");
    Serial.flush();
    delay(50);
    Serial.end();
    delay(1000);
    dfuRequestBootloader();
}

void consoleBegin() {
    Serial.begin(115200);
    Serial.println("ford-box - HS/CAN1 500000 bps (OBD 3/11)");
    Serial.println("0x2B4 transmission: ON (press h for help)");
}

void consolePoll() {
    while (Serial.available()) {
        const char command = (char)Serial.read();

        if (readingLineCommand) {
            appendLineCommand(command);
            continue;
        }

        if (command == 'h' || command == 'H') {
            printHelp();
        } else if (command == '1') {
            toggleTransmission();
        } else if (command == 'l' || command == 'L' ||
                   command == 'f' || command == 'F') {
            beginLineCommand(command);
        } else if (command == 'u' || command == 'U') {
            enterDfu();
        } else if (command != '\r' && command != '\n') {
            Serial.print("Unknown command: ");
            Serial.println(command);
            Serial.println("Press h for help.");
        }
    }
}

bool consoleTransmissionEnabled() {
    return sendingEnabled;
}

bool consoleTakeTransmissionRestartRequest() {
    const bool requested = transmissionRestartRequested;
    transmissionRestartRequested = false;
    return requested;
}
