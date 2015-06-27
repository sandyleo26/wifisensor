#define    WIFI_SSID    "lijing"
#define    WIFI_KEY     "12345678"
//****************************************************************

// Smart TH WiFi Sensor code

//****************************************************************
#include <ds3231.h>
#include <SdFat.h>
#include <Wire.h>
#include <EEPROM.h>
#include "HTU21D.h"

// SD card    ******************************
SdFat sd;
SdFile myFile;
char sdTextLog[] = "testlog.txt";
char configFile[] = "config.txt";

#define SDcsPin 9 // D9

// HTU21D    ******************************
HTU21D htu;

// NPN    ******************************
#define WIFI_CP_PD 3 // D3 control ESP8266
#define NPN_Q1 4 // D4 control DHT & SD Card
#define LED_ON   LOW
#define LED_OFF  HIGH
#define VCC_ON   HIGH
#define VCC_OFF  LOW
#define WIFI_ON   HIGH
#define WIFI_OFF  LOW

// Control flag
boolean testFailFlag = false;

struct ts t;

// LED
#define LED 2

// regulator
#define LDO 5
#define VCC LDO

// setup ****************************************************************
void setup()
{
    initialize();

    boolean bFlag;

    bFlag = createNewLogFile();
    if (!bFlag)
    {
        while (true)
        {
            flickerLED1();
        }
    }
}

void flickerLED1()
{
    digitalWrite(LED, LED_ON);
    delay(100);
    digitalWrite(LED, LED_OFF);
    delay(100);
    digitalWrite(LED, LED_ON);
    delay(100);
    digitalWrite(LED, LED_OFF);
    delay(100);
    digitalWrite(LED, LED_ON);
    delay(100);
    digitalWrite(LED, LED_OFF);
    delay(2000);

}

void flickerLED2()
{
    digitalWrite(LED, LED_ON);
    delay(500);
    digitalWrite(LED, LED_OFF);
    delay(500);
}

boolean createNewLogFile()
{
    uint8_t i = 0;
    ifstream f(sdTextLog);
    while (!sd.begin(SDcsPin, SPI_FULL_SPEED))
    {
        digitalWrite(LDO, VCC_OFF);
        delay(3000);
        digitalWrite(LDO, VCC_ON);
        delay(3000);
        if (i == 10)
        {
            return false;
        }
        i++;
    }
    if (!myFile.open(sdTextLog, O_WRITE | O_CREAT | O_TRUNC))
    {
        myFile.close();
    }
    else
    {
        myFile.close();
        return true;
    }

    return false;
}

void initialize()
{
    delay(5);
    pinMode(VCC, OUTPUT);
    pinMode(LED, OUTPUT);
    pinMode(NPN_Q1, OUTPUT); // Turn on DHT & SD
    pinMode(WIFI_CP_PD, OUTPUT); // Turn on WiFi 
    pinMode(SDcsPin, OUTPUT); 
    digitalWrite(VCC, VCC_ON);
    digitalWrite(LED, LED_ON);
    digitalWrite(NPN_Q1, VCC_ON);
    digitalWrite(WIFI_CP_PD, WIFI_ON);

    // 1. RTC
    Wire.begin();
    DS3231_init(DS3231_INTCN);
    adjustTime(__DATE__, __TIME__);
    delay(1000);
    DS3231_clear_a1f();
    DS3231_get(&t);

    // 2. check SD
    // initialize SD card on the SPI bus
    int i = 0;
    while (!sd.begin(SDcsPin, SPI_FULL_SPEED))
    {
        delay(1000);
        Serial.print(++i); Serial.println(F(" initialize fail."));
        // sysLog(F("initialize."));
        if (i > 10)
        {
           while (true)
           {
             flickerLED1();
           }
        }
       
    }
    //createNewLogFile();

    // HTU21D
    htu.begin();

}

void loop()
{
    //test begin
    digitalWrite(LED, LED_ON);
    
    //DS3231 test
    DS3231_test();
    
    //HTU test
    HTU_test();
    
    //WIFI test
    WIFI_test();
    
    if (testFailFlag)
    {
        while (true)
            flickerLED2();
    }
    else
    {
        digitalWrite(LED, LED_OFF);
        while (true)
            delay(5000000);
    }
}

void DS3231_test()
{
    char logMessage[32];
    uint8_t timebef, timeaft;

    DS3231_get(&t);
    //test RTC
    if (t.year != 2015)
    {
        testFail();
        memset(logMessage, 0, 32);
        sprintf(logMessage, "RTC time unSet! year = %d", t.year);
        testLogPrint(logMessage);
    }

    //test time count
    for (int i = 0; i < 5; i++)
    {
        DS3231_get(&t);
        timebef = t.sec;
        delay(1000);
        DS3231_get(&t);
        timeaft = t.sec;
        if ((timeaft - timebef) > 2 || (timeaft + 10 - timebef) < 10 )
        {
            memset(logMessage, 0, 64);
            sprintf(logMessage, "RTC time flicker! bef=%d,aft=%d", timebef, timeaft);
            testLogPrint(logMessage);
            testFail();
            break;
        }
    }

    //test alarm
}

void HTU_test()
{
    char ttmp[8];
    char htmp[8];
    char logMessage[32];
    float temp, hum;

    digitalWrite(LDO, VCC_ON);
    digitalWrite(NPN_Q1, VCC_ON);
    delay(500);

    for (int i=0; i < 10; i++)
    {
        memset(ttmp, 0, 8);
        memset(htmp, 0, 8);
        temp = 0;
        hum = 0;

        temp = htu.readTemperature();
        hum = htu.readHumidity();
        if (temp < 10.0 || temp > 40.0)
        {
            memset(logMessage, 0, 32);
            dtostrf(temp, 3, 1, ttmp);
            sprintf(logMessage, "Htu temp error!Temp:%s", ttmp);
            testLogPrint(logMessage);
            testFail();
        }
        if (hum < 20.0 || hum > 90.0)
        {
            memset(logMessage, 0, 32);
            dtostrf(hum, 3, 1, htmp);
            sprintf(logMessage, "Htu hum error!Hum:%s", htmp);
            testLogPrint(logMessage);
            testFail();
        }
        dtostrf(temp, 3, 1, ttmp);
        dtostrf(hum, 3, 1, htmp);
        memset(logMessage, 0, 32);
        sprintf(logMessage, "Temp:%s, Hum:%s", ttmp, htmp);
        testLogPrint(logMessage);
        delay(1000);
    }

//    digitalWrite(NPN_Q1, HIGH);
}

boolean testSerialFunc(char *findKey, char *input, char *logMessage, uint8_t delayTime, uint8_t retryTime)
{
    uint8_t i = 0;
    while (!Serial.find(findKey)) {
        if (i++ > retryTime)
        {
            testLogPrint(logMessage);
            testFail();
            return true;
        }
        if (input)
            Serial.println(input);
        delay(delayTime);
    }
    return false;
}

void WIFI_test()
{
    char buffer[128];
    char wifi_connect_fail_str[] = "Connect WiFi failed!";
    digitalWrite(LDO, VCC_ON);
    delay(500);
    digitalWrite(WIFI_CP_PD, WIFI_ON);
    delay(500);

    Serial.begin(57600);
    Serial.println(F("AT"));//WIFI module on position
    if (testSerialFunc("OK", "AT", wifi_connect_fail_str, 100, 10))
        return;

    Serial.println(F("AT+CWMODE=1"));//init module setting
    if (testSerialFunc("OK", NULL, wifi_connect_fail_str, 100, 10))
        return;

    Serial.println(F("AT+CIPMODE=0"));//init module setting
    if (testSerialFunc("OK", NULL, wifi_connect_fail_str, 100, 10))
        return;

    Serial.println(F("AT+CIPMUX=0"));//init module setting
    if (testSerialFunc("OK", NULL, wifi_connect_fail_str, 100, 10))
        return;
    
    //Serial.println(F("AT+RST"));

    //delay(10000);

    Serial.println(F("AT"));//WIFI module on position
    if (testSerialFunc("OK", "AT", wifi_connect_fail_str, 100, 10))
        return;
    delay(500);

    memset(buffer, 0, 128);
    sprintf(buffer, "AT+CWJAP=\"%s\",\"%s\"", WIFI_SSID, WIFI_KEY);//jion AP
    Serial.println(buffer);
    if (testSerialFunc("OK", NULL, "Join AP failed!", 1000, 20))
        return;

    delay(500);
    Serial.println(F("AT+CIPSTART=\"TCP\",\"www.baidu.com\",80"));// use baidu as a TCP test server

    delay(10000);
    Serial.println(F("AT+CIPSEND=4"));// send test data
    if (testSerialFunc(">", NULL, "WIFI_Mode test failed!(TCP_SEND)", 500, 10))
        return;

    Serial.print(F("OK\r\n"));// send test data
    delay(500);
    if (testSerialFunc("SEND OK", NULL, "WIFI_Mode test failed!(SEND_OK)", 100, 10))
        return;

    return;
}

void testLogPrint(char *pTestMessage)
{
    if (!myFile.open(sdTextLog, O_RDWR | O_CREAT | O_AT_END))
    {
        while (true)
        {
            flickerLED1();
        }
    }
    myFile.println(pTestMessage);
    myFile.close();
}

void testFail()
{
    testFailFlag = true;
}

void adjustTime (const char* date, const char* time) {
    // sample input: date = "Dec 26 2009", time = "12:34:56"
    struct ts t;
    t.year = conv2y(date + 9);
    Serial.println(t.year);
    // Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec
    switch (date[0]) {
        case 'J': t.mon = date[1] == 'a' ? 1 : t.mon = date[2] == 'n' ? 6 : 7; break;
        case 'F': t.mon = 2; break;
        case 'A': t.mon = date[2] == 'r' ? 4 : 8; break;
        case 'M': t.mon = date[2] == 'r' ? 3 : 5; break;
        case 'S': t.mon = 9; break;
        case 'O': t.mon = 10; break;
        case 'N': t.mon = 11; break;
        case 'D': t.mon = 12; break;
    }
    t.mday = conv2d(date + 4);
    t.hour = conv2d(time);
    t.min = conv2d(time + 3);
    t.sec = conv2d(time + 6);
    DS3231_set(t);
}

static uint8_t conv2d(const char* p) {
    uint8_t v = 0;
    if ('0' <= *p && *p <= '9')
        v = *p - '0';
    return 10 * v + *++p - '0';
}

static uint8_t conv2y(const char* p) {
    uint8_t v = 0;
    if ('0' <= *p && *p <= '9')
        v = *p - '0';
    return 10 * v + *++p - '0' + 2000;
}
