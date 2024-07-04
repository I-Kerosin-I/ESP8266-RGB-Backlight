#include "test.h"
#include "udpUtils.h"

void IRAM_ATTR Gpio2Interrupt() {
  DBG_UDP("D4 Interrupt!", sizeof("D4 Interrupt!"));
  DBG_PRINTLN("D4 Interrupt!");
}
void IRAM_ATTR Gpio0Interrupt() {
  DBG_UDP("D2 Interrupt!", sizeof("D2 Interrupt!"));
  DBG_PRINTLN("D2 Interrupt!");
}