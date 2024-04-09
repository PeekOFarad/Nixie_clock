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
uint32_t nixie[6] = {0,0,0,0,0,0};

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
  if(!getLocalTime(&timeinfo)) {
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
inline uint32_t bit_clr(uint32_t vector, uint32_t n) {
      return vector & ~((uint32_t)1 << n);
}
//Splits 32 bit word into 8but words for the shift regsiter library
void loadShiftRegs(){
  for (int i = 0; i <= 3;i++) {
      PinValues_T[i] = (PinValuesA >> (i)*8) & 0xFF;
      PinValues_U[i] = (PinValuesB >> (i)*8) & 0xFF;
    }
}
//converts integers into a 32bit word, that controlls the actual nixie pins
void loadPinRegs(bool zero = false){
  //clear pins registers
    PinValuesA = 0;
    PinValuesB = 0;
    //Load values to display correct digits
    if (!zero) {
      PinValuesA = bit_set(PinValuesA, nixie[5]);
      PinValuesB = bit_set(PinValuesB, nixie[4]);

      PinValuesA = bit_set(PinValuesA, nixie[3]+10);
      PinValuesB = bit_set(PinValuesB, nixie[2]+10);

      PinValuesA = bit_set(PinValuesA, nixie[1]+20);
      PinValuesB = bit_set(PinValuesB, nixie[0]+20);
    }
    //load register values into bytes in PinValues arrays
    //This is needed, because the ShiftReg library uses Byte arrays to load data
    loadShiftRegs();
}

void show_date() {
  nixie[0] = ((timeinfo.tm_year + 1900) % 100) % 10;
  nixie[1] = ((timeinfo.tm_year + 1900) % 100) / 10;
  nixie[2] = (timeinfo.tm_mon + 1) % 10; // Month is zero-based, so adding 1
  nixie[3] = (timeinfo.tm_mon + 1) / 10; // Month is zero-based, so adding 1
  nixie[4] = timeinfo.tm_mday % 10;
  nixie[5] = timeinfo.tm_mday / 10;
  loadPinRegs();
}

void stopwatch() {
  //stopwatch
  memset(nixie, 0, sizeof(nixie));
  loadPinRegs();
  while(digitalRead(btn) == LOW) {} //wait for btn press
  while(digitalRead(btn) == HIGH) {} //wait for btn depress
  unsigned long startTime = millis();
  while(digitalRead(btn) == LOW) {  //start stopwatch
    unsigned long currentTime = millis(); // Get the current time

    // Calculate elapsed time since the start time
    unsigned long elapsedTime = currentTime - startTime;

    // Calculate hundredths of a second, seconds, and minutes
    unsigned int hundredths = (elapsedTime / 10) % 100;
    unsigned int seconds = (elapsedTime / 1000) % 60;
    unsigned int minutes = (elapsedTime / 60000) % 60;

    // Extract units and tens digits
    nixie[0] = hundredths % 10;      // Units digit of hundredths
    nixie[1] = hundredths / 10;      // Tens digit of hundredths
    nixie[2] = seconds % 10;         // Units digit of seconds
    nixie[3] = seconds / 10;         // Tens digit of seconds
    nixie[4] = minutes % 10;         // Units digit of minutes
    nixie[5] = minutes / 10;         // Tens digit of minutes

    loadPinRegs(); // Assuming this function updates the display
  }
  while(digitalRead(btn) == HIGH) {} //wait for btn depress
  //wait for button press before jumping out of stopwatch mode
  while(digitalRead(btn) == LOW) {} //wait for btn press
}

void lightshow() {
  //blink progresively faster
  for (int i = 0; i < 20; i++) {
    loadPinRegs(true);
    delay(200-i*10);
    loadPinRegs();
    delay(200-i*10);
  }
  //blink fast for a bit
  for (int i = 0; i < 10; i++) {
    loadPinRegs(true);
    delay(20);
    loadPinRegs();
    delay(20);
  }
  loadPinRegs(true);
  delay(500);
  //memset(nixie, 0, sizeof(nixie)); // zero out
  //NUMBER WAVE
  const int delay_wave = 30;
  for (int l = 0; l < 9; l++) {
    for (int i = 2; i >= 0; i--) {
      PinValuesB = bit_set(PinValuesB, i*10+l);
      loadShiftRegs(); 
      delay(delay_wave);//50
      PinValuesA = bit_set(PinValuesA, i*10+l);
      loadShiftRegs();
      delay(delay_wave); 
    }
    for (int i = 2; i >= 0; i--) {
      PinValuesB = bit_clr(PinValuesB, i*10+l);
      loadShiftRegs(); 
      delay(delay_wave);
      PinValuesA = bit_clr(PinValuesA, i*10+l);
      loadShiftRegs();
      delay(delay_wave); 
    }
  }
  //NUMBER PONG
  const int delay_pong = 70;
  for (int l = 9; l >= 0; l--) {
    //shift number l from left to right
    for (int i = 2; i >= 0; i--) {
      PinValuesB = bit_set(PinValuesB, i*10+l);
      loadShiftRegs(); 
      delay(delay_pong);
      PinValuesB = bit_clr(PinValuesB, i*10+l);
      PinValuesA = bit_set(PinValuesA, i*10+l);
      loadShiftRegs();
      delay(delay_pong);
      PinValuesA = bit_clr(PinValuesA, i*10+l); 
    }

    //shift the number l one to the right
    PinValuesB = bit_set(PinValuesB, l);
    loadShiftRegs();
    delay(delay_pong);
    PinValuesB = bit_clr(PinValuesB, l);

    //shift the number l right to left
    for (int i = 1; i <= 2; i++) {
      PinValuesA = bit_set(PinValuesA, i*10+l);
      loadShiftRegs(); 
      delay(delay_pong);
      PinValuesA = bit_clr(PinValuesA, i*10+l);
      PinValuesB = bit_set(PinValuesB, i*10+l);
      loadShiftRegs();
      delay(delay_pong);
      PinValuesB = bit_clr(PinValuesB, i*10+l); 
    }
  }
  //SHIFT IN CURRENT TIME
  const int delay_finish = 80;
  int display_num = 0;
  for (int l = 0; l <= 4; l++) {
    //first shift in hours, then minutes, then seconds
    switch (l) {
      case 0:
        display_num = timeinfo.tm_hour/10;
        break;
      case 1:
        display_num = timeinfo.tm_hour%10;
        break;
      case 2:
        display_num = timeinfo.tm_min/10;
        break;
      case 3:
        display_num = timeinfo.tm_min%10;
        break;
      case 4:
        display_num = timeinfo.tm_sec/10;
        break;
      default:
        break;
    }
    
    for (int i = 5; i >= l; i--) {
      /*
      This loop index runs from 5 to l, instead of from 2 to 0 as in the previous effect.
      This value is responsible for selecting the correct 10x range in PinValues,
      i.e. selecting the correct pair of nixies.
      */
      int sel_digits = (i/2)*10;
      if (i%2 == 1) {
        PinValuesB = bit_set(PinValuesB, sel_digits+display_num);
        loadShiftRegs();
        delay(delay_finish);
        if (i > l) { //clear the bit except for the last cycle, i.e. keep the shifted digits visible
          PinValuesB = bit_clr(PinValuesB, sel_digits+display_num);
        }
      }
      if (i%2 == 0) {
        PinValuesA = bit_set(PinValuesA, sel_digits+display_num);
        loadShiftRegs();
        delay(delay_finish);
        if (i > l) { //clear the bit except for the last cycle, i.e. keep the shifted digits visible
          PinValuesA = bit_clr(PinValuesA, sel_digits+display_num);
        }
      }
    }
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
    show_date();
    // Check if the button is released
    while (digitalRead(btn) == HIGH) {} 
    prevMillis = millis();
    currentMillis = prevMillis;
    //wait for a double press
    while(currentMillis - prevMillis <= 200) { 
      if (digitalRead(btn) == HIGH) {
        //wait for btn depress
        while(digitalRead(btn) == HIGH) {} 
        stopwatch();
      }
      currentMillis = millis();
    }

    delay(2000);
    if (digitalRead(btn) == HIGH) {
      lightshow();
    }

  }
  else if ((prevSec != timeinfo.tm_sec)) {
    prevSec = timeinfo.tm_sec;
    //if it's midnight, get atomic time
    if ((timeinfo.tm_hour == 1)&&(timeinfo.tm_min == 0)&&(timeinfo.tm_sec == 0)) {
      wifiManager.autoConnect("AutoConnectAP");
      getLocalTime(&timeinfo);
      wifiManager.disconnect();
    }
    //Do a lightshow at midnight and noon
    else if ((timeinfo.tm_hour == 0)&&(timeinfo.tm_min == 0)&&(timeinfo.tm_sec == 0)) {
      lightshow();
    }
    else if ((timeinfo.tm_hour == 12)&&(timeinfo.tm_min == 0)&&(timeinfo.tm_sec == 0)) {
      lightshow();
    }
    //Get tens and units of time
    nixie[0] = timeinfo.tm_sec%10;
    nixie[1] = timeinfo.tm_sec/10;
    nixie[2] = timeinfo.tm_min%10;
    nixie[3] = timeinfo.tm_min/10;
    nixie[4] = timeinfo.tm_hour%10;
    nixie[5] = timeinfo.tm_hour/10;

    loadPinRegs();
  }
}
