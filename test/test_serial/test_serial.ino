#include <SoftwareSerial.h>

SoftwareSerial mySerial(10, 11); // RX, TX


String inputString = "";         // a string to hold incoming data
String myInputString = "";

void setup() {
  // initialize serial:
    mySerial.begin(9600);
  Serial.begin(9600);
  // reserve 200 bytes for the inputString:
  inputString.reserve(200);
  myInputString.reserve(200);
}

void loop() {
  // print the string when a newline arrives:
    while (Serial.available()) {
    // get the new byte:
    char inChar = (char)Serial.read();
    // add it to the inputString:
    inputString += inChar;
    // if the incoming character is a newline, set a flag
    // so the main loop can do something about it:
    if (inChar == '\n') {
      mySerial.println(inputString);
      // clear the string:
      inputString = "";

      while (mySerial.available()) {
        char myInChar = (char)mySerial.read();
        myInputString += myInChar;
        if (myInChar == '\n') {
          Serial.println(myInputString);
          myInputString = "";
        }
      }
    }
  }
}