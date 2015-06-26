#define CONFIG_UNIXTIME
#include <ds3231.h>
#include <SdFat.h>
#include <Wire.h>

#define LED 2
#define LDO 5
#define WIFI_CP_PD 3 // D3 control ESP8266
#define NPN_Q1 4 // D4 control DHT & SD Card
#define WIFI_BUF_MAX 64
#define SDcsPin 9 // D9


// test program to find out that turnning off wifi's vcc can cause sd abnormal
// behavior while switching off CPPD doesn't
#define VER "19 June 2015"
#define SECONDS_DAY 86400
#define BUFF_MAX 96
#define MAX_LINES_PER_FILE 200
#define MAX_LINES_PER_UPLOAD 2
#define LINE_BUF_SIZE 40*MAX_LINES_PER_UPLOAD
#define WIFI_BUF_MAX 64

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

char sdLogFile[15] = "";
const char string_0[] PROGMEM = "L%02d%02d%02d%c.csv";   // "String 0" etc are strings to store - change to suit.
const char string_1[] PROGMEM = "";
const char *const string_table[] PROGMEM =       // change "string_table" name to suit

{   
  string_0,
  string_1 };

struct ts t;
SdFat sd;
SdFile myFile;

void setup() {
  // put your setup code here, to run once:
    Serial.begin(57600);
    pinMode(LDO, OUTPUT);
    pinMode(LED, OUTPUT);
    pinMode(NPN_Q1, OUTPUT); // Turn on DHT & SD
    pinMode(WIFI_CP_PD, OUTPUT); // Turn on WiFi 
    digitalWrite(LDO, HIGH);
    digitalWrite(LED, HIGH);
    digitalWrite(NPN_Q1, HIGH);
    digitalWrite(WIFI_CP_PD, HIGH);

    // 1. RTC
    Wire.begin();
    DS3231_init(DS3231_INTCN);
    DS3231_clear_a1f();
    DS3231_get(&t);

    // 2. check SD, initialize SD card on the SPI bus
    int i = 0;
    delay(1000);
    while (!sd.begin(SDcsPin, SPI_FULL_SPEED)) {
        DEBUG_PRINT(++i); DEBUG_PRINTLN(F(" initialize fail."));
        digitalWrite(LDO, LOW);
        delay(3000);
        digitalWrite(LDO, HIGH);
        delay(5000);
        if (i > 10) sd.initErrorHalt();
    }
    SdFile::dateTimeCallback(dateTime);
    createNewLogFile();
}

void loop() {
    int i = 0;
    while (i++<3) {
        createNewLogFile();
        delay(2000);
    }
    //toggleWiFi();
    delay(1000);
    sd.begin(SDcsPin, SPI_FULL_SPEED);
    delay(500);
}

void toggleWiFi()
{
    digitalWrite(WIFI_CP_PD, LOW);
    delay(500);
    digitalWrite(WIFI_CP_PD, HIGH);
    delay(1000);
}

void createNewLogFile()
{
    if (!createNewLogFile(false))
        if (!createNewLogFile(true)) {
            Serial.println(F("Overwrite fails."));
        }
}
boolean createNewLogFile(boolean overwrite)
{
    char c = 'a';
    char fmt[24];
    uint8_t i = 0;

    strcpy_P(fmt, (char*)pgm_read_word(&(string_table[0])));
    DS3231_get(&t);
    while(i++<26) {
        sprintf(sdLogFile, fmt, t.year-2000, t.mon, t.mday, c++);
        ifstream f(sdLogFile);
        if (f.good()) {
            f.close();
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

void dateTime(uint16_t* date, uint16_t* time) {
    // return date using FAT_DATE macro to format fields
    *date = FAT_DATE(t.year, t.mon, t.mday);
    // return time using FAT_TIME macro to format fields
    *time = FAT_TIME(t.hour, t.min, t.sec);
}
