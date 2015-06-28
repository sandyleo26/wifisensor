//****************************************************************

// Smart TH WiFi Sensor code

//****************************************************************
// conditional compile
#define CONFIG_UNIXTIME
#define PCB0528
#undef PRODUCTION

#include <ds3231.h>
#include <PowerSaver.h>
#include <SdFat.h>
#include <Wire.h>
#include <EEPROM.h>
#include "HTU21D.h"

// RTC    ******************************
#define VER "final3.ino-170615"
#define SECONDS_DAY 86400
#define CAPTURE_UPLOAD_INT_LEN_MAX 8
#define WIFI_NAME_PASS_LEN_MAX 16
#define WIFI_IPCMD_LEN_MAX 64
#define WIFI_API_LEN_MAX 64
#define WIFI_BUF_MAX 64
#define MAX_LINES_PER_FILE 200
#define MAX_LINES_PER_UPLOAD 4
#define LINE_BUF_SIZE 30*MAX_LINES_PER_UPLOAD
#define RTC_DIFF_FACTOR 8
#define SD_WAIT_FOR_WIFI_DELAY 2000

// ERROR_TYPE
#define SD_INIT_ERROR 1
#define SD_CREATE_NEW_ERROR 2
#define SD_CONFIG_ERROR 3
#define ACK_FAIL_ERROR 4

const char string_0[] PROGMEM = "L%02d%02d%02d%c.csv";   // "String 0" etc are strings to store - change to suit.
const char string_1[] PROGMEM = "";

const char *const string_table[] PROGMEM =       // change "string_table" name to suit
{   
  string_0,
  string_1 };


ISR(PCINT0_vect)  // Setup interrupts on D8; Interrupt (RTC SQW) 
{
  PORTB ^= (1<<PORTB1);
}

// SD card    ******************************
SdFat sd;
SdFile myFile;
char sdLogFile[15] = "";
char sdSysLog[] = "sys.log";
char configFile[] = "config.txt";
char configFileBak[] = "cfg.bak";
#define SDcsPin 9 // D9

// HTU21D    ******************************
HTU21D htu;

// NPN    ******************************
#define WIFI_CP_PD 3 // D3 control ESP8266
#define NPN_Q1 4 // D4 control DHT & SD Card

// User Configuration: SSID,PASS,API,CAPINT,UPINT
uint16_t captureInt = 15, uploadInt = 30;  // in seconds

// Low Power 
PowerSaver chip;  // declare object for PowerSaver class

// Control flag
uint32_t captureCount = 0, uploadCount = 0, uploadedLines = 0;
uint32_t nextCaptureTime, nextUploadTime, alarm;
struct ts t;
boolean wifiConnected = false;
boolean newLogFileNeeded = false;

// LED
#define LED 2

// regulator
#define LDO 5

// wifi
//#define SSID "iPhone"  //change to your WIFI name
//#define PASS "s32nzqaofv9tv"  //wifi password
#define CONCMD1 "AT+CWMODE=1"
#define IPcmd "AT+CIPSTART=\"TCP\",\""
#define getPort "\",80"
#define XXXX "XXXX"

#ifdef PRODUCTION
#define DEBUG_PRINT(str) ;
#else
#define DEBUG_PRINT(str) (Serial.print(str))
#endif
#ifdef PRODUCTION
#define DEBUG_PRINTLN(str) ;
#else
#define DEBUG_PRINTLN(str) (Serial.println(str))
#endif

// setup ****************************************************************
void setup()
{
    initialize();
    //testMemSetup();
    // setup sleep function on the ATmega328p. Power-down mode is used here
    chip.sleepInterruptSetup();

    calculateNextCaptureUploadTime();
}

void initialize()
{
    Serial.begin(57600);
    delay(5);
    DEBUG_PRINTLN(F(VER));
    pinMode(LDO, OUTPUT);
    pinMode(LED, OUTPUT);
    pinMode(NPN_Q1, OUTPUT); // Turn on DHT & SD
    pinMode(WIFI_CP_PD, OUTPUT); // Turn on WiFi 
    pinMode(SDcsPin, OUTPUT); 
    digitalWrite(LDO, HIGH);
    digitalWrite(LED, HIGH);
    digitalWrite(NPN_Q1, HIGH);
    digitalWrite(WIFI_CP_PD, LOW);

    // 1. RTC
    initializeRTC();

    // 2. check SD, initialize SD card on the SPI bus
    int i = 0;
    if (!initializeSD()) {
        deviceFailureShutdown();
        blinkError(SD_INIT_ERROR);
    }

    // 2. check wifi connection

    // HTU21D
    htu.begin();

    // 4. check battery
}

void loop()
{
    //DS3231_get(&t);
    updateRTC();
    if (isCaptureMode()) {
        DEBUG_PRINTLN(F("Capture"));
        digitalWrite(LDO, HIGH);
        digitalWrite(WIFI_CP_PD, LOW);
        digitalWrite(NPN_Q1, HIGH);
        //pinMode(SDcsPin, OUTPUT);
        //captureStoreData();
        //DS3231_get(&t);
        delay(1000);
        while (nextCaptureTime <= t.unixtime + 1) nextCaptureTime += captureInt;
    } else if (isUploadMode()) {
        DEBUG_PRINTLN(F("Upload"));
        digitalWrite(LDO, HIGH);
        digitalWrite(WIFI_CP_PD, LOW);
#ifdef PCB0528
        digitalWrite(NPN_Q1, HIGH);
#else
        digitalWrite(NPN_Q1, LOW);
#endif
        pinMode(SDcsPin, OUTPUT);
        //uploadData();
        delay(5000);
        //DS3231_get(&t);
        while (nextUploadTime <= t.unixtime + 1) nextUploadTime += uploadInt;
    } else if (isSleepMode()) {
        DEBUG_PRINTLN(F("Sleep"));
        setAlarm1();
        goSleep();
    }
}

void setAlarm1()
{
    uint32_t dayclock, wakeupTime;
    uint8_t second, minute, hour;
    wakeupTime = t.unixtime + alarm;
    dayclock = (uint32_t)wakeupTime % SECONDS_DAY;

    second = dayclock % 60;
    minute = (dayclock % 3600) / 60;
    hour = dayclock / 3600;
#ifndef PRODUCTION
    Serial.println(F("setAlarm1"));
    Serial.print(t.hour); Serial.print(":"); Serial.print(t.min); Serial.print(":"); Serial.println(t.sec);
    Serial.print(hour); Serial.print(":"); Serial.print(minute); Serial.print(":"); Serial.println(second);
#endif

    uint8_t flags[5] = { 0, 0, 0, 1, 1};

    // set Alarm1
    DS3231_set_a1(second, minute, hour, 0, flags);

    // activate Alarm1
    DS3231_set_creg(DS3231_INTCN | DS3231_A1IE);
}

void goSleep()
{
#ifndef PRODUCTION
    Serial.println(t.unixtime);
    Serial.println(alarm);
    delay(5);  // give some delay
#endif
    digitalWrite(NPN_Q1, LOW);
    digitalWrite(WIFI_CP_PD, LOW);
    // TODO: find out why LDO cannot be turned off here
    digitalWrite(LDO, LOW);
    DEBUG_PRINTLN(F("LDO is turned Off"));
    delay(5);  // give some delay
    chip.turnOffADC();
    chip.turnOffSPI();
    chip.turnOffWDT();
    chip.turnOffBOD();
    delay(500);
    chip.goodNight();
    // wake up here
    chip.turnOnADC();    // enable ADC after processor wakes up
    chip.turnOnSPI();   // turn on SPI bus once the processor wakes up
    digitalWrite(LDO, HIGH);
    delay(500);    // important delay to ensure SPI bus is properly activated
    DEBUG_PRINTLN(F("LDO is turned on"));
    if (DS3231_triggered_a1()) {
        Serial.println(F("**Alarm has been triggered**"));
        DS3231_clear_a1f();
        delay(500);
    }
}

bool isCaptureMode()
{
  return int32_t(nextCaptureTime - t.unixtime) <= 1;
}

bool isUploadMode()
{
    return int32_t(nextUploadTime - t.unixtime) <= 1;
}

bool isSleepMode()
{
    uint32_t capAlarm, upAlarm;
    capAlarm = nextCaptureTime - t.unixtime;
    upAlarm = nextUploadTime - t.unixtime;
    alarm = (capAlarm<upAlarm) ? capAlarm : upAlarm;
    // sleep&wake needs at most 2 sec so any value below is not worth while
    return alarm>1;
}

void updateRTC()
{
    uint32_t t0 = t.unixtime;
    int i = 0;
    while (i++<20) {
        DS3231_get(&t);
        int32_t diff = int32_t(t.unixtime - t0);
        if (diff >= 0 && diff <= RTC_DIFF_FACTOR*uploadInt) {
            return;
        } else {
            delay(400);
        }
    }
    // if RTC still get invalid time, re-initialize and use this time.
    initializeRTC();
    nextCaptureTime = t.unixtime;
    nextUploadTime = t.unixtime + uploadInt;
}

void initializeRTC()
{
    Wire.begin();
    DS3231_init(DS3231_INTCN);
    delay(1000);
    DS3231_clear_a1f();
    DS3231_get(&t);
}

boolean initializeSD()
{
    int i = 0;
    while (!sd.begin(SDcsPin, SPI_FULL_SPEED)) {
        DEBUG_PRINT(++i); DEBUG_PRINTLN(F(" initialize fail."));
        digitalWrite(LDO, LOW);
        delay(2000);
        digitalWrite(LDO, HIGH);
        delay(3000);
        if (i > 3) return false;
    }
    return true;
}

void deviceFailureShutdown()
{
    delay(100);
    digitalWrite(WIFI_CP_PD, LOW);
    digitalWrite(NPN_Q1, LOW);
//    chip.turnOffADC();
    chip.turnOffSPI();
    chip.turnOffWDT();
    chip.turnOffBOD();
}

void calculateNextCaptureUploadTime()
{
    updateRTC();
#ifdef PRODUCTION
    uint32_t dayclock, tempUnixTime;
    uint8_t second, minute, hour;
    dayclock = (uint32_t)t.unixtime % SECONDS_DAY;
    second = dayclock % 60;
    minute = (dayclock % 3600) / 60;
    hour = dayclock / 3600;
    tempUnixTime = t.unixtime - dayclock + 3600*hour;
    nextCaptureTime = tempUnixTime + (minute/15+1)*900;
    //nextCaptureTime = t.unixtime;
    nextUploadTime = nextCaptureTime + uploadInt;
    //dayclock = (uint32_t)tempCaptureTime % SECONDS_DAY;
#endif
    // roundTime2Quarter for production; otherwise, straightly add interval
    nextCaptureTime = t.unixtime;
    nextUploadTime = t.unixtime + uploadInt;
}
