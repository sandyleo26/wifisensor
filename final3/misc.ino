#define BUFF_MAX 96

void debugPrintTime()
{
    DS3231_get(&t);
    char buff[BUFF_MAX];
    snprintf(buff, BUFF_MAX, "%02d:%02d:%02d,cap:%d,up:%d,",t.hour, t.min, t.sec, captureCount, uploadedLines);
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

void blinkError(uint8_t ERROR_TYPE) {
    while(1) {
        blink(ERROR_TYPE);
        delay(3000);
    }
}

void blink(uint8_t ERROR_TYPE) {
    if (ERROR_TYPE <= 4) {
        for(int x = 0 ; x < ERROR_TYPE ; x++) {
            digitalWrite(LED, LOW);
            delay(100);
            digitalWrite(LED, HIGH);
            delay(100);
        }
    } else {
        for(int x = 0 ; x < ERROR_TYPE ; x++) {
            digitalWrite(LED, LOW);
            delay(500);
            digitalWrite(LED, HIGH);
            delay(500);
        }
    }
}

void blinkAndShutDown(uint8_t ERROR_TYPE)
{
    int i = 0;
    while(i++<10) {
        for(int x = 0 ; x < ERROR_TYPE ; x++) {
            digitalWrite(LED, LOW);
            delay(100);
            digitalWrite(LED, HIGH);
            delay(100);
        }
        delay(3000);
    }
    digitalWrite(NPN_Q1, LOW);
    digitalWrite(WIFI_CP_PD, LOW);
    digitalWrite(LDO, LOW);
    chip.turnOffADC();
    chip.turnOffSPI();
    chip.turnOffWDT();
    chip.turnOffBOD();
    delay(500);
    chip.goodNight();
}

void sysLog(const __FlashStringHelper* msg)
{
    // if (!sd.begin(SDcsPin, SPI_FULL_SPEED)) {
    //     Serial.println(F("initErrorHalt"));
    //     return;
    // }
    if (!myFile.open(sdDebugLog, O_RDWR | O_CREAT | O_AT_END)) {
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
    if (!myFile.open(sdDebugLog, O_RDWR | O_CREAT | O_AT_END)) {
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

String getAllEEPROM()
{ 
    char val;
    int i = 0;
    String str;
    val = EEPROM.read(i++);
    while (val != '$') {
        str += val;
        val = EEPROM.read(i++);
    }
    return str;
}

void generateNewLogName(char *tmpName, char c)
{
    //L150710a.csv
    tmpName[0] = 'L';
    int2Str(tmpName+1, t.year-2000);
    if (t.mon < 10) {
        tmpName[3] = '0';
        int2Str(tmpName+4, t.mon);
    } else {
        int2Str(tmpName+3, t.mon);
    }
    if (t.mday < 10) {
        tmpName[5] = '0';
        int2Str(tmpName+6, t.mday);
    } else {
        int2Str(tmpName+5, t.mday);
    }
    tmpName[7] = c;
    tmpName[8] = '.';
    tmpName[9] = 'c';
    tmpName[10] = 's';
    tmpName[11] = 'v';
    tmpName[12] = '\0';
}

void int2Str(char *buf, uint8_t i)
{
    uint8_t L = 0;
    char c;
    char b;  // lower-byte of i
    b = char( i );
    if( b > 9 ) {
        c = b < 30 ? ( b < 20 ? 1 : 2 ) : 3;
        buf[L++] = c + 48;
        b -= c * 10;
    }
    // last digit
    buf[L++] = b + 48;
    // null terminator
    buf[L] = 0;  
}
