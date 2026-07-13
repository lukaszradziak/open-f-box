#include "CanLink.h"
#include "SerialConfig.h"
#include <Arduino.h>
#include <STM32_CAN.h>
#include <stdio.h>

#define STB_CAN1 PB3
#define STB_CAN2 PB7
#define STB_CAN3 PB4

#define CAN1_BAUD 500000UL
#define CAN2_BAUD 500000UL
#define CAN3_BAUD 125000UL
#define CF_INTERVAL_MS 20

static STM32_CAN can1(PB8, PB9);    // OBD 3/11, bxCAN1 remap.
static STM32_CAN can2(PB12, PB13);  // OBD 6/14, bxCAN2 default.
static STM32_CAN can3(PB5, PB6);    // OBD 1/8, bxCAN2 remap.

static CanLoggerBus loggerBus = CanLoggerBus::NONE;
static bool can1Started = false;
static bool loggerFilterEnabled = false;
static uint16_t loggerFilterLo = 0;
static uint16_t loggerFilterHi = 0;

static STM32_CAN *loggerInstance(CanLoggerBus bus) {
    switch (bus) {
        case CanLoggerBus::PORT1: return &can1;
        case CanLoggerBus::PORT2: return &can2;
        case CanLoggerBus::PORT3: return &can3;
        default: return nullptr;
    }
}

static uint32_t loggerStbPin(CanLoggerBus bus) {
    switch (bus) {
        case CanLoggerBus::PORT1: return STB_CAN1;
        case CanLoggerBus::PORT2: return STB_CAN2;
        case CanLoggerBus::PORT3: return STB_CAN3;
        default: return 0;
    }
}

uint32_t canLoggerBaudRate(CanLoggerBus bus) {
    switch (bus) {
        case CanLoggerBus::PORT1: return CAN1_BAUD;
        case CanLoggerBus::PORT2: return CAN2_BAUD;
        case CanLoggerBus::PORT3: return CAN3_BAUD;
        default: return 0;
    }
}

const char *canLoggerBusName(CanLoggerBus bus) {
    switch (bus) {
        case CanLoggerBus::PORT1: return "CAN1";
        case CanLoggerBus::PORT2: return "CAN2";
        case CanLoggerBus::PORT3: return "CAN3";
        default: return "OFF";
    }
}

static uint32_t maskBlockSize(uint32_t lo, uint32_t hi) {
    uint32_t size = 1;
    while ((lo & ((size << 1) - 1)) == 0 &&
           lo + ((size << 1) - 1) <= hi) {
        size <<= 1;
    }
    return size;
}

static uint8_t maskBlockCount(uint32_t lo, uint32_t hi) {
    uint8_t count = 0;
    while (lo <= hi) {
        lo += maskBlockSize(lo, hi);
        count++;
    }
    return count;
}

static void disableAllFilters(STM32_CAN &can) {
    const uint8_t bankCount = can.getFilterBankCount(STD);
    for (uint8_t bank = 0; bank < bankCount; ++bank) {
        can.setFilter(bank, false);
    }
}

static void applyLoggerFilter() {
    STM32_CAN *can = loggerInstance(loggerBus);
    if (!can) return;

    const uint8_t bankCount = can->getFilterBankCount(STD);
    uint8_t bank = 0;

    if (!loggerFilterEnabled) {
        can->setFilterRaw(bank++, 0, 0, CAN_FILTERMODE_IDMASK,
                          CAN_FILTERSCALE_32BIT);
    } else if (maskBlockCount(loggerFilterLo, loggerFilterHi) <= bankCount) {
        uint32_t lo = loggerFilterLo;
        while (lo <= loggerFilterHi) {
            const uint32_t size = maskBlockSize(lo, loggerFilterHi);
            can->setFilterSingleMask(bank++, lo,
                                     0x7FFUL & ~(size - 1), STD);
            lo += size;
        }
    } else {
        // Accept a common-prefix superset in hardware. canLoggerPoll() makes
        // the final exact range check in software.
        uint32_t mask = 0x7FF;
        while ((loggerFilterLo & mask) != (loggerFilterHi & mask)) {
            mask = (mask << 1) & 0x7FF;
        }
        can->setFilterSingleMask(bank++, loggerFilterLo & mask, mask, STD);
    }

    while (bank < bankCount) can->setFilter(bank++, false);
}

static void discardReceivedFrames(STM32_CAN &can) {
    CAN_message_t msg;
    while (can.read(msg)) {}
}

static void startCan1Application() {
    digitalWrite(STB_CAN1, LOW);
    can1.begin();
    __HAL_AFIO_REMAP_CAN1_2();
    can1.setBaudRate(CAN1_BAUD);
    can1Started = true;

    // The application only transmits. Do not fill its RX FIFO unless the
    // CAN1 logger is explicitly enabled.
    disableAllFilters(can1);
}

static void stopCurrentLoggerHardware() {
    if (loggerBus == CanLoggerBus::NONE) return;

    if (loggerBus == CanLoggerBus::PORT1) {
        disableAllFilters(can1);
        discardReceivedFrames(can1);
    } else {
        STM32_CAN *can = loggerInstance(loggerBus);
        if (can) can->end();
        digitalWrite(loggerStbPin(loggerBus), HIGH);
    }
    loggerBus = CanLoggerBus::NONE;
}

void canLinkBegin() {
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_AFIO_REMAP_SWJ_NOJTAG();

    pinMode(STB_CAN1, OUTPUT);
    pinMode(STB_CAN2, OUTPUT);
    pinMode(STB_CAN3, OUTPUT);
    digitalWrite(STB_CAN1, HIGH);
    digitalWrite(STB_CAN2, HIGH);
    digitalWrite(STB_CAN3, HIGH);

    startCan1Application();
}

bool canLoggerStart(CanLoggerBus bus) {
    if (bus == CanLoggerBus::NONE) return false;

    stopCurrentLoggerHardware();

    if (bus == CanLoggerBus::PORT1) {
        if (!can1Started) startCan1Application();
    } else {
        if (can1Started) {
            can1.end();
            can1Started = false;
            digitalWrite(STB_CAN1, HIGH);
        }

        STM32_CAN *can = loggerInstance(bus);
        if (!can) return false;
        digitalWrite(loggerStbPin(bus), LOW);
        can->begin();
        can->setBaudRate(canLoggerBaudRate(bus));
    }

    loggerBus = bus;
    applyLoggerFilter();
    STM32_CAN *can = loggerInstance(loggerBus);
    if (can) discardReceivedFrames(*can);
    return true;
}

void canLoggerStop() {
    stopCurrentLoggerHardware();
    if (!can1Started) startCan1Application();
}

CanLoggerBus canLoggerActiveBus() {
    return loggerBus;
}

void canLoggerSetFilter(uint16_t idLo, uint16_t idHi) {
    loggerFilterEnabled = true;
    loggerFilterLo = idLo;
    loggerFilterHi = idHi;
    applyLoggerFilter();

    STM32_CAN *can = loggerInstance(loggerBus);
    if (can) discardReceivedFrames(*can);
}

void canLoggerClearFilter() {
    loggerFilterEnabled = false;
    applyLoggerFilter();

    STM32_CAN *can = loggerInstance(loggerBus);
    if (can) discardReceivedFrames(*can);
}

bool canLoggerHasFilter() {
    return loggerFilterEnabled;
}

uint16_t canLoggerFilterLo() {
    return loggerFilterLo;
}

uint16_t canLoggerFilterHi() {
    return loggerFilterHi;
}

void canLoggerPoll() {
    STM32_CAN *can = loggerInstance(loggerBus);
    if (!can) return;

    CAN_message_t msg;
    while (can->read(msg)) {
        if (loggerFilterEnabled &&
            (msg.id < loggerFilterLo || msg.id > loggerFilterHi)) {
            continue;
        }

        // A single USB CDC write prevents the missing characters observed
        // when a line was assembled with many Serial.print() calls.
        char line[48];
        size_t used = (size_t)snprintf(line, sizeof(line), "%03lX",
                                      (unsigned long)msg.id);
        for (uint8_t i = 0; i < msg.len && used < sizeof(line); ++i) {
            used += (size_t)snprintf(line + used, sizeof(line) - used,
                                     " %02X", msg.buf[i]);
        }
        if (used + 2 <= sizeof(line)) {
            line[used++] = '\r';
            line[used++] = '\n';
        }
        Serial.write((const uint8_t *)line, used);
    }
}

void sendIsoTp(uint16_t id, const uint8_t *payload, uint8_t len) {
    if (!can1Started || loggerBus != CanLoggerBus::NONE) return;

    CAN_message_t msg;
    msg.id = id;
    msg.len = 8;

    msg.buf[0] = 0x10 | ((len >> 8) & 0x0F);
    msg.buf[1] = len & 0xFF;
    uint8_t chunk = (len < 6) ? len : 6;
    for (uint8_t i = 0; i < chunk; i++) msg.buf[2 + i] = payload[i];
    for (uint8_t i = chunk; i < 6; i++) msg.buf[2 + i] = 0x00;
    can1.write(msg);

    uint8_t sent = chunk;
    uint8_t seq = 1;
    while (sent < len) {
        delay(CF_INTERVAL_MS);
        msg.buf[0] = 0x20 | (seq & 0x0F);
        uint8_t remaining = len - sent;
        uint8_t n = (remaining < 7) ? remaining : 7;
        for (uint8_t i = 0; i < n; i++) msg.buf[1 + i] = payload[sent + i];
        for (uint8_t i = n; i < 7; i++) msg.buf[1 + i] = 0x00;
        can1.write(msg);
        sent += n;
        seq++;
    }
}
