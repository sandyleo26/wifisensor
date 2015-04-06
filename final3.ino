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
#define SECONDS_DAY 86400
#define BUFF_MAX 96
#define LINE_BUF_SIZE 160

ISR(PCINT0_vect)  // Setup interrupts on D8; Interrupt (RTC SQW) 
{
  PORTB ^= (1<<PORTB1);
}

// SD card    ******************************
SdFat sd;
SdFile myFile;
char sdLogFile[] = "sdlog.csv";
char configFile[] = "index.txt";
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
#define IPcmd "AT+CIPSTART=\"TCP\",\"184.106.153.149\",80" // ThingSpeak IP Address: 184.106.153.149
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
    Serial.println(getWiFiName());
    Serial.println(getWifiPass());
    Serial.println(getAPI());
    Serial.println(getCaptureInt());
    Serial.println(getUploadInt());
    delay(5);
}

void echoEEPROM() {
  for (int address = 0; address < 100; address++) {
      int value = EEPROM.read(address);
      Serial.print(address);
      Serial.print("\t");
      Serial.print(value, DEC);
      Serial.println();
      delay(10);
  }
  Serial.println(getAllEEPROM());
  Serial.println(getWiFiName());
  Serial.println(getWifiPass());
  Serial.println(getAPI());
}

void readUserSetting()
{
    const int line_buffer_size = 100;
    char buffer[line_buffer_size];
    int line_number = 0;
    int num = 0;
    ifstream sdin(configFile);

    if (!myFile.open(configFile, O_READ)) {
        sd.errorHalt("sd!");
    }

    while (sdin.getline(buffer, line_buffer_size, ',') || sdin.gcount()) {
        num = num + 1;
        if (num == 1){
          //wifiName = (String) buffer;
            //strncpy(wifiName, buffer, 16);
        }else if (num == 2){
          //wifiPassword = (String) buffer;
            //strncpy(wifiPass, buffer, 16);
        }else if (num == 3){
          //API = (String) buffer;
          //strncpy(API, buffer, 40);
        }else if (num == 4){
            captureInt = atof(buffer);
        }else if (num == 5){
            uploadInt = atof(buffer);
        }   
    }
    myFile.close();
}

void readUserSettingEEPROM()
{
    int i = 0;
    int addr = 0;
    if (!sd.begin(SDcsPin, SPI_HALF_SPEED)) sd.initErrorHalt();
    if (!myFile.open(configFile, O_READ)) {
        sd.errorHalt("index.txt!");
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
    delay(1000);
    pinMode(LDO, OUTPUT);
    pinMode(LED, OUTPUT);
    pinMode(NPN_Q1, OUTPUT); // Turn on DHT & SD
    pinMode(WIFI_CP_PD, OUTPUT); // Turn on WiFi  
    digitalWrite(LDO, HIGH);
    digitalWrite(LED, HIGH);
    digitalWrite(NPN_Q1, HIGH);
    digitalWrite(WIFI_CP_PD, HIGH);

    // 1. check SD
    // initialize SD card on the SPI bus
    //todo: get the line number to ignore
    if (!sd.begin(SDcsPin, SPI_HALF_SPEED)) sd.initErrorHalt();
    if (!myFile.open(sdLogFile, O_WRITE | O_CREAT | O_TRUNC)) {
        sd.errorHalt("sdlog.csv!");
    }
    myFile.close();
    
    // 2. check wifi connection

    // RTC
    Wire.begin();
    DS3231_init(DS3231_INTCN);
    DS3231_clear_a1f();
    DS3231_get(&t);

    // HTU21D
    htu.begin();

    // 4. check battery
    // 5. check USB connection
}

void loop()
{
    DS3231_get(&t);
    // testCaptureData();
    // testMemSetup();
    // testUploadData();
    // testWiFi();
    if (isCaptureMode()) {
        Serial.println(F("CaptureMode"));
        digitalWrite(NPN_Q1, HIGH);
        captureStoreData();
        captureCount++;
        DS3231_get(&t);
        while (nextCaptureTime < t.unixtime) nextCaptureTime += captureInt;
    } else if (isUploadMode()) {
        Serial.println(F("UploadMode"));
        digitalWrite(WIFI_CP_PD, HIGH);
        uploadData();
        uploadCount++;
        DS3231_get(&t);
        while (nextUploadTime < t.unixtime) nextUploadTime += uploadInt;
    } else if (isSleepMode()) {
        Serial.println(F("SleepMode"));
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

    // flags define what calendar component to be checked against the current time in order
    // to trigger the alarm - see datasheet
    // A1M1 (seconds) (0 to enable, 1 to disable)
    // A1M2 (minutes) (0 to enable, 1 to disable)
    // A1M3 (hour)    (0 to enable, 1 to disable) 
    // A1M4 (day)     (0 to enable, 1 to disable)
    // DY/DT          (dayofweek == 1/dayofmonth == 0)
    uint8_t flags[5] = { 0, 0, 0, 1, 1};

    // set Alarm1
    DS3231_set_a1(second, minute, hour, 0, flags);
    // Serial.print(F("Hour: "));Serial.println(hour);
    // Serial.print(F("Min: "));Serial.println(minute);
    // Serial.print(F("Second: "));Serial.println(second);

    // activate Alarm1
    DS3231_set_creg(DS3231_INTCN | DS3231_A1IE);
}

void goSleep()
{
    //Serial.println(F("goSleep"));
    digitalWrite(NPN_Q1, LOW);
    digitalWrite(WIFI_CP_PD, LOW);
    delay(5);  // give some delay
    chip.turnOffADC();
    chip.turnOffSPI();
    chip.turnOffWDT();
    chip.turnOffBOD();
    chip.goodNight();
    if (DS3231_triggered_a1()) {
        delay(5);
        //debugPrintTime();
        //Serial.println(F("**Alarm has been triggered**"));
        DS3231_clear_a1f();
    }
    //chip.turnOnADC();    // enable ADC after processor wakes up
    chip.turnOnSPI();   // turn on SPI bus once the processor wakes up
    delay(5);    // important delay to ensure SPI bus is properly activated
}

bool isCaptureMode()
{
  return int32_t(nextCaptureTime - t.unixtime) < 1;
}

bool isUploadMode()
{
    return int32_t(nextUploadTime - t.unixtime) < 1;
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

void testCaptureData()
{
    //char raw[64] = "1,2015-02-27,01:44:33,38.5,66.6$";
    char ttmp[8]; 
    char htmp[8];
    float temp, hum;
    String str;
    delay(5000);
//    dtostrf(27.5, 3, 1, buff);
//    Serial.println(buff);
//    Serial.println(sizeof(buff));
//    dtostrf(-3.0, 3, 1, buff);
//    Serial.println(buff);
//    Serial.println(sizeof(buff));
//    Serial.println(F("testCaptureData"));

    temp = htu.readTemperature();
    hum = htu.readHumidity();
    captureCount++;
    dtostrf(temp, 3, 1, ttmp);
    dtostrf(hum, 3, 1, htmp);
    str += String(captureCount) + "," + String(t.year) + "-" + String(t.mon) + "-" + String(t.mday) + "," + 
        String(t.hour) + ":" + String(t.min) + ":" + String(t.sec) + "," + String(ttmp) + "," + String(htmp) + String("$");
        
    delay(1000);
    if (!sd.begin(SDcsPin, SPI_HALF_SPEED)) sd.initErrorHalt();
    if (!myFile.open(sdLogFile, O_RDWR | O_CREAT | O_AT_END)) {
        sd.errorHalt("sd!");
    }
    //myFile.println("testing 1, 2, 3.");
    myFile.println(str);
    // close the file:
    myFile.close();
}

void captureStoreData()
{
    //char raw[64] = "9999999,2015-02-27,01:44:33,-38.5,66.6$";
    char ttmp[8]; 
    char htmp[8];
    String str;
    float temp, hum;
    temp = htu.readTemperature();
    hum = htu.readHumidity();
    dtostrf(temp, 3, 1, ttmp);
    dtostrf(hum, 3, 1, htmp);
        
    delay(1000);
    if (!sd.begin(SDcsPin, SPI_HALF_SPEED)) sd.initErrorHalt();
    if (!myFile.open(sdLogFile, O_RDWR | O_CREAT | O_AT_END)) {
        sd.errorHalt("sd!");
    }

    myFile.print(captureCount);myFile.print(F(","));
    myFile.print(t.year); myFile.print(F("-"));myFile.print(t.mon);myFile.print(F("-"));myFile.print(t.mday);myFile.print(F(","));
    myFile.print(t.hour); myFile.print(F(":")); myFile.print(t.min); myFile.print(F(":")); myFile.print(t.sec); myFile.print(F(","));
    myFile.print(ttmp); myFile.print(F(",")); myFile.print(htmp); myFile.println(F("$"));

    myFile.close();
    captureBlink();    
}

void uploadData()
{
    uint16_t lineNum = 0, offset = 0, multilines = 0;
    char buffer[LINE_BUF_SIZE];
    if (!initWifiSerial()) return;

    //Serial.println(F("connectWiFi"));
    if (connectWiFi()) {
        // initialize the SD card at SPI_HALF_SPEED to avoid bus errors with breadboards. use SPI_FULL_SPEED for better performance.
        if (!sd.begin(SDcsPin, SPI_HALF_SPEED)) sd.initErrorHalt();
        ifstream sdin(sdLogFile);         

        while (sdin.getline(buffer+offset, LINE_BUF_SIZE-offset, '\n') || sdin.gcount()) {
          //int count = sdin.gcount();
          if (sdin.fail()) {
            //cout << "Partial long line";
            //Serial.println(F("Partial long line"));
            sdin.clear(sdin.rdstate() & ~ios_base::failbit);
          } else if (sdin.eof()) {
            //cout << "Partial final line";  // sdin.fail() is false
            //Serial.println(F("Partial final line"));
            continue;
          } else {
            lineNum++;
          }
          if (lineNum<=uploadedLines) continue;

          multilines++;
          offset = strlen(buffer);

          // buffer: "133,2015-02-27,01:44:33,25.6,66.6$";
          if (multilines == 4) {
            if (!transmitData(buffer, multilines))
                return;
            multilines = 0;
            offset = 0;
          }
        }
    }
}

boolean connectWiFi(){
    uint8_t i = 0;
    String wifiName, wifiPass;
    wifiName = getWiFiName();
    wifiPass = getWifiPass();
    Serial.println(F(CONCMD1));
    Serial.print(F("AT+CWJAP=\"")); Serial.print(wifiName); Serial.print(F("\",\"")); Serial.print(wifiPass); Serial.println(F("\""));
    //Serial.println(String(F("AT+CWJAP=\"")) + String(wifiName) + String(F("\",\"")) + String(wifiPass) + String(F("\"")));
    //testMemSetup();
    while (!Serial.find("OK")) {
        if (i++>15) return false;
    }
    return true;
}

boolean transmitData(char* data, uint16_t lines) {  

    String cmd;
    int length;
    uint8_t i = 0;

    cmd = getAPI();
    length = cmd.length() + strlen(data) + 2;

    if (!initDataSend(length)) return false;
    while (!Serial.find(">")) {
        if (i++>30) return false;
        Serial.println(F("AT+CIPCLOSE"));
        if (!initWifiSerial()) return false;

        if (connectWiFi()) {
            if (!initDataSend(length)) return false;
        }
    }
    Serial.print(cmd); Serial.print(data); Serial.print(F("\r\n"));
    //Serial.println(F("AT+CIPCLOSE"));
    uploadedLines += lines;
    i = 0;
    while (!Serial.find("SEND OK"))
        if (i++>50) break;
    delay(1000);
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
        delay(5);
    }
    return true;
}

boolean initDataSend(int length)
{
    Serial.println(F(IPcmd));
    delay(5);
    if(Serial.find("Error")) return false;
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

String getWifiPass()
{
    char val;
    boolean flag = false;
    int count = 0, i = 0;
    String str;
    while (val != '$') {
        val = EEPROM.read(i++);
        if (val == ',') count++;
        if (count == 1 && flag == false) {flag = true; continue;} 
        if (count == 2) {break;}
        if (flag) str += val;
    }
    return str;
}

String getAPI()
{
    char val;
    boolean flag = false;
    int count = 0, i = 0;
    String str;
    while (val != '$') {
        val = EEPROM.read(i++);
        if (val == ',') count++;
        if (count == 2 && flag == false) {flag = true; continue;} 
        if (count == 3) {break;}
        if (flag) str += val;
    }
    return str;
}

uint16_t getCaptureInt()
{
    char val;
    boolean flag = false;
    int count = 0, i = 0;
    String str;
    while (val != '$') {
        val = EEPROM.read(i++);
        if (val == ',') count++;
        if (count == 3 && flag == false) {flag = true; continue;} 
        if (count == 4) {break;}
        if (flag) str += val;
    }
    return str.toInt();
}

uint16_t getUploadInt()
{
    char val;
    boolean flag = false;
    int count = 0, i = 0;
    String str;
    val = EEPROM.read(i++);
    while (val != '$') {
        if (flag) str += val;
        if (val == ',') count++;
        if (count == 4) {flag = true;}
        val = EEPROM.read(i++);
    }
        
    return str.toInt();
}

String getTemp(char *buf)
{
    // Will copy 18 characters from array1 to array2
    //strncpy(array2, array1, 18);
    int count = 0, sp = 0, ep = 0;
    String str;
    for(int i=0; buf[i]!='$';i++) {
        if(buf[i]==',')  count++;
        if (count == 3 && sp == 0) {sp = i+1;}
        if (count == 4 && ep == 0) {ep = i-1;}
    }
    for (int i = sp; i != ep + 1; i++)
        str += buf[i];
    return str;
}

String getHum(char *buf)
{
    int count = 0, sp = 0;
    String str;
    for(int i=0; buf[i]!='$';i++) {
        if(buf[i]==',')  count++;
        if (count == 4 && sp == 0) {sp = i+1;}
    }
    for (int i = sp; buf[i] != '$'; i++)
        str += buf[i];
    return str;
}

String getTime(char *buf)
{
    int count = 0, sp = 0, ep = 0;
    String str;
    for(int i=0; buf[i]!='$';i++) {
        if(buf[i]==',')  count++;
        if (count == 1 && sp == 0) {sp = i+1;}
        if (count == 2 && ep == 0) {ep = i-1;}
    }
    for (int i = sp; i != ep + 1; i++)
        str += buf[i];
    str += String(F("%20"));
    sp =0; ep = 0; count = 0;
    for(int i=0; buf[i]!='$';i++) {
        if(buf[i]==',')  count++;
        if (count == 2 && sp == 0) {sp = i+1;}
        if (count == 3 && ep == 0) {ep = i-1;}
    }
    for (int i = sp; i != ep + 1; i++)
        str += buf[i];
    str += String(F("&timezone=Australia%2FSydney"));
    return str;
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
