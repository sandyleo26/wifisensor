//****************************************************************

// Smart TH WiFi Sensor code

//****************************************************************
// conditional compile
#define CONFIG_UNIXTIME
#define PCB0528

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
#define WIFI_BUF_MAX 96
#define MAX_LINES_PER_FILE 200
#define MAX_LINES_PER_UPLOAD 4
#define LINE_BUF_SIZE 30*MAX_LINES_PER_UPLOAD
#define RTC_DIFF_FACTOR 8
#define SD_WAIT_FOR_WIFI_DELAY 2000

// ERROR_TYPE
#define SD_ERROR 1

const char string_0[] PROGMEM = "L%02d%02d%02d%c.csv";   // "String 0" etc are strings to store - change to suit.
const char string_1[] PROGMEM = "";

const char *const string_table[] PROGMEM =       // change "string_table" name to suit
{   
  string_0,
  string_1 };

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
    //readUserSetting();
    readUserSettingEEPROM();
    nextCaptureTime = t.unixtime;
    nextUploadTime = t.unixtime + uploadInt;

    char buffer[WIFI_BUF_MAX];
    getWiFiName(buffer, WIFI_NAME_PASS_LEN_MAX);
    DEBUG_PRINTLN(buffer);
    getWifiPass(buffer, WIFI_NAME_PASS_LEN_MAX);
    DEBUG_PRINTLN(buffer);
    getIP(buffer, WIFI_IPCMD_LEN_MAX);
    DEBUG_PRINTLN(buffer);
    getAPI(buffer, WIFI_API_LEN_MAX);
    DEBUG_PRINTLN(buffer);
    DEBUG_PRINTLN(getCaptureInt());
    DEBUG_PRINTLN(getUploadInt());
    delay(5);
}

void readUserSettingEEPROM()
{
    int i = 0;
    int addr = 0;
    // if (myFile.open(configFile, O_EXCL | O_CREAT)) {
    //     myFile.close();
    //     sd.errorHalt("O_EXCL");
    // }
    if (!myFile.open(configFile, O_READ)) {
        if (!myFile.open(configFileBak, O_READ)) {
            sd.errorHalt(configFileBak);
        }
    }

    while (myFile.available()) {
        i = myFile.read();
        //Serial.write(i);
        EEPROM.write(addr++, i);
        delay(5);
    }
    myFile.close();

    captureInt = getCaptureInt();
    uploadInt = getUploadInt();
}

void initialize()
{
    Serial.begin(9600);
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

    // SD
    if (!initializeSD())
        blinkError(SD_ERROR);
}

void loop()
{
    //DS3231_get(&t);
    digitalWrite(LDO, HIGH);
    digitalWrite(WIFI_CP_PD, HIGH);
    DEBUG_PRINTLN(F("Upload"));
    uploadData();
    digitalWrite(LDO, LOW);
    delay(5000);
}

void uploadData()
{
    uint16_t lineNum = 0, offset = 0, multilines = 0;
    char buffer[LINE_BUF_SIZE];

    if (!initWifiSerial()) return;

    uint32_t start = millis();
    if (connectWiFi()) {
        Serial.println(F("WiFi Connected"));
    } else {
        Serial.println(F("WiFi Failed to Connect."));
    }
    uint32_t end = millis();
    Serial.println(end - start);
}

boolean connectWiFi() {
    uint8_t i = 0;
    uint8_t n = 0;
    uint8_t j = 0;
    uint8_t k = 0;
    char buffer[WIFI_BUF_MAX];

    //Serial.println(F(CONCMD1));
    if (wifiConnected) {
        return findIP();
    }

    cwjap(true);
    delay(10000);
    while (i++<10) {
        delay(1000);
        if (Serial.find("OK")) {
            wifiConnected = true;
            return true;
        }
        /*
        if ((n = Serial.available()) != 0) {
            j = 0;
            k = n < WIFI_BUF_MAX - 1 ? n : WIFI_BUF_MAX -1;
            while (j<k)
                buffer[j++] = Serial.read();
            buffer[k] = '\0';
            if (strstr(buffer, "OK")) {
                break;
            } else if (strstr(buffer, "FAIL")) {
                cwjap(true);
            }
        }
        */
    }
    return false;
}

boolean findIP() {
    uint8_t i = 0;
    uint8_t n = 0;
    uint8_t j = 0;
    uint8_t k = 0;
    char buffer[WIFI_BUF_MAX];
    Serial.println(F("AT+CIFSR"));
    while (i++<10) {
        delay(1000);
        if ((n = Serial.available()) != 0) {
            j = 0;
            k = n < WIFI_BUF_MAX - 1 ? n : WIFI_BUF_MAX -1;
            while (j<k)
                buffer[j++] = Serial.read();
            buffer[k] = '\0';
            if (strstr(buffer, "STAIP")) {
                if (strstr(buffer, "0.0.0.0")) {
                    delay(2000);
                    Serial.println(F("AT+CIFSR"));
                    continue;
                }
                else
                    break;
            }
        }
    }
    return i <=10;
}

void cwjap(boolean real) {
    char buffer[WIFI_NAME_PASS_LEN_MAX];
    Serial.print(F("AT+CWJAP=\""));
    if (real) getWiFiName(buffer, WIFI_NAME_PASS_LEN_MAX);
    Serial.print(buffer); Serial.print(F("\",\""));
    if (real) getWifiPass(buffer, WIFI_NAME_PASS_LEN_MAX);
    Serial.print(buffer); Serial.println(F("\""));
    delay(500);
}

void cwjapxxx() {
    Serial.print(F("AT+CWJAP_CUR=\"")); 
    Serial.print(F("XXXX")); Serial.print(F("\",\"")); 
    Serial.print(F("XXXX")); Serial.println(F("\""));
}

boolean initWifiSerial()
{
    uint8_t i = 0;
    //Serial.println(F("AT+RST"));
    delay(5);
    while (!Serial.find("OK")) {
        if (i++>10) return false;
        Serial.println(F("AT"));
        delay(100);
    }
    return true;
}

void getWiFiName(char* buf, uint8_t len)
{
    char val;
    int i = 0;
    val = EEPROM.read(i++);
    while (val != ',') {
        if (i-1>=len-1) break;
        buf[i-1] = val;
        val = EEPROM.read(i++);
    }
    buf[i-1] = '\0';
}

void getConfigByPos(char *buf, uint8_t pos, uint8_t len)
{
    char val;
    boolean flag = false;
    int count = 0, i = 0, j = 0;
    while (val != '$') {
        val = EEPROM.read(i++);
        if (val == ',') count++;
        if (count == pos-1 && flag == false) {flag = true; continue;} 
        if (count == pos) {break;}
        if (flag) buf[j++] = val;
        if (j>=len-1) break;
    }
    buf[j] = '\0';
}

void getWifiPass(char* buf, uint8_t len)
{
    getConfigByPos(buf, 2, len);
}

void getIP(char* buf, uint8_t len)
{
    getConfigByPos(buf, 3, len);
}

void getAPI(char* buf, uint8_t len)
{
    getConfigByPos(buf, 4, len);
}

uint16_t getCaptureInt()
{
    char buf[CAPTURE_UPLOAD_INT_LEN_MAX];
    getConfigByPos(buf, 5, CAPTURE_UPLOAD_INT_LEN_MAX);
    return atoi(buf);
}

uint16_t getUploadInt()
{
    char buf[CAPTURE_UPLOAD_INT_LEN_MAX];
    getConfigByPos(buf, 6, CAPTURE_UPLOAD_INT_LEN_MAX);
    return atoi(buf);
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

