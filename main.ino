//****************************************************************

// Smart TH WiFi Sensor code

//****************************************************************
#include <EEPROM.h>
//#include <DS3234lib3.h>
#include <PowerSaver.h>
#include <SdFat.h>
#include <dht.h>


// RTC    ******************************
int dayStart = 26, hourStart = 20, minStart = 30;    // start time: day of the month, hour, minute (values automatically assigned by the GUI)

ISR(PCINT0_vect)  // Setup interrupts on D8; Interrupt (RTC SQW) 
{
  PORTB ^= (1<<PORTB1);
}

// SD card    ******************************
SdFat sd;
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
unsigned long captureInt = 900, uploadInt = 86400;  // default, capture data per 15 min; upload per 24 hours

// Low Power 
PowerSaver chip;  // declare object for PowerSaver class

// Control flag
int captureCount = 0, uploadCount = 0;
unsigned long curTime = 0, lastCaptureTime = 0;


// setup ****************************************************************
void setup()
{
	// Todo: configured by user
	readUserSetting();

	// initialize
	initialize();

	RTC.checkInterval(hourStart, minStart, interval); // Check if the logging interval is in secs, mins or hours
	RTC.alarm2set(dayStart, hourStart, minStart);  // Configure begin time
	RTC.alarmFlagClear();  // clear alarm flag
	chip.sleepInterruptSetup();    // setup sleep function on the ATmega328p. Power-down mode is used here
}

void loop()
{
	//
	if (isCaptureMode()) {
		// turn on NPN2
		// 1. capture T/H
		// 2. save in SD
		incrCaptureCount();
		setLastCaptureTime(getCurTime());

	} else if (isUploadMode()) {
		// turn on NPN1
		uploadData();
		incrUploadCount();

	} else if (isSleepMode()) {
		setAlarm();
		// * shutdown

	}
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
		String time = RTC.timeStamp();    // get date and time from RTC
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
	    reutnr true;
	else if (getCaptureCount() == 0)
		return true;
	else
		return false;
}

bool isUploadMode()
{
	if (getCaptureCount() != 0 && getCaptureCount() % 96 == 0 && getCaptureCount() / 96 > getUploadCount())
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
	//Todo: return last capture time
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

void incrCaptureCount()
{
	captureCount++;
}

unsigned long getCaptureInt()
{
    return captureInt;
}

void setCaptureInt(unsigned long t)
{
    captureInt = t;
}

unsigned long getUploadInt()
{
    return uploadInt;
}

void setUploadInt(unsigned long t)
{
    uploadInt = t;
}
