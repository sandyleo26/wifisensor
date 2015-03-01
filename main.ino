//****************************************************************

// Smart TH WiFi Sensor code

//****************************************************************
#include <ds3231.h>
#include <PowerSaver.h>
#include <SdFat.h>
#include <dht.h>
#include <Wire.h>

// RTC    ******************************
#define BUFF_MAX 96
//char buff[BUFF_MAX];
//int dayStart = 26, hourStart = 20, minStart = 30;    // start time: day of the month, hour, minute (values automatically assigned by the GUI)
const uint8_t days_in_month [12] PROGMEM = { 31,28,31,30,31,30,31,31,30,31,30,31 };
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

// DHT    ******************************
#define DHT22_PIN 6 // D6
dht DHT;
float temp = 0.0, hum = 0.0;

// NPN    ******************************
#define NPN1 3 // D3 control ESP8266
#define NPN2 4 // D4 control DHT & SD Card

// User Configuration
uint16_t captureInt = 60, uploadInt = 90;  // in seconds

// Low Power 
PowerSaver chip;  // declare object for PowerSaver class

// Control flag
uint32_t captureCount = 0, uploadCount = 0, uploadedLines = 0;
uint32_t curTime = 0, lastCaptureTime = 0, lastUploadTime = 0;
struct ts t;

// LED
#define LED 2

// regulator
#define LDO 5

// wifi
//#define SSID "iPhone"  //change to your WIFI name
//#define PASS "s32nzqaofv9tv"  //wifi password
#define CONCMD1 "AT+CWMODE=1"
//#define CONCMD2 "AT+CWJAP=\"iPhone\",\"s32nzqaofv9tv\"" // iPhone is SSID, s32*** is password
#define IPcmd "AT+CIPSTART=\"TCP\",\"184.106.153.149\",80" // ThingSpeak IP Address: 184.106.153.149
//String GET = "GET /update?key=8LHRO7Q7L74WVJ07&field1=";
char wifiName[16]; 
char wifiPass[16];
#define API "GET /update?key=8LHRO7Q7L74WVJ07&field1="


//ArduinoOutStream cout(Serial);

// setup ****************************************************************
void setup()
{
    initialize();
    readUserSetting();
    testMemSetup();
    chip.sleepInterruptSetup();    // setup sleep function on the ATmega328p. Power-down mode is used here
}

void loop()
{
    updateCurTime();
    //debugPrintTime();
    //testCaptureData();
    //testMemSetup();
    //testUploadData();
    //testWiFi();
    delay(10);
    if (isCaptureMode()) {
        //Serial.println(F("CaptureMode"));
        digitalWrite(NPN2, HIGH);
        captureStoreData();
        setLastCaptureTime();
        //debugPrintTime();
    } else if (isUploadMode()) {
        //Serial.println(F("UploadMode"));
        digitalWrite(NPN1, HIGH);
        uploadData();
        // update status
        uploadCount++;
        setLastUploadTime();
        //debugPrintTime();
    } else if (isSleepMode()) {
        //Serial.println(F("SleepMode"));
        setAlarm();
        goSleep();
        delay(5000);
    }
}

void goSleep()
{
    //Serial.println(F("goSleep"));
    //debugPrintTime();
    delay(2000);
    digitalWrite(NPN2, LOW);
    digitalWrite(NPN1, LOW);
    delay(5);  // give some delay
    chip.turnOffADC();
    chip.turnOffSPI();
    chip.turnOffWDT();
    chip.turnOffBOD();
    chip.goodNight();
    if (DS3231_triggered_a2()) {
        delay(1000);
        //debugPrintTime();
        //Serial.println(F("**Alarm has been triggered**"));
        DS3231_clear_a2f();
    }
}

void wakeUp()
{
    //chip.turnOnADC();    // enable ADC after processor wakes up
    chip.turnOnSPI();   // turn on SPI bus once the processor wakes up
    delay(1);    // important delay to ensure SPI bus is properly activated
    //RTC.alarmFlagClear();
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
      strncpy(wifiName, buffer, 16);
    }else if (num == 2){
      //wifiPassword = (String) buffer;
      strncpy(wifiPass, buffer, 16);
    }else if (num == 3){
      //API = (String) buffer;
      //strncpy(API, buffer, 60);
    }else if (num == 4){
      captureInt = atof(buffer) * 60;
    }else if (num == 5){
      uploadInt = atof(buffer) * 60;
    }   
  }
  myFile.close();
  //debug
//   Serial.println(wifiName);
//    Serial.println(wifiPass);
//     Serial.println(API);
//      Serial.println(captureInt);
//       Serial.println(uploadInt);
}

void initialize()
{
    Serial.begin(9600); // Todo: try 19200
    //Serial.println("initialize");
    delay(1000);
    pinMode(NPN2, OUTPUT); // Turn on DHT & SD
    pinMode(NPN1, OUTPUT); // Turn on WiFi  
    digitalWrite(NPN2, HIGH);
    digitalWrite(NPN1, HIGH);

    // 1. check SD
    // initialize SD card on the SPI bus
    if (!sd.begin(SDcsPin, SPI_HALF_SPEED)) sd.initErrorHalt();
    if (!myFile.open(sdLogFile, O_WRITE | O_CREAT | O_TRUNC)) {
        sd.errorHalt("opening failed");
    }
    myFile.close();
    
//    // 2. check wifi connection
//    if(Serial.find("OK")) {
//        connectWiFi();
//    }

    // RTC
    Wire.begin();
    DS3231_init(DS3231_INTCN);
    DS3231_clear_a2f();
    updateCurTime();
    // 3. check DHT
    // 4. check battery
    // 5. check USB connection
    // 6. check wifi
    
    setLastUploadTime();
}

bool isCaptureMode()
{
    if ((getCurTime() - getLastCaptureTime()) > captureInt)
        return true;
    else if (getCaptureCount() == 0)
        return true;
    else
        return false;
}

bool isUploadMode()
{
    if (getCurTime() - getLastUploadTime() > uploadInt)
        return true;
    else
        return false;
}

bool isSleepMode()
{
    return true;
}

void updateCurTime()
{
    DS3231_get(&t);
    curTime = getUnixTime();
}

uint32_t getCurTime()
{
    return curTime;
}

uint32_t getLastCaptureTime()
{
    return lastCaptureTime;
}

uint32_t getLastUploadTime()
{
    return lastUploadTime;
}

void setLastCaptureTime()
{
    
    lastCaptureTime = getCurTime();
}

void setLastUploadTime()
{
    lastUploadTime = getCurTime();
}

unsigned int getCaptureCount()
{
    return captureCount;
}

void testCaptureData()
{
    //char raw[64] = "1,2015-02-27,01:44:33,38.5,66.6$";
    char ttmp[8]; 
    char htmp[8];
    String str;
    delay(5000);
//    dtostrf(27.5, 3, 1, buff);
//    Serial.println(buff);
//    Serial.println(sizeof(buff));
//    dtostrf(-3.0, 3, 1, buff);
//    Serial.println(buff);
//    Serial.println(sizeof(buff));
//    Serial.println(F("testCaptureData"));
    int chk = DHT.read22(DHT22_PIN);
    //Serial.print("chk: "); Serial.print(chk);
    temp = DHT.temperature;
    hum = DHT.humidity;
    captureCount++;
    dtostrf(temp, 3, 1, ttmp);
    dtostrf(hum, 3, 1, htmp);
    str += String(captureCount) + "," + String(t.year) + "-" + String(t.mon) + "-" + String(t.mday) + "," + 
        String(t.hour) + ":" + String(t.min) + ":" + String(t.sec) + "," + String(ttmp) + "," + String(htmp) + String("$");
        
    //snprintf(buff, BUFF_MAX, "%ld,%d-%02d-%02d,%02d:%02d:%02d,",t.year, t.mon, t.mday, t.hour, t.min, t.sec);    

//    Serial.println(str);
    
//    Serial.println(F("storeData"));
    delay(1000);
    if (!sd.begin(SDcsPin, SPI_HALF_SPEED)) sd.initErrorHalt();
    if (!myFile.open(sdLogFile, O_RDWR | O_CREAT | O_AT_END)) {
        sd.errorHalt("sd!");
    }
    //myFile.println("testing 1, 2, 3.");
    myFile.println(str);
    // close the file:
    myFile.close();
    
//    if (!myFile.open(sdLogFile, O_READ)) {
//        sd.errorHalt("opening failed");
//    }
//    Serial.println(sdLogFile);
//
//    // read from the file until there's nothing else in it:
//    int data;
//    while ((data = myFile.read()) >= 0) Serial.write(data);
//    // close the file:
//    myFile.close(); 
}

void captureStoreData()
{
    //char raw[64] = "9999999,2015-02-27,01:44:33,-38.5,66.6$";
    char ttmp[8]; 
    char htmp[8];
    String str;
    //Serial.println(F("captureStoreData"));
    int chk = DHT.read22(DHT22_PIN);
    //Serial.print("chk: "); Serial.print(chk);
    temp = DHT.temperature;
    hum = DHT.humidity;
    captureCount++;
    dtostrf(temp, 3, 1, ttmp);
    dtostrf(hum, 3, 1, htmp);
    str += String(captureCount) + "," + String(t.year) + "-" + String(t.mon) + "-" + String(t.mday) + "," + 
        String(t.hour) + ":" + String(t.min) + ":" + String(t.sec) + "," + String(ttmp) + "," + String(htmp) + String("$");
        
    //snprintf(buff, BUFF_MAX, "%ld,%d-%02d-%02d,%02d:%02d:%02d,",t.year, t.mon, t.mday, t.hour, t.min, t.sec);    

    //Serial.println(str);
    
    //Serial.println(F("storeData"));
    delay(1000);
    if (!sd.begin(SDcsPin, SPI_HALF_SPEED)) sd.initErrorHalt();
    if (!myFile.open(sdLogFile, O_RDWR | O_CREAT | O_AT_END)) {
        sd.errorHalt("sd!");
    }
    //myFile.println("testing 1, 2, 3.");
    myFile.println(str);
    // close the file:
    myFile.close();
    
//    if (!myFile.open(sdLogFile, O_READ)) {
//        sd.errorHalt("opening failed");
//    }
//    Serial.println(sdLogFile);
//
//    // read from the file until there's nothing else in it:
//    int data;
//    while ((data = myFile.read()) >= 0) Serial.write(data);
//    // close the file:
//    myFile.close(); 
}

void uploadData()
{
    String stemp, shum, stime, data;
    int lineNum = 0;
    const int line_buffer_size = 64;
    char buffer[line_buffer_size];
    Serial.println(F("AT"));
    delay(2000);
//    Serial.println(F("AT"));
//    delay(2000);
//    Serial.println(F("AT"));
//    delay(2000);
    if(Serial.find("OK")){
        Serial.println(F("connectWiFi"));
        if (connectWiFi()) {
            // initialize the SD card at SPI_HALF_SPEED to avoid bus errors with
            // breadboards.  use SPI_FULL_SPEED for better performance.
            if (!sd.begin(SDcsPin, SPI_HALF_SPEED)) sd.initErrorHalt();
            ifstream sdin(sdLogFile);         

            while (sdin.getline(buffer, line_buffer_size, '\n') || sdin.gcount()) {
              //int count = sdin.gcount();
              if (sdin.fail()) {
                //cout << "Partial long line";
                Serial.println(F("Partial long line"));
                sdin.clear(sdin.rdstate() & ~ios_base::failbit);
              } else if (sdin.eof()) {
                //cout << "Partial final line";  // sdin.fail() is false
                Serial.println(F("Partial final line"));
              } else {
                //count--;  // Don’t include newline in count
                //cout << "Line " << ++lineNum;
                lineNum++;
                //Serial.print(F("Line ")); Serial.println(lineNum);
              }
              if (lineNum<=uploadedLines) continue;

              // created_at=[Date] in the format YYYY-MM-DDThh:mm:ss+/-hh:00 es: 2013-11-07T18:04:02+01:00 where the last part is the time zone
              // String data = getTemp(buffer) + "&field2=" + getHum(buffer) +
              //     "&created_at=" + getTime(buffer);
              
              //cout << " (" << count << " chars): " << buffer << endl;
              //Serial.print(count); Serial.println(F(" chars: ")); Serial.println(buffer);
              stemp = getTemp(buffer);
              shum = getHum(buffer);
              stime = getTime(buffer);
              //https://api.thingspeak.com/update?api_key=8LHRO7Q7L74WVJ07&field1=33&field2=3&created_at=2015-02-27%2012:43:00
              data = stemp + String(F("&field2=")) + shum + String(F("&created_at=")) + stime;                        
              Serial.println(data);
              transmitData(data);
              delay(10000);  
            }
        }
    }
}

void setAlarm()
{
    struct ts t;
    unsigned char wakeup_min;
    DS3231_get(&t);
    unsigned long intMin = 0;
    
    //Serial.println(F("setAlarm"));
    // calculate the minute when the next alarm will be triggered
    intMin = captureInt/60;
    wakeup_min = (t.min / intMin + 1) * intMin;
    if (wakeup_min > 59) {
        wakeup_min -= 60;
    }

    // flags define what calendar component to be checked against the current time in order
    // to trigger the alarm
    // A2M2 (minutes) (0 to enable, 1 to disable)
    // A2M3 (hour)    (0 to enable, 1 to disable) 
    // A2M4 (day)     (0 to enable, 1 to disable)
    // DY/DT          (dayofweek == 1/dayofmonth == 0)
    boolean flags[4] = { 0, 1, 1, 1 };

    // set Alarm2. only the minute is set since we ignore the hour and day component
    DS3231_set_a2(wakeup_min, 0, 0, flags);

    // activate Alarm2
    DS3231_set_creg(DS3231_INTCN | DS3231_A2IE);
}

boolean connectWiFi(){
    Serial.println(F(CONCMD1));
    delay(2000);
    //Serial.println(F(CONCMD2));
    Serial.println(String(F("AT+CWJAP=\"")) + String(wifiName) + String(F("\",\"")) + String(wifiPass) + String(F("\"")));
    delay(5000);
    if (Serial.find("OK")) {
      return true;
    } else {
      return false;
    }
}

void debugPrintTime()
{
    // display current time
    String str;
    DS3231_get(&t);
    //snprintf(buff, BUFF_MAX, "%02d:%02d:%02d,cap:%d,up:%d,",t.hour, t.min, t.sec, captureCount, uploadCount);
    //Serial.println(buff);
    str += String(t.year) + "-" + String(t.mon) + "-" + String(t.mday) + "," + 
        String(t.hour) + ":" + String(t.min) + ":" + String(t.sec) + ",cap:" + String(captureCount) + ",up:" + String(uploadCount);
    Serial.println(str);
}

uint32_t getUnixTime()
{
    uint8_t i;
    uint16_t d;
    int16_t y;
    uint32_t rv;

    if (t.year >= 2000) {
        y = t.year - 2000;
    } else {
        return 0;
    }

    d = t.mday - 1;
    for (i=1; i<t.mon; i++) {
        d += pgm_read_byte(days_in_month + i - 1);
    }
    if (t.mon > 2 && y % 4 == 0) {
        d++;
    }
    // count leap days
    d += (365 * y + (y + 3) / 4);
    rv = ((d * 24UL + t.hour) * 60 + t.min) * 60 + t.sec + SECONDS_FROM_1970_TO_2000;
    return rv;
}

//void makeTestFile() {
//  ofstream sdout("GETLINE.TXT");
//  // use flash for text to save RAM
//  sdout << pstr(
//    "short line\n"
//    "\n"
//    "17 character line\n"
//    "too long for buffer\n"
//    "line with no nl");
//
//  sdout.close();
//}

void transmitData(String data)
{  
  //String cmd(GET);
  String cmd(F(API));
  int i = 0;
//  char buf[32];
  Serial.println(F(IPcmd));
  delay(2000);
  if(Serial.find("Error")){
    return;
  }

  cmd += data;
  cmd += "\r\n";
  Serial.print(F("AT+CIPSEND="));
  Serial.println(cmd.length());
  delay(5000);
  if(Serial.find(">")){
    Serial.print(cmd);
//    while (Serial.available()) {
//        buf[i] = Serial.read();
//        if (buf[i] == '\r') {buf[i] = '\n';break;}
//        i++;
//    }
//    Serial.println(buf);
    uploadedLines++;
  }else{
    Serial.println(F("AT+CIPCLOSE"));
  }
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
    str += "%20";
    sp =0; ep = 0; count = 0;
    for(int i=0; buf[i]!='$';i++) {
        if(buf[i]==',')  count++;
        if (count == 2 && sp == 0) {sp = i+1;}
        if (count == 3 && ep == 0) {ep = i-1;}
    }
    for (int i = sp; i != ep + 1; i++)
        str += buf[i];
    //str += "+10:00";
    return str;
}

void testWiFi()
{
    String stemp, shum, stime, data;
    char raw[64] = "1,2015-02-27,01:44:33,38.5,66.6$";    
    Serial.println(F("AT"));
    delay(5000);
    if(Serial.find("OK")){
    //if (true) {
        Serial.println(F("connectWiFi"));
        if (connectWiFi()) {
        //if (true) {
            stemp = getTemp(raw);
            shum = getHum(raw);
            //stime = getTime(raw);
            //data = stemp + "&field2=" + shum + "&created_at=" + stime;
            data = stemp + "&field2=" + shum;            
            //Serial.println(data);
            transmitData(data);
            delay(30000);  
        }
    }
}

int freeRam () {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}

void testMemSetup () {
    //Serial.begin(57600);
    Serial.println("\n[memCheck]");
    Serial.println(freeRam());
}


