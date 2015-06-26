#define BUFF_MAX 96

void debugPrintTime()
{
    DS3231_get(&t);
    char buff[BUFF_MAX];
    snprintf(buff, BUFF_MAX, "%02d:%02d:%02d,cap:%d,up:%d,",t.hour, t.min, t.sec, captureCount, uploadCount);
    Serial.println(buff);
}

void debugPrint(char *buf)
{
    #ifndef PRODUCTION
        Serial.println(buf);
    #endif
}

int freeRam () {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}

void testMemSetup () {
    //Serial.begin(57600);
    Serial.println(F("\n[memCheck]"));
    Serial.println(freeRam());
}

void blinkError(byte ERROR_TYPE) {
    digitalWrite(LDO, LOW);
    digitalWrite(WIFI_CP_PD, LOW);
    digitalWrite(NPN_Q1, LOW);
    while(1) {
        for(int x = 0 ; x < ERROR_TYPE ; x++) {
            digitalWrite(LED, HIGH);
            delay(100);
            digitalWrite(LED, LOW);
            delay(100);
        }
        delay(2000);
    }
}

void sysLog(const __FlashStringHelper* msg)
{
    // if (!sd.begin(SDcsPin, SPI_FULL_SPEED)) {
    //     Serial.println(F("initErrorHalt"));
    //     return;
    // }
    if (!myFile.open(sdSysLog, O_RDWR | O_CREAT | O_AT_END)) {
        myFile.print(t.year); myFile.print(t.mon); myFile.print(t.mday);myFile.print(F(" "));
        myFile.print(t.hour); myFile.print(F(":")); myFile.print(t.min); myFile.print(F(":")); myFile.print(t.sec);
        myFile.print(F("  info:")); myFile.println(msg);
    }
    myFile.close();
}

void sysLog(const char* msg)
{
    // if (!sd.begin(SDcsPin, SPI_FULL_SPEED)) {
    //     Serial.println(F("initErrorHalt"));
    //     return;
    // }
    if (!myFile.open(sdSysLog, O_RDWR | O_CREAT | O_AT_END)) {
        myFile.print(t.year); myFile.print(t.mon); myFile.print(t.mday);myFile.print(F(" "));
        myFile.print(t.hour); myFile.print(F(":")); myFile.print(t.min); myFile.print(F(":")); myFile.print(t.sec);
        myFile.print(F("  info:")); myFile.println(msg);
    }
    myFile.close();
}

void dateTime(uint16_t* date, uint16_t* time) {
    // User gets date and time from GPS or real-time
    // clock in real callback function
    // return date using FAT_DATE macro to format fields
    *date = FAT_DATE(t.year, t.mon, t.mday);
    // return time using FAT_TIME macro to format fields
    *time = FAT_TIME(t.hour, t.min, t.sec);
}
