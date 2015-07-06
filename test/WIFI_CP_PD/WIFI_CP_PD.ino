//****************************************************************

// Smart TH WiFi Sensor code

//****************************************************************
#define LED 2
#define WIFI_CP_PD 3 // D3 control ESP8266
#define NPN 4 // D4 control DHT & SD Card
#define LDO 5
#define SDcsPin 9 // D9

// setup ****************************************************************
void setup()
{
    Serial.begin(9600);
    delay(5);
    Serial.println(F(__DATE__));
    pinMode(LDO, OUTPUT);
    pinMode(LED, OUTPUT);
    pinMode(NPN, OUTPUT); // Turn on DHT & SD
    pinMode(WIFI_CP_PD, OUTPUT); // Turn on WiFi 
    pinMode(SDcsPin, OUTPUT); 
    digitalWrite(LDO, HIGH);
    digitalWrite(LED, HIGH);
    digitalWrite(NPN, HIGH);
    digitalWrite(WIFI_CP_PD, LOW);
}

void loop()
{
    Serial.println("loop");
    delay(2000);
    digitalWrite(WIFI_CP_PD, HIGH);
    delay(5000);
}
