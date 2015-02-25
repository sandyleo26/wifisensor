//****************************************************************

// Smart TH WiFi Sensor code

//****************************************************************
#include <EEPROM.h>
#include <ds3231.h>
#include <PowerSaver.h>
#include <SdFat.h>
#include <dht.h>
#include <Wire.h>


// RTC    ******************************
#define BUFF_MAX 256
char buff[BUFF_MAX];
int dayStart = 26, hourStart = 20, minStart = 30;    // start time: day of the month, hour, minute (values automatically assigned by the GUI)

// 
ISR(PCINT0_vect)  // Setup interrupts on D8; Interrupt (RTC SQW) 
{
  PORTB ^= (1<<PORTB1);
}

// SD card    ******************************
Sd2Card sd;
SdFile file;
char filename[15] = "sd_log.csv";    // file name is automatically assigned by GUI. Format: "12345678.123". Cannot be more than 8 characters in length
int SDcsPin = 9; // D9

// DHT    ******************************
dht DHT;
#define DHT22_PIN 6 // D6

// NPN    ******************************
#define NPN1 3 // D3 control ESP8266
#define NPN2 4 // D4 control DHT & SD Card

// User Configuration
unsigned long captureInt = 15, uploadInt = 1440;  // default, capture data per 15 min; upload per 24 hours

// Low Power 
PowerSaver chip;  // declare object for PowerSaver class

// Control flag
int captureCount = 0, uploadCount = 0;
unsigned long curTime = 0, lastCaptureTime = 0;

// LED
#define LED 2

// regulator
#define LDO 5

// wifi
String SSID = "TP-LINK_7AD25A";  //change to your WIFI name
String PASS = "4*108=27";  //wifi password
String IP = "184.106.153.149"; // ThingSpeak IP Address: 184.106.153.149
String GET = "GET /update?key=E8O4K566I1MZWRR2&field1=";

// setup ****************************************************************
void setup()
{
    // Todo: configured by user
    readUserSetting();

    // initialize
    initialize();

    chip.sleepInterruptSetup();    // setup sleep function on the ATmega328p. Power-down mode is used here
}

void loop()
{
    //
    if (isCaptureMode()) {
        digitalWrite(NPN2, HIGH);

        captureData();

        storeData();

        captureCount++;
        setLastCaptureTime(getCurTime());

    } else if (isUploadMode()) {
        // turn on NPN1
        digitalWrite(NPN1, HIGH);

        uploadData();
        // update status
        uploadCount++;

    } else if (isSleepMode()) {
        setAlarm();
        // * shutdown
    }
}

void goSleep()
{
    digitalWrite(NPN2, LOW);
    digitalWrite(NPN1, LOW);
    delay(5);  // give some delay
    chip.turnOffADC();
    chip.turnOffSPI();
    chip.turnOffWDT();
    chip.turnOffBOD();
    chip.goodNight();
}

void wakeUp()
{
    chip.turnOnADC();    // enable ADC after processor wakes up
    chip.turnOnSPI();   // turn on SPI bus once the processor wakes up
    delay(1);    // important delay to ensure SPI bus is properly activated
    //RTC.alarmFlagClear();
}

void readUserSetting()
{
    // Todo:
    // 1. set time
    // 2. logging interval, uploading interval
    // 3. wifi user/pass
    // 4. server URL
}

void initialize()
{
    Serial.begin(9600); // Todo: try 19200
    pinMode(NPN2, OUTPUT); // Turn on DHT & SD
    pinMode(NPN1, OUTPUT); // Turn on WiFi  
    digitalWrite(NPN2, HIGH);
    digitalWrite(NPN1, HIGH);

    // 1. check SD
    // initialize SD card on the SPI bus
    if(!sd.init(SPI_FULL_SPEED, SDcsPin)) {
        delay(10);
        SDcardError();
    }
    else {
        delay(10);
        file.open(filename, O_CREAT | O_APPEND | O_WRITE);  // open file in write mode and append data to the end of file
        delay(1);
        //String time = RTC.timeStamp();    // get date and time from RTC
        file.println();
        file.print("Date/Time,Temp(C),RH(%)");    // Print header to file
        file.println();
        PrintFileTimeStamp();
        file.close();    // close file - very important
        // give some delay to wait for the file to properly close
        delay(20);    
    }

    // 2. check wifi connection
    if(Serial.find("OK")) {
        connectWiFi();
    }

    // RTC
    Wire.begin();
    DS3231_init(DS3231_INTCN);
    DS3231_clear_a2f();
    set_next_alarm();

    // 3. check DHT
    // 4. check battery
    // 5. check USB connection
}

// Read file name ****************************************************************
void readFileName()  // get the file name stored in EEPROM (set by GUI)
{
  for(int i = 0; i < 12; i++)
  {
    filename[i] = EEPROM.read(0x06 + i);
  }
}

// SD card Error response ****************************************************************
void SDcardError()
{
    for(int i=0;i<3;i++)   // blink LED 3 times to indicate SD card write error
    {
      digitalWrite(LED, HIGH);
      delay(50);
      digitalWrite(LED, LOW);
      delay(150);
    }
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
    if (getCaptureCount() != 0 && getCaptureCount() % 96 == 0 && getCaptureCount() / 96 > uploadCount)
        return true;
    else
        return false;
}

bool isSleepMode()
{
    return true;
}

unsigned long getCurTime()
{
    return curTime;
}

unsigned long getLastCaptureTime()
{
    return lastCaptureTime;
}

void setLastCaptureTime(unsigned long t)
{
    lastCaptureTime = t;
}

unsigned int getCaptureCount()
{
    return captureCount;
}

void captureData()
{
}

void storeData()
{
}

void uploadData()
{
}

void setAlarm()
{
}

boolean connectWiFi(){
  Serial.println("AT+CWMODE=1");
  delay(2000);
  String cmd="AT+CWJAP=\"";
  cmd+=SSID;
  cmd+="\",\"";
  cmd+=PASS;
  cmd+="\"";
  Serial.println(cmd);
  delay(5000);
  if(Serial.find("OK")){
    return true;
  }else{
    return false;
  }
}

// file timestamps
void PrintFileTimeStamp() // Print timestamps to data file. Format: year, month, day, hour, min, sec
{ 
  //file.timestamp(T_WRITE, RTC.year, RTC.month, RTC.day, RTC.hour, RTC.minute, RTC.second);    // edit date modified
  //file.timestamp(T_ACCESS, RTC.year, RTC.month, RTC.day, RTC.hour, RTC.minute, RTC.second);    // edit date accessed
}

void set_next_alarm(void)
{
    struct ts t;
    unsigned char wakeup_min;

    DS3231_get(&t);

    // calculate the minute when the next alarm will be triggered
    wakeup_min = (t.min / captureInt + 1) * captureInt;
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

void debugPrintTime()
{
    // display current time
    struct ts t;
    DS3231_get(&t);
    snprintf(buff, BUFF_MAX, "%d.%02d.%02d %02d:%02d:%02d", t.year,
         t.mon, t.mday, t.hour, t.min, t.sec);
    Serial.println(buff);
}

