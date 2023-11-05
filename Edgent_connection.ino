/*************************************************************
  Blynk is a platform with iOS and Android apps to control
  ESP32, Arduino, Raspberry Pi and the likes over the Internet.
  You can easily build mobile and web interfaces for any
  projects by simply dragging and dropping widgets.

    Downloads, docs, tutorials: https://www.blynk.io
    Sketch generator:           https://examples.blynk.cc
    Blynk community:            https://community.blynk.cc
    Follow us:                  https://www.fb.com/blynkapp
                                https://twitter.com/blynk_app

  Blynk library is licensed under MIT license
 *************************************************************
  Blynk.Edgent implements:
  - Blynk.Inject - Dynamic WiFi credentials provisioning
  - Blynk.Air    - Over The Air firmware updates
  - Device state indication using a physical LED
  - Credentials reset using a physical Button
 *************************************************************/

/* Fill in information from your Blynk Template here */
/* Read more: https://bit.ly/BlynkInject */
//#define BLYNK_TEMPLATE_ID           "TMPxxxxxx"
//#define BLYNK_TEMPLATE_NAME         "Device"

#define BLYNK_TEMPLATE_ID "TMPL69VmzcyiJ"
#define BLYNK_TEMPLATE_NAME "energy monitor"

#define BLYNK_FIRMWARE_VERSION        "0.1.0"

#define BLYNK_PRINT Serial
//#define BLYNK_DEBUG

#define APP_DEBUG

// Uncomment your board, or configure a custom board in Settings.h
#define USE_ESP32_DEV_MODULE
//#define USE_ESP32C3_DEV_MODULE
//#define USE_ESP32S2_DEV_KIT
//#define USE_WROVER_BOARD
//#define USE_TTGO_T7
//#define USE_TTGO_T_OI

#include "BlynkEdgent.h"


// #include <LiquidCrystal_I2C.h>
// LiquidCrystal_I2C lcd(0x27, 20, 4);


//#define BLYNK_AUTH_TOKEN "WvMk_aMP1UUcpg9Y9mqR9f5ZcLw0GQYA"
#include "EmonLib.h"
#include <EEPROM.h>
#define BLYNK_PRINT Serial
//#include <WiFi.h>
//#include <WiFiClient.h>
// #include <BlynkSimpleEsp32.h>
#include <TimeLib.h> // Include the Time library
#include <NTPClient.h>
#include <ESP32Time.h>

ESP32Time rtc(19800);  // offset in seconds GMT+1





const unsigned long STARTUP_DELAY = 90000;
bool machineOn = false; // Machine state (OFF by default)
unsigned long dailyOnTime = 0;
unsigned long ontimerlastMillis = 0;

unsigned long sensorStartTime;

int lastSavedDay = 0; // Initialize with a value that will not match the actual date


 
EnergyMonitor emon;
#define vCalibration 83.3
#define currCalibration 0.50
 
BlynkTimer timer;
// char auth[] = "WvMk_aMP1UUcpg9Y9mqR9f5ZcLw0GQYA";
// char ssid[] = "Airtel fiber";
// char pass[] = "Jaishnu@123";
 
float kWh = 0;
unsigned long sensorlastmillis = millis();

const long gmtOffset_sec = 19800; // UTC +5.30 hours offset for India
const int daylightOffset_sec = 0;

char ntpServer[] = "pool.ntp.org";

void myTimerEvent()
{
  emon.calcVI(20, 2000);
  kWh = kWh + emon.apparentPower * (millis() - sensorlastmillis) / 3600000000.0;
  yield();
  Serial.print("Vrms: ");
  Serial.print(emon.Vrms, 2);
  Serial.print("V");
  EEPROM.put(0, emon.Vrms);
  delay(100);
 
  Serial.print("\tIrms: ");
  Serial.print(emon.Irms, 4);
  Serial.print("A");
  EEPROM.put(4, emon.Irms);
  delay(100);
 
  Serial.print("\tPower: ");
  Serial.print(emon.apparentPower, 4);
  Serial.print("W");
  EEPROM.put(8, emon.apparentPower);
  delay(100);
 
  Serial.print("\tkWh: ");
  Serial.print(kWh, 5);
  Serial.println("kWh");
  EEPROM.put(12, kWh);

  unsigned long timecurrentMillis = millis();  // Get the current time

  //  // Check if the initial startup delay has passed
  //   if ((emon.Vrms > 200) && (emon.Vrms > 240 || emon.Vrms < 220) && (emon.Vrms>300)) {
  //     Blynk.logEvent("voltage_alert", "Voltage fluctuates. Take necessary precautions and actions");
  //   }

  if(millis() - sensorStartTime >= STARTUP_DELAY){
    Serial.println("Startup delay end");
    
    // Within startup delay
    if (emon.Vrms >= 220 && emon.Vrms <= 240) {
        // Voltage is between 220 and 240
        Serial.println("Voltage is within the limit");
        // Your code here
    } else if (emon.Vrms < 100) {
        // Voltage is less than 100, so ignore this case
    } else {
        // Voltage is outside the desired range (not between 220 and 240)
        Blynk.logEvent("voltage_alert", "Voltage fluctuation occured");
    }

  }
  bool alertSent = false;

if(millis() - sensorStartTime < STARTUP_DELAY){

  if(!alertSent){
    // Send alert
    alertSent = true;
  }

}
  
  //timer event for on/off time
    unsigned long ontimercurrentMillis = millis();

  if (machineOn) {
    dailyOnTime += (ontimercurrentMillis - ontimerlastMillis) / 1000; // Convert to seconds
      // Check if it's a new day
      if (rtc.getDay() != lastSavedDay) {
          // Reset dai9lyOnTime to 0
          dailyOnTime = 0;
          lastSavedDay = rtc.getDay(); // Update the last saved day
          saveDurationToEEPROM(dailyOnTime);
          saveLastSavedDayToEEPROM(lastSavedDay); // Save lastSavedDay to EEPROM
      } else {
          saveDurationToEEPROM(dailyOnTime);
      }
  }

  // In timerEvent()

Serial.print("Current day: ");
Serial.println(rtc.getDay());


Serial.print("Last saved day: ");
Serial.println(lastSavedDay);

  if (emon.Vrms > 220) {
    machineOn = true;
  } else {
    machineOn = false;
  }

  ontimerlastMillis = ontimercurrentMillis;

    Serial.print("Machine is ");
    if (machineOn) {
      Serial.print("ON");
    } else {
      Serial.print("OFF");
    }

    // Calculate the daily on-time in hours, minutes, and seconds
    unsigned long seconds = dailyOnTime % 60;
    unsigned long minutes = (dailyOnTime / 60) % 60;
    unsigned long hours = dailyOnTime / 3600;

    Serial.print(" | Daily On-Time: ");
    Serial.print(hours);
    Serial.print(" hours, ");
    Serial.print(minutes);
    Serial.print(" minutes, ");
    Serial.print(seconds);
    Serial.println(" seconds");

  
  // lcd.clear();
  // lcd.setCursor(0, 0);
  // lcd.print("Vrms:");
  // lcd.print(emon.Vrms, 2);
  // lcd.print("V");
  // lcd.setCursor(0, 1);
  // lcd.print("Irms:");
  // lcd.print(emon.Irms, 4);
  // lcd.print("A");
  // lcd.setCursor(0, 2);
  // lcd.print("Power:");
  // lcd.print(emon.apparentPower, 4);
  // lcd.print("W");
  // lcd.setCursor(0, 3);
  // lcd.print("kWh:");
  // lcd.print(kWh, 4);
  // lcd.print("W");
 
  sensorlastmillis = millis();

  
  if(millis() - sensorStartTime >= STARTUP_DELAY){
    Blynk.virtualWrite(V0, emon.Vrms);
    Blynk.virtualWrite(V1, emon.Irms);
    Blynk.virtualWrite(V2, emon.apparentPower);
    Blynk.virtualWrite(V3, kWh);
    Blynk.virtualWrite(V4,hours);
    Blynk.virtualWrite(V5,minutes);
  }
}
 
void saveDurationToEEPROM(unsigned long duration) {
  EEPROM.begin(9); // Initialize EEPROM with a size of 4 bytes (adjust as needed)
  EEPROM.write(0, (duration >> 24) & 0xFF);
  EEPROM.write(1, (duration >> 16) & 0xFF);
  EEPROM.write(2, (duration >> 8) & 0xFF);
  EEPROM.write(3, duration & 0xFF);
  EEPROM.commit();
}

unsigned long readDurationFromEEPROM() {
  EEPROM.begin(9); // Initialize EEPROM with a size of 4 bytes (adjust as needed)
  unsigned long duration = 0;
  duration = (EEPROM.read(0) << 24) | (EEPROM.read(1) << 16) | (EEPROM.read(2) << 8) | EEPROM.read(3);
  return duration;
}

// Function to save lastSavedDay to EEPROM
void saveLastSavedDayToEEPROM(int day) {
  EEPROM.begin(9); // Initialize EEPROM with a size of 4 bytes (adjust as needed)
  EEPROM.write(8, day);
  EEPROM.commit();
}

// Function to read lastSavedDay from EEPROM
int readLastSavedDayFromEEPROM() {
  return EEPROM.read(8);
}


void setup()
{
  sensorStartTime = millis();
  Serial.begin(115200);
  //Blynk.begin(auth, ssid, pass);
  BlynkEdgent.begin();
  //lcd.init();
  //lcd.backlight();
  emon.voltage(35, vCalibration, 1.7);  // Voltage: input pin, calibration, phase_shift
  emon.current(34, currCalibration);    // Current: input pin, calibration.
 
  timer.setInterval(5000L, myTimerEvent);
  // setTime(12, 0, 0, 1, 1, 2023);
  // Get time from NTP
  //   configTime(gmtOffset_sec, daylightOffset_sec, ntpServer); 

  //   struct tm timeinfo;
  //   if(!getLocalTime(&timeinfo)) {
  //     // Error getting time
  //     Serial.println("error in ntp");
  //   }

  //   // Extract date
  //   int day = timeinfo.tm_mday;  
  //   int month = timeinfo.tm_mon + 1;
  //   int year = timeinfo.tm_year + 1900;

  //   Serial.print("Day: ");
  //   Serial.println(timeinfo.tm_mday);
  //   Serial.print("Month: ");
  //   Serial.println(timeinfo.tm_mon + 1); // Months are 0-based
  //   Serial.print("Year: ");
  //   Serial.println(timeinfo.tm_year + 1900); // Years since 1900


  //   // Set TimeLib time
  //   setTime(timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, 
  //           day, month, year);

  // // Rest of setup code...

    rtc.setTime(30, 50, 20, 5, 11, 2023);  // 17th Jan 2021 15:24:30


  EEPROM.begin(9); 
  dailyOnTime = readDurationFromEEPROM();

  lastSavedDay = readLastSavedDayFromEEPROM();
  //lcd.setCursor(3, 0);
  //lcd.print("IoT Energy");
  //lcd.setCursor(5, 1);
  //lcd.print("Meter");
  delay(3000);
  //lcd.clear();
  EEPROM.begin(9); // Initialize EEPROM once with proper size
  dailyOnTime = readDurationFromEEPROM();

  lastSavedDay = readLastSavedDayFromEEPROM(); // Initialize lastSavedDay from EEPROM

}
 
void loop()
{
  BlynkEdgent.run();
  timer.run();
  struct tm timeinfo = rtc.getTimeStruct();

    // Check and update NTP time in the loop
  // Your other loop code here...
}
