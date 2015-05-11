//****************************************************************

// test only the wifi part of code

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
#define MAX_LINES_PER_FILE 100
#define WIFI_BUF_MAX 64

ISR(PCINT0_vect)  // Setup interrupts on D8; Interrupt (RTC SQW) 
{
  PORTB ^= (1<<PORTB1);
}

// SD card    ******************************
SdFat sd;
SdFile myFile;
char sdLogFile[15] = "";
char configFile[] = "config.txt";
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
//#define wifiName "iPhone"  //change to your WIFI name
//#define wifiPass "Fh818891"  //wifi password
// #define wifiName "Elonihp"  //change to your WIFI name
// #define wifiPass "qazwsxedc"  //wifi password
#define wifiName "TEHS_53281F"  //change to your WIFI name
#define wifiPass "8847EA0B45"  //wifi password
//#define wifiName "NetComm 3521"  //change to your WIFI name
//#define wifiPass "Thawifi2"  //wifi password
#define CONCMD1 "AT+CWMODE=1"
//#define IPcmd "AT+CIPSTART=\"TCP\",\"184.106.153.149\",80" // ThingSpeak
//#define IPcmd "AT+CIPSTART=\"TCP\",\"69.195.124.239\",80" // meetisan
#define IPcmd "AT+CIPSTART=\"TCP\",\""
#define getIP "69.195.124.239"
#define getAPI "GET /~meetisan/r.php?k=test3&d="

//#define getIP "184.106.153.149"
//#define getAPI "/update?api_key=8LHRO7Q7L74WVJ07&field1="

#define getIP "119.9.30.179"
#define getPort "\",80"
#define getPort "\",12345"


// setup ****************************************************************
void setup()
{
    initialize();
    //readUserSetting();
    delay(5);
}

void initialize()
{
    Serial.begin(57600);
    delay(5);
    pinMode(LDO, OUTPUT);
    pinMode(LED, OUTPUT);
    pinMode(NPN_Q1, OUTPUT); // Turn on DHT & SD
    pinMode(WIFI_CP_PD, OUTPUT); // Turn on WiFi 
    pinMode(SDcsPin, OUTPUT); 
    digitalWrite(LDO, HIGH);
    digitalWrite(LED, HIGH);
    digitalWrite(NPN_Q1, HIGH);
    digitalWrite(WIFI_CP_PD, HIGH);

    // 1. RTC
    Wire.begin();
    DS3231_init(DS3231_INTCN);
    DS3231_clear_a1f();
    DS3231_get(&t);

    //Serial.println(F("AT+CIPMUX=0"));
    if (initWifiSerial()) {
        connectWiFi();

    }

    // Serial.println(F("AT+CIFSR"));

    // uint8_t i = 0;
    // uint8_t n = 0;
    // uint8_t j = 0;
    // uint8_t k = 0;
    // char buffer[WIFI_BUF_MAX];

    // while (1) {
    //     if (i++>25) break;
    //     delay(1000);
    //     if ((n = Serial.available()) != 0) {
    //         j = 0;
    //         k = n < WIFI_BUF_MAX - 1 ? n : WIFI_BUF_MAX -1;
    //         while (j<k)
    //             buffer[j++] = Serial.read();
    //         buffer[k] = '\0';
    //         Serial.println(buffer);
    //     }
    // }
}

void loop()
{
    DS3231_get(&t);
    debugPrintTime();
    testWiFi();
    delay(20000);
}

// original recipe
// boolean connectWiFi() {
//     uint8_t i = 0;
//     uint8_t n = 0;
//     uint8_t j = 0;
//     char buffer[WIFI_BUF_MAX];

//     Serial.println(F(CONCMD1));
//     cwjap();
//     while (1) {
//         if (i++>20) return false;
//         delay(1000);
//         if ((n = Serial.available()) != 0) {
//             j = 0;
//             while (j<WIFI_BUF_MAX-1)
//                 buffer[j++] = Serial.read();
//             buffer[WIFI_BUF_MAX-1] = '\0';
//             if (strstr(buffer, "OK")) {
//                 break;
//             }
//             else if (strstr(buffer, "FAIL")) {
//                 cwjap();
//             }
//         }
//     }
//     return true;
// }

// detect ready and retry
boolean connectWiFi() {
    uint8_t i = 0;
    uint8_t n = 0;
    uint8_t j = 0;
    uint8_t k = 0;
    char buffer[WIFI_BUF_MAX];

    //Serial.println(F(CONCMD1));
    cwjap924();
    //delay(15000);
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
                //cwjap();
            } else if (strstr(buffer, "ready")) {
                cwjapxxx();
                delay(3000);
                //cwjap();
            }
        }
    }
    return true;
}

// boolean connectWiFi1()
// {
//     Serial.println(F(CONCMD1));
//     int i = 0;
//     int j = 0;
//     while (i++<5) {
//         j = 0;
//         cwjap();
//         while (!Serial.find("OK")) {
//             if (++j>15) break;
//             delay(2000);
//         }
//         if (j<=15)
//             return true;
//     }

//     return true;
// }

// boolean connectWiFi()
// {
//     Serial.println(F(CONCMD1));
//     delay(2000);
//     cwjap();
//     delay(10000);
//     if(Serial.find("OK")){
//         //monitor.println("RECEIVED: OK");
//         return true;
//     } else {
//         //monitor.println("RECEIVED: Error");
//         return true;
//     }
// }

void cwjap950() {
    //Serial.println(F("AT+CWJAP_CUR=\"Elonihp\",\"qazwsxedc\"")); 
    Serial.println(F("AT+CWJAP_CUR=\"iPhone\",\"Fh818891\"")); 
    // Serial.print(F(wifiName)); Serial.print(F("\",\"")); 
    // Serial.print(F(wifiPass)); Serial.println(F("\""));
}
void cwjap924() {
    Serial.print(F("AT+CWJAP=\"")); 
    Serial.print(F(wifiName)); Serial.print(F("\",\"")); 
    Serial.print(F(wifiPass)); Serial.println(F("\""));
}

void cwjapxxx() {
    Serial.print(F("AT+CWJAP_CUR=\"")); 
    Serial.print(F("XXXX")); Serial.print(F("\",\"")); 
    Serial.print(F("XXXX")); Serial.println(F("\""));
}

boolean transmitData(char* data, uint16_t lines) {  

    String cmd;
    int length;
    uint8_t i = 0;

    cmd = getAPI;
    length = cmd.length() + strlen(data) + 2;

    if (!initDataSend(length)) return false;

    while (!Serial.find(">")) {
        if (i++>10) return false;
        delay(200);
        // Serial.println(F("AT+CIPCLOSE"));
        // Serial.println(F("initWifiSerial"));
        // if (!initWifiSerial()) return false;

        // if (connectWiFi()) {
        //     if (!initDataSend(length)) return false;
        // }
    }
    Serial.print(cmd); Serial.print(data); Serial.print(F("\r\n"));
    //Serial.println(F("AT+CIPCLOSE"));
    uploadedLines += lines;
    i = 0;
    while (!Serial.find("SEND OK"))
        if (i++>100) break;
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
        delay(100);
    }
    return true;
}

void cipstart() {
    Serial.print(F(IPcmd)); Serial.print(F(getIP)); Serial.println(F(getPort));
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

void testWiFi()
{
    String data, stime;
    char raw[64] = "1,2015-02-27,01:44:33,38.5,66.6$";  
    
    Serial.println(F("testWiFi"));
    // if (!initWifiSerial()) return;

    //Serial.println(F("connectWiFi"));
//    if (connectWiFi()) {
        transmitData(raw, 1);
 //   }
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

