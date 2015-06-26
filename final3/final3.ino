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
#define SD_ERROR 1

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
    //readUserSetting();
    readUserSettingEEPROM();
    //testMemSetup();
    chip.sleepInterruptSetup();    // setup sleep function on the ATmega328p. Power-down mode is used here
    nextCaptureTime = t.unixtime;
    nextUploadTime = t.unixtime + uploadInt;

    roundTime2Quarter();
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
    if (!initializeSD())
        blinkError(SD_ERROR);
    SdFile::dateTimeCallback(dateTime);
    createNewLogFile();

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
        captureStoreData();
        captureCount++;
        //DS3231_get(&t);
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
        uploadData();
        uploadCount++;
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

void roundTime2Quarter()
{
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
}

void goSleep()
{
#ifndef PRODUCTION
    Serial.println(t.unixtime);
    Serial.println(alarm);
    Serial.println(nextCaptureTime);
    Serial.println(nextUploadTime);
    delay(5);  // give some delay
#endif
    digitalWrite(NPN_Q1, LOW);
    digitalWrite(WIFI_CP_PD, LOW);
    //digitalWrite(LDO, LOW);
    delay(5);  // give some delay
    chip.turnOffADC();
    chip.turnOffSPI();
    chip.turnOffWDT();
    chip.turnOffBOD();
    chip.goodNight();
    // wake up here
    chip.turnOnADC();    // enable ADC after processor wakes up
    chip.turnOnSPI();   // turn on SPI bus once the processor wakes up
    delay(500);    // important delay to ensure SPI bus is properly activated
    //if (DS3231_triggered_a1()) {
        //Serial.println(F("**Alarm has been triggered**"));
        DS3231_clear_a1f();
        delay(10);
    //}
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

void createNewLogFile()
{
    if (!createNewLogFile(false))
        if (!createNewLogFile(true)) {
            //sd.errorHalt("NewLog");
            digitalWrite(LDO, LOW);
            chip.turnOffADC();
            chip.turnOffSPI();
            chip.turnOffWDT();
            chip.turnOffBOD();
            blinkError(3);
        }
    uploadedLines = 0;
}

boolean createNewLogFile(boolean overwrite)
{
    char c = 'a';
    char fmt[24];
    uint8_t i = 0;

    strcpy_P(fmt, (char*)pgm_read_word(&(string_table[0])));
    //DS3231_get(&t);
    while(i++<26) {
        sprintf(sdLogFile, fmt, t.year-2000, t.mon, t.mday, c++);
        /*
        ifstream f(sdLogFile);
        if (f.good()) {
            f.close();
            Serial.print(sdLogFile); Serial.println(F(" exists."));
            if (!overwrite) continue;
        }
        */
        if (sd.exists(sdLogFile)) {
            Serial.print(sdLogFile); Serial.println(F(" exists."));
            if (!overwrite) continue;
        }
        if (!myFile.open(sdLogFile, O_WRITE | O_CREAT | O_TRUNC)) {
            DEBUG_PRINT(F("Failed to open ")); DEBUG_PRINTLN(sdLogFile);
            myFile.close();
        } else {
            DEBUG_PRINT(F("New "));
            DEBUG_PRINTLN(sdLogFile);
            myFile.close();
            return true;
        }
    }
    return false;
}

void captureStoreData()
{
    //char raw[64] = "9999999,2015-02-27,01:44:33,-38.5,66.6$";
    char ttmp[8]; 
    char htmp[8];
    float temp, hum;
    temp = htu.readTemperature();
    hum = htu.readHumidity();
    dtostrf(temp, 3, 1, ttmp);
    dtostrf(hum, 3, 1, htmp);
        
    delay(2000);
    if (!initializeSD()) return;

    delay(5);
    if (newLogFileNeeded) {
        createNewLogFile();
        newLogFileNeeded = true;
    }
    if (!myFile.open(sdLogFile, O_RDWR | O_CREAT | O_AT_END)) {
        createNewLogFile();
        if (!myFile.open(sdLogFile, O_RDWR | O_CREAT | O_AT_END)) {
            sd.errorHalt("sd!");
        }
    }

/*
    myFile.print(captureCount);myFile.print(F(","));
    myFile.print(t.year); myFile.print(F("-"));myFile.print(t.mon);myFile.print(F("-"));myFile.print(t.mday);myFile.print(F(","));
    myFile.print(t.hour); myFile.print(F(":")); myFile.print(t.min); myFile.print(F(":")); myFile.print(t.sec); myFile.print(F(","));
    myFile.print(ttmp); myFile.print(F(",")); myFile.print(htmp); myFile.println(F("$"));
    */
    myFile.print(t.year-2000);
    if (t.mon<10) myFile.print(0); myFile.print(t.mon);
    if (t.mday<10) myFile.print(0); myFile.print(t.mday);
    if (t.hour<10) myFile.print(0); myFile.print(t.hour);
    if (t.min<10) myFile.print(0); myFile.print(t.min);
    if (t.sec<10) myFile.print(0); myFile.print(t.sec); myFile.print(F(",")); 
    myFile.print(ttmp); myFile.print(F(",")); myFile.print(htmp); myFile.println(F("$"));
    myFile.close();
    //captureBlink();    
}


void uploadData()
{
    uint16_t lineNum = 0, offset = 0, multilines = 0;
    char buffer[LINE_BUF_SIZE];

    if (!initializeSD()) return;

    digitalWrite(WIFI_CP_PD, HIGH);
    if (!initWifiSerial()) return;

    if (connectWiFi()) {
        // Important: print nothing before TCP connecton. Otherwise, it might fail
        //DEBUG_PRINTLN(F("WiFi Connected"));
        if (!initWifiSerial()) return;
        ifstream sdin(sdLogFile);
        if (sdin.good()) {
            DEBUG_PRINTLN(F("I'm good"));
        } else {
            DEBUG_PRINTLN(F("I'm bad"));
        }
        if (!initWifiSerial()) return;
        while (sdin.getline(buffer+offset, LINE_BUF_SIZE-offset-1, '\n') || sdin.gcount()) {
            //DEBUG_PRINTLN(F("Reading SD..."));
            if (++lineNum<=uploadedLines) continue;
            if (sdin.fail()) {
              //Serial.println(F("Partial long line"));
              sdin.clear(sdin.rdstate() & ~ios_base::failbit);
              buffer[LINE_BUF_SIZE-1] = '\0';
              buffer[LINE_BUF_SIZE-2] = '$';
            }

            multilines++;
            offset = strlen(buffer);

            /*
            DEBUG_PRINTLN(lineNum);
            DEBUG_PRINTLN(offset);
            DEBUG_PRINTLN(multilines);
            DEBUG_PRINTLN(buffer);
            */

            // buffer: "133,2015-02-27,01:44:33,25.6,66.6$";
            if (multilines == MAX_LINES_PER_UPLOAD) {
                if (!transmitData(buffer, multilines))
                    return;
                multilines = 0;
                offset = 0;
            }
        }
        if (multilines!=MAX_LINES_PER_UPLOAD && uploadedLines+multilines>=MAX_LINES_PER_FILE) {
            if (multilines>0) {
                if (!transmitData(buffer, multilines))
                    return;
            }
            //delay(SD_WAIT_FOR_WIFI_DELAY);
            //createNewLogFile();
            newLogFileNeeded = true;
        }
    }
}

boolean connectWiFi() {
    uint8_t i = 0;
    uint8_t n = 0;
    uint8_t j = 0;
    uint8_t k = 0;
    char buffer[WIFI_BUF_MAX];

    //Serial.println(F(CONCMD1));
    if (wifiConnected) return true;

    cwjap(true);
    delay(10000);
    while (i++<10) {
        delay(1000);
        if (Serial.find("OK")) return true;
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

boolean transmitData(char* data, uint16_t lines) {  

    char cmd[WIFI_API_LEN_MAX];
    int length;
    uint8_t i = 0;

    //DEBUG_PRINTLN(F("transmitData"));
    getAPI(cmd, WIFI_API_LEN_MAX);
    length = strlen(cmd) + strlen(data) + 2;

    if (!initDataSend(length)) return false;

    while (!Serial.find(">")) {
        if (i++>20) return false;
        delay(200);
        // Serial.println(F("AT+CIPCLOSE"));
        // if (!initWifiSerial()) return false;

        // if (connectWiFi()) {
        //     if (!initDataSend(length)) return false;
        // }
    }
    Serial.print(cmd); Serial.print(data); Serial.print(F("\r\n"));
    wifiConnected = true;
    //Serial.println(F("AT+CIPCLOSE"));
    uploadedLines += lines;
    // This delay is necessary sometimes for uploading to complete
    delay(500);
    return true;
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

void cipstart() {
    char buffer[WIFI_IPCMD_LEN_MAX];
    Serial.print(F(IPcmd));
    getIP(buffer, WIFI_IPCMD_LEN_MAX);
    Serial.print(buffer);
    Serial.println(F(getPort));
}

boolean initDataSend(int length)
{
    uint8_t i = 0;
    uint8_t n = 0;
    uint8_t j = 0;
    uint8_t k = 0;
    char buffer[WIFI_BUF_MAX];
    //Serial.find("ERROR");
    cipstart();
    delay(2000);
    while (1) {
        // Don't use find, because it's likely ERROR is returned. So detect it.
        /*
        if (Serial.find("CONNECT"))
            break;
            */
        if (i++>10) return false;
        delay(1000);
        if ((n = Serial.available()) != 0) {
            j = 0;
            k = n < WIFI_BUF_MAX - 1 ? n : WIFI_BUF_MAX -1;
            while (j<k)
                buffer[j++] = Serial.read();
            buffer[k] = '\0';
            if (strstr(buffer, "CONNECT")) {
                break;
            } else if (strstr(buffer, "ERROR")) {
                cipstart();
            }
        }
    }
    //Serial.println(F("AT+CIPMODE=0"));
    delay(100);
    Serial.print(F("AT+CIPSEND="));
    Serial.println(length);
    delay(5);
    return true;
}

String getAllEEPROM()
{ 
    char val;
    int i = 0;
    String str;
    val = EEPROM.read(i++);
    while (val != '$') {
        str += val;
        val = EEPROM.read(i++);
    }
    return str;
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
