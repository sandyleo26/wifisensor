// this to be used with product to try

#include <SoftwareSerial.h>

// LED
#define LED 2
#define LDO 5
#define WIFI_CP_PD 3 // D3 control ESP8266
#define NPN_Q1 4 // D4 control DHT & SD Card
#define WIFI_BUF_MAX 64


#define SSID "NetComm 3521"
#define PASS "Thawifi2"
#define IP "69.195.124.239" // thingspeak.com
String GET = "GET /~meetisan/r.php?k=test3&d=";

void setup()
{
  Serial.begin(57600);

    pinMode(LDO, OUTPUT);
    pinMode(LED, OUTPUT);
    pinMode(NPN_Q1, OUTPUT); // Turn on DHT & SD
    pinMode(WIFI_CP_PD, OUTPUT); // Turn on WiFi 
    digitalWrite(LDO, HIGH);
    digitalWrite(LED, HIGH);
    digitalWrite(NPN_Q1, HIGH);
    digitalWrite(WIFI_CP_PD, HIGH);

  sendDebug("AT");
  delay(5000);
  if(Serial.find("OK")){
    connectWiFi();
  }
}

void loop(){
  updateTemp("1,2015-02-27,01:44:33,38.5,66.6$");
  delay(20000);
}


void updateTemp(String tenmpF){
  String cmd = "AT+CIPSTART=\"TCP\",\"";
  cmd += IP;
  cmd += "\",80";
  sendDebug(cmd);
  delay(2000);
  if(Serial.find("Error")){
    return;
  }
  cmd = GET;
  cmd += tenmpF;
  cmd += "\r\n";
  Serial.print("AT+CIPSEND=");
  Serial.println(cmd.length());
  if(Serial.find(">")){
    Serial.print(cmd);
  }else{
    sendDebug("AT+CIPCLOSE");
  }
  if(Serial.find("OK")){
  }else{
  }
}

// boolean connectWiFi(){
//   mySerial.println("AT+CWMODE=1");
//   delay(2000);
//   String cmd="AT+CWJAP=\"";
//   cmd+=SSID;
//   cmd+="\",\"";
//   cmd+=PASS;
//   cmd+="\"";
//   sendDebug(cmd);
//   delay(10000);
//   if(mySerial.find("OK")){
//     Serial.println("RECEIVED: OK");
//     return true;
//   }else{
//     Serial.println("RECEIVED: Error");
//     return false;
//   }
// }

boolean connectWiFi() {
    uint8_t i = 0;
    uint8_t n = 0;
    uint8_t j = 0;
    char buffer[WIFI_BUF_MAX];
    Serial.println("AT+CWMODE=1");
    delay(2000);
    String cmd="AT+CWJAP=\"";
    cmd+=SSID;
    cmd+="\",\"";
    cmd+=PASS;
    cmd+="\"";
    sendDebug(cmd);
    while (1) {
        if (i++>100) return false;
        delay(1000);
        if ((n = Serial.available()) != 0) {
            j = 0;
            while (j<WIFI_BUF_MAX-1)
                buffer[j++] = Serial.read();
            buffer[WIFI_BUF_MAX-1] = '\0';
            Serial.println(buffer);
            if (strstr(buffer, "OK")) {
                //Serial.print(i);
                //Serial.println(" RECEIVED: OK");
                return true;
            }
            else if (strstr(buffer, "FAIL")) {
                //Serial.print(i);
                //Serial.println("RECEIVED: FAIL");
            }
        }
    }
    return true;
}

void sendDebug(String cmd){
  //Serial.print("SEND: ");
  Serial.println(cmd);
}


