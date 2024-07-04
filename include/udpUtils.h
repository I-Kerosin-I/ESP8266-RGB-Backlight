#pragma once

#include <Arduino.h>
#include <WiFiUdp.h>
#include "settings.h"

extern WiFiUDP udpClient;

void udpSend(const uint8_t *buffer, size_t size, IPAddress remoteIp);
void udpSend(const char *buffer, size_t size, IPAddress remoteIp);
bool isIpInArray(IPAddress address, IPAddress* array, byte size);
