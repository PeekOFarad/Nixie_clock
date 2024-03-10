#include <Arduino.h>
#include <ShiftRegister74HC595.h>
#include <WiFiManager.h>
#include <Wifi.h>
#include <time.h>
#include <ESP32Time.h>


const int numberOfShiftRegisters = 4; // number of shift registers attached in series
const int serialDataPin = 5; // DS
const int clockPin = 7; // SHCP
const int latchPin = 6; // STCP
ShiftRegister74HC595<numberOfShiftRegisters> sr(serialDataPin, clockPin, latchPin);
const int btn = 3; //Capacitive button

uint32_t PinValuesA = 0b01000001000000000001000000000001;
uint32_t PinValuesB = 0b01000010000000000010000000000010;
/*
00 987654 3210 9876 543210 98 76543210
01 000001 0000 0000 000100 00 00000001
01 000010 0000 0000 001000 00 00000010
*/
uint8_t PinValues_T[] = {0,0,0,0};
uint8_t PinValues_U[] = {0,0,0,0};

const int interruptPin = 10;

unsigned long prevMillis = 0;
unsigned int prevSec = 0;

const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;

uint32_t H_T,H_U,M_T,M_U,S_T,S_U = 0;

WiFiManager wifiManager;

const char* timezone = "CET-1CEST,M3.5.0,M10.5.0/3";  // TimeZone rule for Europe/Rome including daylight adjustment rules (optional)

ESP32Time rtc(0);

struct tm timeinfo;

void IRAM_ATTR ISR() {
  if((digitalRead(interruptPin) == LOW)) {
    sr.setAll(PinValues_U);
  }
  else {
    sr.setAll(PinValues_T);
  }  
}

void setTimezone(String timezone){
  //Serial.printf("  Setting Timezone to %s\n",timezone.c_str());
  setenv("TZ",timezone.c_str(),1);  //  Now adjust the TZ.  Clock settings are adjusted to show the new local time
  tzset();
}

void initTime(String timezone){
  // struct tm timeinfo;

  // Serial.println("Setting up time");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);    // First connect to NTP server, with 0 TZ offset
  if(!getLocalTime(&timeinfo)){
    // Serial.println("  Failed to obtain time");
    return;
  }
  // Serial.println("  Got the time from NTP");
  // Now we can set the real timezone
  setTimezone(timezone);
}

void printLocalTime()
{
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

inline uint32_t bit_set(uint32_t vector, uint32_t n) {
      return vector | ((uint32_t)1 << n);
}

void loadPinRegs(){
  //clear pins registers
    PinValuesA = 0;
    PinValuesB = 0;
    //Load values to display correct digits
    PinValuesA = bit_set(PinValuesA, H_T);
    PinValuesB = bit_set(PinValuesB, H_U);

    PinValuesA = bit_set(PinValuesA, M_T+10);
    PinValuesB = bit_set(PinValuesB, M_U+10);

    PinValuesA = bit_set(PinValuesA, S_T+20);
    PinValuesB = bit_set(PinValuesB, S_U+20);
    //load register values into bytes in PinValues arrays
    //This is needed, because the ShiftReg library uses Byte arrays to load data
    for (int i = 0; i <= 3;i++) {
      PinValues_T[i] = (PinValuesA >> (i)*8) & 0xFF;
      PinValues_U[i] = (PinValuesB >> (i)*8) & 0xFF;
    }
}

void setup() {
  Serial.begin(115200);
  ////////////////////////////////////////////////////////////////////////////////////////
  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  
  //reset saved settings
  //wifiManager.resetSettings();
  
  //set custom ip for portal
  // wifiManager.setAPStaticIPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
  
  //fetches ssid and pass from eeprom and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  wifiManager.autoConnect("AutoConnectAP");
  //or use this for auto generated name ESP + ChipID
  //wifiManager.autoConnect();
  
  //if you get here you have connected to the WiFi
  // Serial.println("connected...yeey :)");
  //////////////////////////////////////////////////////////////////////////////////////
  // configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);
  // printLocalTime();
  initTime(timezone);
  rtc.setTimeStruct(timeinfo);
  //Shift Register
  pinMode(serialDataPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(latchPin, OUTPUT);
  //Interrupt (ZeroCross detection)
  pinMode(interruptPin, INPUT);
  attachInterrupt(interruptPin, ISR, CHANGE);

  wifiManager.setConfigPortalTimeout(5);
  wifiManager.setWiFiAutoReconnect(false);
  wifiManager.disconnect();
}

void loop() {
  unsigned long currentMillis = millis();
  if ((currentMillis - prevMillis >= 50)) {
    prevMillis = currentMillis;
    timeinfo = rtc.getTimeStruct();
  }

  if (digitalRead(btn) == HIGH){
    S_T = ((timeinfo.tm_year + 1900) % 100) / 10;
    S_U = ((timeinfo.tm_year + 1900) % 100) % 10;
    M_T = (timeinfo.tm_mon + 1) / 10; // Month is zero-based, so adding 1
    M_U = (timeinfo.tm_mon + 1) % 10; // Month is zero-based, so adding 1
    H_T = timeinfo.tm_mday / 10;
    H_U = timeinfo.tm_mday % 10;
    loadPinRegs();
    while (digitalRead(btn) != LOW) {}// Check if the button is released

    delay(5000);
  }
  else if ((prevSec != timeinfo.tm_sec)) {
    prevSec = timeinfo.tm_sec;
    //if it's midnight, get atomic time
    if ((timeinfo.tm_hour == 0)&&(timeinfo.tm_min == 0)&&(timeinfo.tm_sec == 0)) {
      wifiManager.autoConnect("AutoConnectAP");
      getLocalTime(&timeinfo);
      wifiManager.disconnect();
    }
    //Get tens and units of time 
    H_T = timeinfo.tm_hour/10;
    H_U = timeinfo.tm_hour%10;
    M_T = timeinfo.tm_min/10;
    M_U = timeinfo.tm_min%10;
    S_T = timeinfo.tm_sec/10;
    S_U = timeinfo.tm_sec%10;
    
    loadPinRegs();
  }
}
