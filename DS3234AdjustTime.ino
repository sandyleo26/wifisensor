
#include <SPI.h>
#include "ds3234.h"
#include "rtc_ds3234.h"

#define BUFF_MAX 256

const int cs = 10;              // chip select pin

uint8_t time[8];
char recv[BUFF_MAX];
unsigned int recv_size = 0;
unsigned long prev, interval = 5000;

void setup()
{
    Serial.begin(9600);
    DS3234_init(cs, DS3234_INTCN);
    memset(recv, 0, BUFF_MAX - 1);
    Serial.println("GET time");
    pinMode(5, OUTPUT);
    digitalWrite(5, HIGH);
    adjustTimeDS3234(__DATE__, __TIME__);

}

void loop()
{
    char buff[BUFF_MAX];
    unsigned long now = millis();
    int in;
    struct ts t;

    // show time once in a while
    if ((now - prev > interval) && (Serial.available() <= 0)) {
        DS3234_get(cs, &t);
        snprintf(buff, BUFF_MAX, "%d.%02d.%02d %02d:%02d:%02d", t.year,
             t.mon, t.mday, t.hour, t.min, t.sec);
        Serial.println(buff);
        prev = now;
    }

    if (Serial.available() > 0) {
        in = Serial.read();

        //snprintf(buf,200,"%d",in);
        //Serial.println(buf);

        if ((in == 10 || in == 13) && (recv_size > 0)) {
            parse_cmd(recv, recv_size);
            recv_size = 0;
            recv[0] = 0;
        } else if (in < 48 || in > 122) { // ~[0-9A-Za-z]
            // ignore 
        } else if (recv_size > BUFF_MAX - 2) {
            // drop
            recv_size = 0;
            recv[0] = 0;
        } else if (recv_size < BUFF_MAX - 2) {
            recv[recv_size] = in;
            recv[recv_size + 1] = 0;
            //snprintf(buf,200,"partial,%d: %s,%d,%d\n",recv_size,recv,recv[recv_size],in);
            //Serial.print(buf);
            recv_size += 1;
        }

    }
}

void parse_cmd(char *cmd, int cmdsize)
{
    uint8_t i;
    uint8_t reg_val;
    char buff[BUFF_MAX];
    struct ts t;

    //snprintf(buf, 200, "cmd was '%s' %d\n", cmd, cmdsize);
    //Serial.print(buf);

    // TssmmhhWDDMMYYYY aka set time
    if (cmd[0] == 84 && cmdsize == 16) {
        //T355720619112011
        t.sec = inp2toi(cmd, 1);
        t.min = inp2toi(cmd, 3);
        t.hour = inp2toi(cmd, 5);
        t.wday = inp2toi(cmd, 7);
        t.mday = inp2toi(cmd, 8);
        t.mon = inp2toi(cmd, 10);
        t.year = inp2toi(cmd, 12) * 100 + inp2toi(cmd, 14);
        DS3234_set(cs, t);
        Serial.println("OK");
    } else if (cmd[0] == 49 && cmdsize == 1) {  // "1" get alarm 1
        DS3234_get_a1(cs, &buff[0], 59);
        Serial.println(buff);
    } else if (cmd[0] == 50 && cmdsize == 1) {  // "2" get alarm 1
        DS3234_get_a2(cs, &buff[0], 59);
        Serial.println(buff);
    } else if (cmd[0] == 51 && cmdsize == 1) {  // "3" get aging register
        Serial.print("aging reg is ");
        Serial.println(DS3234_get_aging(cs), DEC);
    } else if (cmd[0] == 52 && cmdsize == 1) {  // "4" read sram
        int i;
        for (i = 0; i < BUFF_MAX-1; i++) {
            buff[i] = DS3234_get_sram_8b(cs, i);
        }
        for (i = 0; i < BUFF_MAX-1; i++) {
            Serial.print(buff[i], DEC);
            Serial.print(" ");
        }
    } else if (cmd[0] == 65 && cmdsize == 9) {  // "A" set alarm 1
        DS3234_set_creg(cs, DS3234_INTCN | DS3234_A1IE);
        //ASSMMHHDD
        for (i = 0; i < 4; i++) {
            time[i] = (cmd[2 * i + 1] - 48) * 10 + cmd[2 * i + 2] - 48; // ss, mm, hh, dd
        }
        boolean flags[5] = { 0, 0, 0, 0, 0 };
        DS3234_set_a1(cs, time[0], time[1], time[2], time[3], flags);
        DS3234_get_a1(cs, &buff[0], 59);
        Serial.println(buff);
    } else if (cmd[0] == 66 && cmdsize == 7) {  // "B" Set Alarm 2
        DS3234_set_creg(cs, DS3234_INTCN | DS3234_A2IE);
        //BMMHHDD
        for (i = 0; i < 4; i++) {
            time[i] = (cmd[2 * i + 1] - 48) * 10 + cmd[2 * i + 2] - 48; // mm, hh, dd
        }
        boolean flags[5] = { 0, 0, 0, 0 };
        DS3234_set_a2(cs, time[0], time[1], time[2], flags);
        DS3234_get_a2(cs, &buff[0], 59);
        Serial.println(buff);
    } else if (cmd[0] == 67 && cmdsize == 1) {  // "C" - get temperature register
        Serial.print("temperature reg is ");
        Serial.println(DS3234_get_treg(cs), DEC);
    } else if (cmd[0] == 68 && cmdsize == 1) {  // "D" - reset status register alarm flags
        reg_val = DS3234_get_sreg(cs);
        reg_val &= B11111100;
        DS3234_set_sreg(cs, reg_val);
    } else if (cmd[0] == 71 && cmdsize == 1) {  // "G" - set aging status register
        DS3234_set_aging(cs, 0);
    } else if (cmd[0] == 77 && cmdsize == 1) {  // "M" - write to sram
        int i;
        for (i = 0; i < BUFF_MAX-1; i++) {
            DS3234_set_sram_8b(cs, i, i);
        }
    } else if (cmd[0] == 83 && cmdsize == 1) {  // "S" - get status register
        Serial.print("status reg is ");
        Serial.println(DS3234_get_sreg(cs), DEC);
    } else {
        Serial.print("unknown command prefix ");
        Serial.println(cmd[0]);
        Serial.println(cmd[0], DEC);
    }
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
    DS3234_set(cs, t);
}
