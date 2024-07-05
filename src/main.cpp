#include <Arduino.h>
#include <GRGB.h>
#include <SPI.h>
#include <EncButton.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRutils.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>

#include "settings.h"
#include "udpUtils.h"
#include "color.h"
#include "test.h"


byte rgbData[3] =           {0};       // RGB
byte rainbowData[2] =       {5, 1};    // Период; Шаг(не доделано)
byte fireData[2] =          {0, 10};   // Стартовый цвет; Шаг

byte curMode = 0;
byte isEnabled = 1;

void fireTick();
inline void updateData(byte *);

bool indicatorState = 0;

uint32_t rainbow_timer = 0, reconnect_timer = 0, sync_timer = 0, keep_alive_timer = 0, indicator_reconnect_timer = 0; // Таймеры
byte cur_rainbow_clr = 0;

byte *modeDataArrayPtrs[] = {rgbData, rainbowData, fireData}; // Указатели на массивы данных режимов
byte modeDataArrayLengths[] = {3,2,2};  // Длины массивов данных режимов

byte dataToSend[6];

GRGB diode(0, R_PIN, G_PIN, B_PIN);     // Инициализация ленты
EncButton eb(S1_PIN, S2_PIN, KEY_PIN);  // Инициализация энкодера

IRrecv irrecv(IR_PIN);                  // Инициализация ИК приёмника
uint32_t IRdata;
decode_results IR_results;

IPAddress clientIps[MAX_CLIENTS];       // Массив ip клиентов
IPAddress KeepAliveIpBuffer[MAX_CLIENTS];
byte KeepAliveIpBufferIndex = 0;
byte client_amount = 0;



void updateDataToSend()
{
  dataToSend[0] = curMode;
  dataToSend[1] = isEnabled;
  dataToSend[2] = diode.getBrightness();
  for (byte j = 0; j < modeDataArrayLengths[curMode]; j++) 
    dataToSend[j + HEADER_LENGTH] = modeDataArrayPtrs[curMode][j];
}

void updateData(byte *data) 
{
  for (byte j = 0; j < modeDataArrayLengths[curMode]; j++) 
    modeDataArrayPtrs[data[0]][j] = data[j + HEADER_LENGTH];
}

inline void changeEnabledState(bool newState)
{
  isEnabled = newState;
  updateDataToSend();
  for (int i = 0; i < client_amount; i++) udpSend(dataToSend, modeDataArrayLengths[dataToSend[0]] + HEADER_LENGTH, clientIps[i]);
  diode.setRGB(0, 0, 0);
}

inline void changeEnabledState(bool newState, IPAddress authorIp)
{
  isEnabled = newState;
  updateDataToSend();
  for (int i = 0; i < client_amount; i++) if(clientIps[i] != authorIp) udpSend(dataToSend, modeDataArrayLengths[dataToSend[0]] + HEADER_LENGTH, clientIps[i]);
  diode.setRGB(0, 0, 0);
}

void setup()
{
  #if DEBUG_EN
    Serial.begin(115200);
  #endif

  delay(10);
  WiFi.begin(SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    DBG_PRINT(F("."));
  }

  DBG_PRINTLN(F(""));
  DBG_PRINTLN(F("WiFi connected"));
  udpClient.begin(UDP_PORT);
  DBG_PRINTLN(F("Server started"));
  irrecv.enableIRIn();
  pinMode(S1_PIN, INPUT);
  pinMode(S2_PIN, INPUT);
  pinMode(INDICATOR_PIN, OUTPUT);
}

void loop()
{
  
  
  if (WiFi.status() != WL_CONNECTED)  
  {
    if (millis() - indicator_reconnect_timer >= 800) 
    {
      indicatorState = !indicatorState;
      digitalWrite(INDICATOR_PIN, indicatorState);
      indicator_reconnect_timer = millis();
    }

    if (millis() - reconnect_timer >= RECONNECT_DELAY)// Переподключение к WI-FI
    {
      DBG_PRINTLN(F("Reconnecting to WIFI network"));
      if(WiFi.reconnect()) 
      {
        indicatorState = false;
        digitalWrite(INDICATOR_PIN, indicatorState);
      }
      reconnect_timer = millis();
    }
  }

  if (udpClient.parsePacket())
  {                                                   // Получение данных от клиента

    IPAddress remoteIP = udpClient.remoteIP();
    byte packetBuffer[32];
    int len = udpClient.read(packetBuffer, 32);
    if (len > 0)
    {
      packetBuffer[len] = 0;
    }

    if (packetBuffer[1] & 0b00100000)
    {                                      // Keep alive пакет
      if (isIpInArray(remoteIP, clientIps, client_amount) && !isIpInArray(remoteIP, KeepAliveIpBuffer, KeepAliveIpBufferIndex))
      {

        KeepAliveIpBuffer[KeepAliveIpBufferIndex++] = remoteIP;
      }
    }

    else if (packetBuffer[1] & 0b00010000) // Запрос на создание канала связи
    {
      if (!(isIpInArray(remoteIP, clientIps, client_amount) || isIpInArray(remoteIP, KeepAliveIpBuffer, KeepAliveIpBufferIndex)))
      {
        updateDataToSend();
        udpSend(dataToSend, modeDataArrayLengths[dataToSend[0]] + 3, remoteIP);
        clientIps[client_amount++] = remoteIP;
        KeepAliveIpBuffer[KeepAliveIpBufferIndex++] = remoteIP;
        DBG_PRINTLN(F("Запрос на создание канала связи"));
      }
      else
      {
        DBG_PRINTLN(F("Попытка дублировать соединение"));
      }
    }

    else if (packetBuffer[1] & 0b00000100) 
    {                                      // Запрос на смену режима
      curMode = packetBuffer[0];
      updateDataToSend();
      for (int i = 0; i < client_amount; i++)
        udpSend(dataToSend, modeDataArrayLengths[dataToSend[0]] + HEADER_LENGTH, clientIps[i]);
      DBG_PRINTLN(F("Запрос на смену режима"));
    }

    else if(packetBuffer[1] & 0b01000000)
    {                                      // Запрос на измененние яркости
      updateDataToSend();
      for (int i = 0; i < client_amount; i++) if(clientIps[i] != remoteIP) udpSend(dataToSend, modeDataArrayLengths[dataToSend[0]] + HEADER_LENGTH, clientIps[i]);
      diode.setBrightness(packetBuffer[2]);
      DBG_PRINTLN(F("Запрос на измененние яркости"));
    }

    else if (packetBuffer[1] & 0b00000010) // Запрос на измененние состояния вкл/выкл
    {
      changeEnabledState(packetBuffer[1] & 0b00000001, remoteIP);
      DBG_PRINTLN(F("Запрос на измененние состояния вкл/выкл"));
    }

    else if (packetBuffer[1] & 0b00001000) // Запрос текущего состояния
    {
      updateDataToSend();
      udpSend(dataToSend, modeDataArrayLengths[dataToSend[0]] + HEADER_LENGTH, remoteIP);
      DBG_PRINTLN(F("Запрос текущего состояния"));
    }

    else if ((packetBuffer[0] != curMode) || ((packetBuffer[1] & 1) != isEnabled)) // Не совпадает режим или сост. вкл/выкл
    {
      updateDataToSend();
      udpSend(dataToSend, modeDataArrayLengths[dataToSend[0]] + HEADER_LENGTH, remoteIP); // Отправляем данные о текущем состоянии
    }
    else
    {
      updateData(packetBuffer); // Применяем принятые данные
      if (millis() - sync_timer > SYNC_DELAY)
      {
        updateDataToSend();
        sync_timer = millis();
        for (int i = 0; i < client_amount; i++)
          if (clientIps[i] != remoteIP)
            udpSend(dataToSend, modeDataArrayLengths[dataToSend[0]] + HEADER_LENGTH, clientIps[i]);
      }
    }
  }

  if (millis() - keep_alive_timer > KEEP_ALIVE_SEND)
  {                                                   // Рассылка KEEP_ALIVE
    for (byte i = 0; i < KeepAliveIpBufferIndex; i++)
    {
      clientIps[i] = KeepAliveIpBuffer[i];
    }
    client_amount = KeepAliveIpBufferIndex;
    KeepAliveIpBufferIndex = 0;
    static byte keep_alive_packet = 0b00100000;
    for (int i = 0; i < client_amount; i++)
      udpSend(&keep_alive_packet, 1U, clientIps[i]);
    keep_alive_timer = millis();
    DBG_PRINTLN(client_amount);
  }

  eb.tick();

  if (eb.hold() && !eb.getClicks()) changeEnabledState(!isEnabled); // TODO: Добавить рассылку-синхронизацию
  
  if (eb.turn())
  {                                                   // Поворот энкодера
    /*
        DBG_PRINT(F("turn: dir "));
        DBG_PRINT(eb.dir());
        DBG_PRINT(F(", fast "));
        DBG_PRINT(eb.fast());
        DBG_PRINT(F(", hold "));
        DBG_PRINT(eb.pressing());
        DBG_PRINT(F(", counter "));
        DBG_PRINT(eb.counter);
        DBG_PRINT(F(", clicks "));
        DBG_PRINTLN(eb.getClicks());
    */
    if (eb.pressing())
    {
      if (eb.getClicks())
      {
        // modeDataArrayPtrs[curMode][constrain(eb.getClicks() - 1, 0, modeDataArrayLengths[curMode] - 1)] += eb.dir() * 10;
        switch (curMode)
        {
        case 0:    // RGB
          switch (eb.getClicks())
          {
          case 1:
            shiftByHsv(rgbData, eb.dir() * 5, 0);
            DBG_PRINT(F("RGB SHIFTED BY ENCODER: "));
            DBG_PRINT(rgbData[0]);
            DBG_PRINT(F(" "));
            DBG_PRINT(rgbData[1]);
            DBG_PRINT(F(" "));
            DBG_PRINTLN(rgbData[2]);
            break;
          case 2:
            shiftByHsv(rgbData, 0, eb.dir() * 5);
            DBG_PRINT(F("RGB SHIFTED BY ENCODER: "));
            DBG_PRINT(rgbData[0]);
            DBG_PRINT(F(" "));
            DBG_PRINT(rgbData[1]);
            DBG_PRINT(F(" "));
            DBG_PRINTLN(rgbData[2]);
            break;
          }
          break;
        case 1:    // Rainbow
          switch (eb.getClicks())
          {
          case 1:
            rainbowData[0] = constrain(rainbowData[0]+eb.dir(), 0, 50);
            break;
          case 2:
            rainbowData[1] = constrain(rainbowData[1]+eb.dir(), 0, 255);
            break;
          }
          break;
        case 2:    // Fire
          switch (eb.getClicks())
          {
          case 1:
            fireData[0] = constrain(fireData[0] + eb.dir() * 5, 0, 255);
            break;
          case 2:
            fireData[1] = constrain(fireData[1] + eb.dir() * 5, 0, 255);
            break;
          }
          break;
        }
      } else
        curMode = (curMode + eb.dir() + MODE_AMOUNT) % MODE_AMOUNT; // TODO: Добавить рассылку-синхронизацию
    } else {
      diode.setBrightness(constrain(diode.getBrightness() + 10 * eb.dir(), 0, 255));
    }
    
  }

  

  if (irrecv.decode(&IR_results))
  {                                                   // ИК приём
    // #if DEBUG_EN
    // serialPrintUint64(IR_results.value, HEX); 
    // #endif
    // DBG_PRINTLN(F(""));

    switch (IR_results.value)
    {
      case 0xF700FF:        // bri ↑
        diode.setBrightness(constrain(diode.getBrightness() + 10, 0, 255));
        DBG_PRINTLN(F("IR: BRI UP"));
        break;

      case 0xF7807F:        // bri ↓
        diode.setBrightness(constrain(diode.getBrightness() - 10, 0, 255));
        DBG_PRINTLN(F("IR: BRI DOWN"));
        break;

      case 0xF740BF:        // ВЫКЛ
        isEnabled = false;
        diode.setRGB(0, 0, 0);
        DBG_PRINTLN(F("IR: ВЫКЛ"));
        break;

      case 0xF7C03F:        // ВКЛ
        isEnabled = true;
        DBG_PRINTLN(F("IR: ВКЛ"));
        break;

      case 0xF720DF:        // RED
        curMode = 0;
        rgbData[0] = 255;
        rgbData[1] = 0;
        rgbData[2] = 0;
        DBG_PRINTLN(F("IR: RED"));
        break;

      case 0xF7A05F:        // GREEN
        curMode = 0;
        rgbData[0] = 0;
        rgbData[1] = 255;
        rgbData[2] = 0;
        DBG_PRINTLN(F("IR: GREEN"));
        break;

      case 0xF7609F:        // BLUE
        curMode = 0;
        rgbData[0] = 0;
        rgbData[1] = 0;
        rgbData[2] = 255;
        DBG_PRINTLN(F("IR: BLUE"));
        break;

      case 0xF7E01F:        // WHITE
        curMode = 0;
        rgbData[0] = 255;
        rgbData[1] = 255;
        rgbData[2] = 255;
        DBG_PRINTLN(F("IR: WHITE"));
        break;
      
      case 0xF710EF:        // COL 1
        curMode = 0;
        rgbData[0] = 255;
        rgbData[1] = 24;
        rgbData[2] = 0;
        DBG_PRINTLN(F("IR: COL 1"));
        break;

      case 0xF7906F:        // COL 2
        curMode = 0;
        rgbData[0] = 0;
        rgbData[1] = 255;
        rgbData[2] = 43;
        DBG_PRINTLN(F("IR: COL 2"));
        break;

      case 0xF750AF:        // COL 3
        curMode = 0;
        rgbData[0] = 0;
        rgbData[1] = 38;
        rgbData[2] = 255;
        DBG_PRINTLN(F("IR: COL 3"));
        break;

      case 0xF7D02F:        // FIRE
        curMode = 2;
        DBG_PRINTLN(F("IR: FIRE"));
        break;

      case 0xF730CF:        // COL 4
        curMode = 0;
        rgbData[0] = 255;
        rgbData[1] = 52;
        rgbData[2] = 0;
        DBG_PRINTLN(F("IR: COL 4"));
        break;

      case 0xF7B04F:        // COL 5
        curMode = 0;
        rgbData[0] = 0;
        rgbData[1] = 255;
        rgbData[2] = 132;
        DBG_PRINTLN(F("IR: COL 5"));
        break;

      case 0xF7708F:        // COL 6
         curMode = 0;
        rgbData[0] = 58;
        rgbData[1] = 0;
        rgbData[2] = 255;
        DBG_PRINTLN(F("IR: COL 6"));
        break;

      case 0xF708F7:        // COL 7
        curMode = 0;
        rgbData[0] = 255;
        rgbData[1] = 94;
        rgbData[2] = 0;
        DBG_PRINTLN(F("IR: COL 7"));
        break;

      case 0xF78877:        // COL 8
        curMode = 0;
        rgbData[0] = 0;
        rgbData[1] = 255;
        rgbData[2] = 193;
        DBG_PRINTLN(F("IR: COL 8"));
        break;

      case 0xF748B7:        // COL 9
         curMode = 0;
        rgbData[0] = 130;
        rgbData[1] = 0;
        rgbData[2] = 255;
        DBG_PRINTLN(F("IR: COL 9"));
        break;

      case 0xF7C837:        // RAINBOW
        curMode = 1;
        DBG_PRINTLN(F("IR: RAINBOW"));
        break;

      case 0xF728D7:        // COL 10
        curMode = 0;
        rgbData[0] = 255;
        rgbData[1] = 156;
        rgbData[2] = 0;
        DBG_PRINTLN(F("IR: COL 10"));
        break;

      case 0xF7A857:        // COL 11
        curMode = 0;
        rgbData[0] = 0;
        rgbData[1] = 255;
        rgbData[2] = 255;
        DBG_PRINTLN(F("IR: COL 11"));
        break;

      case 0xF76897:        // COL 12
         curMode = 0;
        rgbData[0] = 255;
        rgbData[1] = 0;
        rgbData[2] = 33;
        DBG_PRINTLN(F("IR: COL 12"));
        break;


    
    }
    updateDataToSend();
      for (int i = 0; i < client_amount; i++)
        udpSend(dataToSend, modeDataArrayLengths[dataToSend[0]] + HEADER_LENGTH, clientIps[i]);
      
    irrecv.resume(); // Receive the next value
  }

  if (isEnabled)
  {                                                   // Обновление цвета
    switch (curMode)
    {
    case 0:
      diode.setRGB(rgbData[0], rgbData[1], rgbData[2]);
      break;
    case 1:
      if (millis() - rainbow_timer > rainbowData[0])
      {
        rainbow_timer = millis();
        cur_rainbow_clr++;
      }
      diode.setHSV(cur_rainbow_clr, 255, 255);
      break;
    case 2:
      fireTick();
    }
  }
}




void fireTick()
{
  static uint32_t prevTime, prevTime2;
  static byte fireRnd = 0;
  static float fireValue = 0;

  // задаём направление движения огня
  if (millis() - prevTime > 100)
  {
    prevTime = millis();
    fireRnd = random(0, 10);
  }
  // двигаем пламя
  if (millis() - prevTime2 > 20)
  {
    prevTime2 = millis();
    fireValue = (float)fireValue * (1 - SMOOTH_K) + (float)fireRnd * 10 * SMOOTH_K;
    diode.setHSV(
        constrain(map(fireValue, 20, 60, fireData[0], fireData[0] + fireData[1]), 0, 255), // H
        constrain(map(fireValue, 20, 60, MAX_SAT, MIN_SAT), 0, 255),                       // S
        constrain(map(fireValue, 20, 60, MIN_BRIGHT, MAX_BRIGHT), 0, 255)                  // V
    );
  }
}
