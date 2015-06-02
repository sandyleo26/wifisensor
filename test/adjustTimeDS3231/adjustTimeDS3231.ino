
// during an alarm the INT pin of the RTC is pulled low
//
// this is handy for minimizing power consumption for sensor-like devices, 
// since they can be started up by this pin on given time intervals.


#include <Wire.h>
#include "ds3231.h"

#define BUFF_MAX 64

uint8_t sleep_period = 5;       // the sleep interval in minutes between 2 consecutive alarms

// how often to refresh the info on stdout (ms)
unsigned long prev = 5000, interval = 5000;

#define LDO 5

void setup()
{
    Serial.begin(57600);
    pinMode(LDO, OUTPUT);
    digitalWrite(LDO, HIGH);
    Wire.begin();
    DS3231_init(DS3231_INTCN);
    adjustTime(__DATE__, __TIME__);

}

void loop()
{
    char buff[BUFF_MAX];
    unsigned long now = millis();
    struct ts t;
    DS3231_get(&t);

    snprintf(buff, BUFF_MAX, "%d.%02d.%02d %02d:%02d:%02d", t.year,
             t.mon, t.mday, t.hour, t.min, t.sec);

    Serial.println(buff);
    
    delay(5000);
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

void adjustTimeDS3234 (const char* date, const char* time) {
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
