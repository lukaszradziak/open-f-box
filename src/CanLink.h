#pragma once
#include <stdint.h>

enum class CanLoggerBus : uint8_t { NONE, PORT1, PORT2, PORT3 };

void canLinkBegin();
void sendIsoTp(uint16_t id, const uint8_t *payload, uint8_t len);

bool canLoggerStart(CanLoggerBus bus);
void canLoggerStop();
CanLoggerBus canLoggerActiveBus();
const char *canLoggerBusName(CanLoggerBus bus);
uint32_t canLoggerBaudRate(CanLoggerBus bus);
void canLoggerPoll();

void canLoggerSetFilter(uint16_t idLo, uint16_t idHi);
void canLoggerClearFilter();
bool canLoggerHasFilter();
uint16_t canLoggerFilterLo();
uint16_t canLoggerFilterHi();
