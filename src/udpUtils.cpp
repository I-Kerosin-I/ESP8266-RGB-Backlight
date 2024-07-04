#include "udpUtils.h"

WiFiUDP udpClient;

void udpSend(const uint8_t *buffer, size_t size, IPAddress remoteIp)
{
  udpClient.beginPacket(remoteIp, UDP_PORT);
  udpClient.write(buffer, size);
  udpClient.endPacket();
}

void udpSend(const char *buffer, size_t size, IPAddress remoteIp)
{
  udpClient.beginPacket(remoteIp, UDP_PORT);
  udpClient.write(reinterpret_cast<const uint8_t*>(buffer), size);
  udpClient.endPacket();
}

bool isIpInArray(IPAddress address, IPAddress* array, byte size)
{
  for (byte i = 0; i < size; i++)
  {
    if (array[i] == address)
    {
      return true;
    }
  }
  return false;
}