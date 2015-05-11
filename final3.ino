//****************************************************************

// Smart TH WiFi Sensor code

//****************************************************************
#include <ds3231.h>
#include <PowerSaver.h>
#include <SdFat.h>
#include <Wire.h>
#include <EEPROM.h>
#include "HTU21D.h"

// RTC    ******************************
#define VER "VER: final3.ino"
#define SECONDS_DAY 86400
#define BUFF_MAX 96
#define MAX_LINES_PER_FILE 200
#define MAX_LINES_PER_UPLOAD 5
#define LINE_BUF_SIZE 40*MAX_LINES_PER_UPLOAD
#define WIFI_BUF_MAX 64


ISR(PCINT0_vect)  // Setup interrupts on D8; Interrupt (RTC SQW) 
{
  PORTB ^= (1<<PORTB1);
}

// SD card    ******************************
SdFat sd;
SdFile myFile;
char sdLogFile[15] = "";
char sdSysLog[] = "syslog.txt";
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

// LED
#define LED 2

// regulator
#define LDO 5

// wifi
//#define SSID "iPhone"  //change to your WIFI name
//#define PASS "s32nzqaofv9tv"  //wifi password
#define CONCMD1 "AT+CWMODE=1"
//#define IPcmd "AT+CIPSTART=\"TCP\",\"184.106.153.149\",80" // ThingSpeak
//#define IPcmd "AT+CIPSTART=\"TCP\",\"69.195.124.239\",80" // meetisan
#define IPcmd "AT+CIPSTART=\"TCP\",\""
#define getPort "\",80"
#define XXXX "XXXX"

//String GET = "GET /update?key=8LHRO7Q7L74WVJ07&field1=";


// setup ****************************************************************
void setup()
{
    initialize();
    //readUserSetting();
    readUserSettingEEPROM();
    //echoEEPROM();
    //testMemSetup();
    chip.sleepInterruptSetup();    // setup sleep function on the ATmega328p. Power-down mode is used here
    nextCaptureTime = t.unixtime;
    nextUploadTime = t.unixtime + uploadInt;
    char buffer[32];
    getWiFiName(buffer);
    Serial.println(buffer);
    getWifiPass(buffer);
    Serial.println(buffer);
    getIP(buffer);
    Serial.println(buffer);
    getAPI(buffer);
    Serial.println(buffer);
    Serial.println(getCaptureInt());
    Serial.println(getUploadInt());
    delay(5);
}

void readUserSettingEEPROM()
{
    int i = 0;
    int addr = 0;
    // if (!sd.begin(SDcsPin, SPI_FULL_SPEED)) sd.initErrorHalt();
    if (myFile.open(configFile, O_EXCL | O_CREAT)) {
        myFile.close();
        sd.errorHalt("O_EXCL");
    }
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
    Serial.println(F(VER));
    pinMode(LDO, OUTPUT);
    //pinMode(LED, OUTPUT);
    pinMode(NPN_Q1, OUTPUT); // Turn on DHT & SD
    pinMode(WIFI_CP_PD, OUTPUT); // Turn on WiFi 
    pinMode(SDcsPin, OUTPUT); 
    digitalWrite(LDO, HIGH);
    //digitalWrite(LED, HIGH);
    digitalWrite(NPN_Q1, HIGH);
    digitalWrite(WIFI_CP_PD, HIGH);

    // 1. RTC
    Wire.begin();
    DS3231_init(DS3231_INTCN);
    DS3231_clear_a1f();
    DS3231_get(&t);

    // 2. check SD
    // initialize SD card on the SPI bus
    //todo: get the line number to ignore
    int i = 0;
    while (!sd.begin(SDcsPin, SPI_FULL_SPEED)) {
        delay(1000);
        Serial.print(++i); Serial.println(F(" initialize fail."));
        // sysLog(F("initialize."));
        if (i > 10) sd.initErrorHalt();
    }
    createNewLogFile();
    
    // 2. check wifi connection

    // HTU21D
    htu.begin();

    // 4. check battery
    // 5. check USB connection
}

void loop()
{
    DS3231_get(&t);

    if (isCaptureMode()) {
        Serial.println(F("Capture"));
        digitalWrite(LDO, HIGH);
        digitalWrite(WIFI_CP_PD, LOW);
        digitalWrite(NPN_Q1, HIGH);
        pinMode(SDcsPin, OUTPUT);
        captureStoreData();
        captureCount++;
        DS3231_get(&t);
        while (nextCaptureTime <= t.unixtime + 1) nextCaptureTime += captureInt;
    } else if (isUploadMode()) {
        Serial.println(F("Upload"));
        digitalWrite(LDO, HIGH);
        digitalWrite(WIFI_CP_PD, HIGH);
        digitalWrite(NPN_Q1, HIGH);
        pinMode(SDcsPin, OUTPUT);
        uploadData();
        uploadCount++;
        DS3231_get(&t);
        while (nextUploadTime <= t.unixtime + 1) nextUploadTime += uploadInt;
    } else if (isSleepMode()) {
        Serial.println(F("Sleep"));
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
    // Serial.println(F("setAlarm1"));
    // Serial.print(t.hour); Serial.print(":"); Serial.print(t.min); Serial.print(":"); Serial.println(t.sec);
    // Serial.print(hour); Serial.print(":"); Serial.print(minute); Serial.print(":"); Serial.println(second);

    uint8_t flags[5] = { 0, 0, 0, 1, 1};

    // set Alarm1
    DS3231_set_a1(second, minute, hour, 0, flags);

    // activate Alarm1
    DS3231_set_creg(DS3231_INTCN | DS3231_A1IE);
}

void goSleep()
{
    digitalWrite(NPN_Q1, LOW);
    digitalWrite(WIFI_CP_PD, LOW);
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
        if (!createNewLogFile(true))
            sd.errorHalt("NewLog");
    uploadedLines = 0;
}

boolean createNewLogFile(boolean overwrite)
{
    char c = 'a';
    char fmt[] = "L%02d%02d%02d%c.csv";
    uint8_t i = 0;

    DS3231_get(&t);
    while(i++<26) {
        sprintf(sdLogFile, fmt, t.year-2000, t.mon, t.mday, c++);
        //Serial.print(F("log: ")); Serial.println(sdLogFile);
        ifstream f(sdLogFile);
        if (f.good()) {
            f.close();
            if (!overwrite) continue;
        }
        if (!myFile.open(sdLogFile, O_WRITE | O_CREAT | O_TRUNC)) {
            Serial.print(F("Failed to open")); Serial.println(sdLogFile);
            myFile.close();
        } else {
            Serial.println(F("sd works!"));
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
        
    delay(5);
    if (!sd.begin(SDcsPin, SPI_FULL_SPEED)) sd.initErrorHalt();

    if (!myFile.open(sdLogFile, O_RDWR | O_CREAT | O_AT_END)) {
        createNewLogFile();
        if (!myFile.open(sdLogFile, O_RDWR | O_CREAT | O_AT_END)) {
            sd.errorHalt("sd!");
        }
    }

    myFile.print(captureCount);myFile.print(F(","));
    myFile.print(t.year); myFile.print(F("-"));myFile.print(t.mon);myFile.print(F("-"));myFile.print(t.mday);myFile.print(F(","));
    myFile.print(t.hour); myFile.print(F(":")); myFile.print(t.min); myFile.print(F(":")); myFile.print(t.sec); myFile.print(F(","));
    myFile.print(ttmp); myFile.print(F(",")); myFile.print(htmp); myFile.println(F("$"));

    myFile.close();
    //captureBlink();    
}

void uploadData()
{
    uint16_t lineNum = 0, offset = 0, multilines = 0;
    char buffer[LINE_BUF_SIZE];
    if (!initWifiSerial()) return;

    //Serial.println(F("connectWiFi"));fi
    if (connectWiFi()) {
        // initialize the SD card at SPI_HALF_SPEED to avoid bus errors with breadboards. use SPI_FULL_SPEED for better performance.
        if (!sd.begin(SDcsPin, SPI_FULL_SPEED)) sd.initErrorHalt();
        ifstream sdin(sdLogFile);         

        while (sdin.getline(buffer+offset, LINE_BUF_SIZE-offset, '\n') || sdin.gcount()) {
            if (++lineNum<=uploadedLines) continue;
            if (sdin.fail()) {
              //Serial.println(F("Partial long line"));
              sdin.clear(sdin.rdstate() & ~ios_base::failbit);
              buffer[LINE_BUF_SIZE-1] = '\0';
              buffer[LINE_BUF_SIZE-2] = '$';
            }

            multilines++;
            offset = strlen(buffer);

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
            createNewLogFile();
        }
    }
}

boolean connectWiFi() {
    uint8_t i = 0;
    uint8_t n = 0;
    uint8_t j = 0;
    uint8_t k = 0;
    char buffer[WIFI_BUF_MAX];

    Serial.println(F(CONCMD1));
    cwjap(true);
    while (1) {
        if (i++>20) return false;
        delay(1000);
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
            } else if (strstr(buffer, "ready")) {
                cwjap(false);
                delay(2000);
                cwjap(true);
            }
        }
    }
    return true;
}

void cwjap(boolean real) {
    char buffer[16];
    Serial.print(F("AT+CWJAP=\""));
    if (real) getWiFiName(buffer);
    Serial.print(buffer); Serial.print(F("\",\""));
    if (real) getWifiPass(buffer);
    Serial.print(buffer); Serial.println(F("\""));
}

boolean transmitData(char* data, uint16_t lines) {  

    char cmd[40];
    int length;
    uint8_t i = 0;

    getAPI(cmd);
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
    //Serial.println(F("AT+CIPCLOSE"));
    uploadedLines += lines;
    i = 0;
    while (!Serial.find("SEND OK")) {
        if (i++>20) break;
        delay(200);
    }
    //uploadBlink();
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
    char buffer[WIFI_BUF_MAX];
    Serial.print(F(IPcmd));
    getIP(buffer);
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
    Serial.find("ERROR");
    cipstart();
    delay(2000);
    while (1) {
        if (i++>20) return false;
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
    Serial.println(F("AT+CIPMODE=0"));
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

String getWiFiName()
{
    char val;
    int i = 0;
    String str;
    val = EEPROM.read(i++);
    while (val != ',') {
        str += val;
        val = EEPROM.read(i++);
    }
    return str;
}

void getWiFiName(char* buf)
{
    char val;
    int i = 0;
    val = EEPROM.read(i++);
    while (val != ',') {
        buf[i-1] = val;
        val = EEPROM.read(i++);
    }
    buf[i-1] = '\0';
}

void getConfigByPos(char *buf, uint8_t pos)
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
    }
    buf[j] = '\0';
}

void getWifiPass(char* buf)
{
    getConfigByPos(buf, 2);
}

void getIP(char* buf)
{
    getConfigByPos(buf, 3);
}

void getAPI(char* buf)
{
    getConfigByPos(buf, 4);
}

uint16_t getCaptureInt()
{
    char buf[8];
    getConfigByPos(buf, 5);
    return atoi(buf);
}

uint16_t getUploadInt()
{
    char buf[8];
    getConfigByPos(buf, 6);
    return atoi(buf);
}

void testWiFi()
{
    String data, stime;
    char raw[64] = "1,2015-02-27,01:44:33,38.5,66.6$";  
    
    Serial.println(F("testWiFi"));
    if (!initWifiSerial()) return;

    //Serial.println(F("connectWiFi"));
    if (connectWiFi()) {
        transmitData(raw, 1);
    }
}

void sysLog(const __FlashStringHelper* msg)
{
    // if (!sd.begin(SDcsPin, SPI_FULL_SPEED)) {
    //     Serial.println(F("initErrorHalt"));
    //     return;
    // }
    if (!myFile.open(sdSysLog, O_RDWR | O_CREAT | O_AT_END)) {
        myFile.print(t.year); myFile.print(t.mon); myFile.print(t.mday);myFile.print(F(" "));
        myFile.print(t.hour); myFile.print(F(":")); myFile.print(t.min); myFile.print(F(":")); myFile.print(t.sec);
        myFile.print(F("  info:")); myFile.println(msg);
    }
    myFile.close();
}

void sysLog(const char* msg)
{
    // if (!sd.begin(SDcsPin, SPI_FULL_SPEED)) {
    //     Serial.println(F("initErrorHalt"));
    //     return;
    // }
    if (!myFile.open(sdSysLog, O_RDWR | O_CREAT | O_AT_END)) {
        myFile.print(t.year); myFile.print(t.mon); myFile.print(t.mday);myFile.print(F(" "));
        myFile.print(t.hour); myFile.print(F(":")); myFile.print(t.min); myFile.print(F(":")); myFile.print(t.sec);
        myFile.print(F("  info:")); myFile.println(msg);
    }
    myFile.close();
}

int freeRam () {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}

void testMemSetup () {
    //Serial.begin(57600);
    Serial.println(F("\n[memCheck]"));
    Serial.println(freeRam());
}

void captureBlink() {
    for (int i = 0; i < 1; i++) {
        digitalWrite(LED, LOW);   // turn the LED on (HIGH is the voltage level)
        delay(5);              // wait for a second
        digitalWrite(LED, HIGH);    // turn the LED off by making the voltage LOW
        delay(100);             
    }
}

void uploadBlink() {
    for (int i = 0; i < 3; i++) {
        digitalWrite(LED, LOW);   // turn the LED on (HIGH is the voltage level)
        delay(5);              // wait for a second
        digitalWrite(LED, HIGH);    // turn the LED off by making the voltage LOW
        delay(100);             
    }
}

void blink5()
{
    for (int i = 0; i < 3; i++) {
        digitalWrite(LED, LOW);   // turn the LED on (HIGH is the voltage level)
        delay(10);              // wait for a second
        digitalWrite(LED, HIGH);    // turn the LED off by making the voltage LOW
        delay(400);             
    }
}

void debugPrintTime()
{
    // display current time
    DS3231_get(&t);
    char buff[BUFF_MAX];
    snprintf(buff, BUFF_MAX, "%02d:%02d:%02d,cap:%d,up:%d,",t.hour, t.min, t.sec, captureCount, uploadCount);
    Serial.println(buff);
    // String str;
    // str += String(t.year) + "-" + String(t.mon) + "-" + String(t.mday) + "," + 
    //     String(t.hour) + ":" + String(t.min) + ":" + String(t.sec) + ",cap:" + String(captureCount) + ",up:" + String(uploadCount);
    // Serial.println(str);
}

