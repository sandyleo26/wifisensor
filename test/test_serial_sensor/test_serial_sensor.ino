#include <SoftwareSerial.h>

#include <SdFat.h>

#define LED 2
#define LDO 5
#define WIFI_CP_PD 3 // D3 control ESP8266
#define NPN_Q1 4 // D4 control DHT & SD Card
#define WIFI_BUF_MAX 64
#define SDcsPin 9

String inputString = "";         // a string to hold incoming data

SdFat sd;
SdFile myFile;

void setup() {
  // initialize serial:
    pinMode(LDO, OUTPUT);
    pinMode(LED, OUTPUT);
    pinMode(NPN_Q1, OUTPUT); // Turn on DHT & SD
    pinMode(WIFI_CP_PD, OUTPUT); // Turn on WiFi 
    digitalWrite(LDO, HIGH);
    digitalWrite(LED, HIGH);
    digitalWrite(NPN_Q1, HIGH);
    digitalWrite(WIFI_CP_PD, HIGH);
  Serial.begin(9600);
  // reserve 200 bytes for the inputString:
  inputString.reserve(200);

  if (!sd.begin(SDcsPin, SPI_FULL_SPEED)) {
    sd.initErrorHalt();
  }
}

void loop() {
  // print the string when a newline arrives:
    while (Serial.available()) {
      // get the new byte:
      char inChar = (char)Serial.read();
      // add it to the inputString:
      inputString += inChar;
      if (inChar == '\n') {

        if (!myFile.open("file.txt", O_RDWR | O_CREAT | O_AT_END)) {
          sd.errorHalt("file.txt");
        }
        myFile.println(inputString);
        inputString = "";
        myFile.close();
      }
  }
}