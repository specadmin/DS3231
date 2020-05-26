//-----------------------------------------------------------------------------
#include "ds3231.h"
#include "../avr-debug/debug.h"
#include "../avr-twi/twi.h"
//-----------------------------------------------------------------------------
#define RTC_I2C_ADDR            0x68    // DS3231 fixed I2C address
#define RTC_CR_TIME             0x00
#define RTC_CR_SECONDS          0x00
#define RTC_CR_MINUTES          0x01
#define RTC_CR_HOURS            0x02
#define RTC_CR_WDAY             0x03
#define RTC_CR_DATE             0x04
#define RTC_CR_DAY              0x04
#define RTC_CR_MONTH            0x05
#define RTC_CR_YEAR             0x06
#define RTC_CR_ALARM1           0x07
#define RTC_CR_ALARM1_SECONDS   0x07
#define RTC_CR_ALARM1_MINUTES   0x08
#define RTC_CR_ALARM1_HOURS     0x09
#define RTC_CR_ALARM1_DAY       0x0A
#define RTC_CR_ALARM2           0x0B
#define RTC_CR_ALARM2_MINUTES   0x0B
#define RTC_CR_ALARM2_HOURS     0x0C
#define RTC_CR_ALARM2_DAY       0x0D
#define RTC_CR_CONFIG           0x0E
#define A1IE                    0
#define A2IE                    1
#define INTCN                   2
#define RS1                     3
#define RS2                     4
#define CONV                    5
#define BBSQW                   6
#define EOSC                    7
#define RTC_CR_STATUS           0x0F
#define A1F                     0
#define A2F                     1
#define BSY                     2
#define EN32KHZ                 3
#define OSF                     7
#define RTC_CR_AGING            0x10
#define RTC_CR_TEMPERATURE      0x11
#define RTC_CR_HTEMP            0x11
#define RTC_CR_LTEMP            0x12
#define BCD(var)                (((var / 10) << 4) + var % 10)
//-----------------------------------------------------------------------------
static BYTE s_buf[8];
static BYTE* s_ptr;
static BYTE s_rx_bytes;
static BYTE s_alarm_flags;
static void (*s_callback)(BYTE result, BYTE* data);
static void (*s_finally)(BYTE result);


//---------------------------------------------------------------------------//
//                           Synchronous methods                             //
//---------------------------------------------------------------------------//
__inline BYTE RTC_receive(BYTE address, BYTE size, BYTE* buf)
{
    BYTE result = twi_send(RTC_I2C_ADDR, 1, &address);
    if(result)
    {
        return result;
    }
    return twi_receive(RTC_I2C_ADDR, size, buf);
}
//-----------------------------------------------------------------------------
BYTE DS3231RTC::get_time_bcd(BYTE time[3])
{
    return RTC_receive(RTC_CR_TIME, 3, time);
}
//-----------------------------------------------------------------------------
BYTE DS3231RTC::get_time_bcd_HM(BYTE time[2])
{
    return RTC_receive(RTC_CR_MINUTES, 1, time);
}
//-----------------------------------------------------------------------------
BYTE DS3231RTC::get_datetime_bcd(BYTE time[7])
{
    return RTC_receive(RTC_CR_TIME, 7, time);
}
//-----------------------------------------------------------------------------
BYTE DS3231RTC::get_date_bcd(BYTE time[3])
{
    return RTC_receive(RTC_CR_DATE, 3, time);
}
//-----------------------------------------------------------------------------
int DS3231RTC::get_temperature()
{
    RTC_receive(RTC_CR_TEMPERATURE, 2, s_buf);
    return (int)(s_buf[0] << 2 | s_buf[1] >> 6);
}
//-----------------------------------------------------------------------------
BYTE DS3231RTC::force_temp_conversion()
{
    RTC_receive(RTC_CR_STATUS, 1, s_buf);
    if(test_bit(s_buf[0], BSY))
    {
        return 0xFF;
    }
    twi_wait();
    s_buf[0] = RTC_CR_CONFIG;
    RTC_receive(RTC_CR_CONFIG, 1, s_buf + 1);
    set_bit(s_buf[1], CONV);
    twi_wait();
    return twi_send(RTC_I2C_ADDR, 2, s_buf);
}
//-----------------------------------------------------------------------------
BYTE DS3231RTC::disable_32kHz_output()
{
    s_buf[0] = RTC_CR_STATUS;
    twi_wait();
    RTC_receive(RTC_CR_STATUS, 1, s_buf + 1);
    clr_bit(s_buf[1], EN32KHZ);
    twi_wait();
    return twi_send(RTC_I2C_ADDR, 2, s_buf);
}
//-----------------------------------------------------------------------------
BYTE DS3231RTC::enable_32kHz_output()
{
    s_buf[0] = RTC_CR_STATUS;
    twi_wait();
    RTC_receive(RTC_CR_STATUS, 1, s_buf + 1);
    set_bit(s_buf[1], EN32KHZ);
    twi_wait();
    return twi_send(RTC_I2C_ADDR, 2, s_buf);
}
//-----------------------------------------------------------------------------
BYTE DS3231RTC::enable_run_on_battery()
{
    s_buf[0] = RTC_CR_CONFIG;
    twi_wait();
    RTC_receive(RTC_CR_CONFIG, 1, s_buf + 1);
    clr_bit(s_buf[1], EOSC);
    twi_wait();
    return twi_send(RTC_I2C_ADDR, 2, s_buf);
}
//-----------------------------------------------------------------------------
BYTE DS3231RTC::disable_run_on_battery()
{
    s_buf[0] = RTC_CR_CONFIG;
    twi_wait();
    RTC_receive(RTC_CR_CONFIG, 1, s_buf + 1);
    set_bit(s_buf[1], EOSC);
    twi_wait();
    return twi_send(RTC_I2C_ADDR, 2, s_buf);
}
//-----------------------------------------------------------------------------
BYTE DS3231RTC::enable_SQW_output(enum SQWFrequency freq, bool run_on_battery)
{
    s_buf[0] = RTC_CR_CONFIG;
    twi_wait();
    RTC_receive(RTC_CR_CONFIG, 1, s_buf + 1);
    s_buf[1] = (s_buf[1] & 0xA3 ) | (run_on_battery << BBSQW) | (freq << 3);
    twi_wait();
    return twi_send(RTC_I2C_ADDR, 2, s_buf);
}
//-----------------------------------------------------------------------------
BYTE DS3231RTC::disable_SQW_output()
{
    s_buf[0] = RTC_CR_CONFIG;
    twi_wait();
    RTC_receive(RTC_CR_CONFIG, 1, s_buf + 1);
    set_bit(s_buf[1], INTCN);
    twi_wait();
    return twi_send(RTC_I2C_ADDR, 2, s_buf);
}//-----------------------------------------------------------------------------
bool DS3231RTC::time_lost()
{
    s_buf[0] = RTC_CR_STATUS;
    twi_wait();
    RTC_receive(RTC_CR_STATUS, 1, s_buf + 1);
    return test_bit(s_buf[1], OSF);
}


//---------------------------------------------------------------------------//
//                           Asynchronous methods                            //
//---------------------------------------------------------------------------//
static void RTC_rx_completed(BYTE result)
{
    s_callback(result, s_buf);
}
//-----------------------------------------------------------------------------
static void RTC_tx_completed(BYTE result)
{
    if(result)
    {
        s_callback(result, NULL);
    }
    else
    {
        twi_wait();
        twi_receive(RTC_I2C_ADDR, s_rx_bytes, s_ptr, &RTC_rx_completed);
    }
}
//-----------------------------------------------------------------------------
static void after_alarms_cleared(__unused BYTE result)
{
    s_finally(s_alarm_flags);
}
//-----------------------------------------------------------------------------
static void alarms_cleaner(__unused BYTE result, __unused BYTE* buf)
{
    s_alarm_flags = s_buf[1] & 0x03;
    clr_bits(s_buf[1], A1F, A2F);
    twi_wait();
    twi_send(RTC_I2C_ADDR, 2, s_buf, &after_alarms_cleared);
}
//-----------------------------------------------------------------------------
__inline BYTE RTC_receive_async(BYTE address, BYTE size, BYTE* buf, void (*callback)(BYTE result, BYTE*))
{
    s_callback = callback;
    s_rx_bytes = size;
    s_ptr = buf;
    twi_wait();
    return twi_send(RTC_I2C_ADDR, 1, &address, &RTC_tx_completed);
}
//-----------------------------------------------------------------------------
void DS3231RTC::clear_alarm_events(void (*callback)(BYTE alarms))
{
    s_finally = callback;
    twi_wait();
    s_buf[0] = RTC_CR_STATUS;
    RTC_receive_async(RTC_CR_STATUS, 1, s_buf + 1, &alarms_cleaner);
}
//-----------------------------------------------------------------------------
BYTE DS3231RTC::get_time_bcd(void (*callback)(BYTE result, BYTE time[3]))
{
    return RTC_receive_async(RTC_CR_TIME, 3, s_buf, callback);
}
//-----------------------------------------------------------------------------
BYTE DS3231RTC::get_time_bcd_HM(void (*callback)(BYTE result, BYTE time[2]))
{
    return RTC_receive_async(RTC_CR_MINUTES, 1, s_buf, callback);
}
//-----------------------------------------------------------------------------
BYTE DS3231RTC::get_datetime_bcd(void (*callback)(BYTE result, BYTE time[7]))
{
    return RTC_receive_async(RTC_CR_TIME, 7, s_buf, callback);
}
//-----------------------------------------------------------------------------
BYTE DS3231RTC::get_date_bcd(void (*callback)(BYTE result, BYTE time[3]))
{
    return RTC_receive_async(RTC_CR_DATE, 3, s_buf, callback);
}


//---------------------------------------------------------------------------//
//                             Universal methods                             //
//---------------------------------------------------------------------------//
__inline void clear_lost_flag()
{
    s_buf[0] = RTC_CR_STATUS;
    twi_wait();
    RTC_receive(RTC_CR_STATUS, 1, s_buf + 1);
    clr_bit(s_buf[1], OSF);
    twi_wait();
    twi_send(RTC_I2C_ADDR, 2, s_buf);
}
//-----------------------------------------------------------------------------
BYTE DS3231RTC::set_time(BYTE minutes, BYTE hours, void (*callback)(BYTE result))
{
    s_buf[0] = RTC_CR_TIME;
    s_buf[1] = 0;
    s_buf[2] = BCD(minutes);
    s_buf[3] = BCD(hours);
    BYTE result = twi_send(RTC_I2C_ADDR, 4, s_buf, callback);
    if(!result)
    {
        clear_lost_flag();
    }
    return result;
}
//-----------------------------------------------------------------------------
BYTE DS3231RTC::set_time(BYTE seconds, BYTE minutes, BYTE hours, void (*callback)(BYTE result))
{
    s_buf[0] = RTC_CR_TIME;
    s_buf[1] = BCD(seconds);
    s_buf[2] = BCD(minutes);
    s_buf[3] = BCD(hours);
    BYTE result = twi_send(RTC_I2C_ADDR, 4, s_buf, callback);
    if(!result)
    {
        clear_lost_flag();
    }
    return result;
}
//-----------------------------------------------------------------------------
BYTE DS3231RTC::reset_seconds(void (*callback)(BYTE result))
{
    s_buf[0] = RTC_CR_TIME;
    s_buf[1] = 0;
    return twi_send(RTC_I2C_ADDR, 2, s_buf, callback);
}
//-----------------------------------------------------------------------------
BYTE DS3231RTC::set_datetime(BYTE datetime[6], void (*callback)(BYTE result))
{
    s_buf[0] = RTC_CR_TIME;
    s_buf[1] = BCD(datetime[SEC]);
    s_buf[2] = BCD(datetime[MIN]);
    s_buf[3] = BCD(datetime[HOUR]);
    s_buf[4] = 1;   // TODO: calculate the day of the week
    s_buf[5] = BCD(datetime[DAY]);
    s_buf[6] = BCD(datetime[MONTH]);
    s_buf[7] = BCD(datetime[YEAR]);
    BYTE result = twi_send(RTC_I2C_ADDR, 8, s_buf, callback);
    if(!result)
    {
        clear_lost_flag();
    }
    return result;
}


//---------------------------------------------------------------------------//
//                                   Alarm 1                                 //
//---------------------------------------------------------------------------//
BYTE DS3231Alarm1::unset()
{
    s_buf[0] = RTC_CR_ALARM1;
    s_buf[1] = 0;
    s_buf[2] = 0;
    s_buf[3] = 0;
    s_buf[4] = 0;
    twi_wait();
    BYTE result = twi_send(RTC_I2C_ADDR, 5, s_buf);
    if(result)
    {
        return result;
    }
    s_buf[0] = RTC_CR_CONFIG;
    twi_wait();
    RTC_receive(RTC_CR_CONFIG, 2, s_buf + 1);
    clr_bit(s_buf[1], A1IE);
    clr_bit(s_buf[2], A1F);
    twi_wait();
    return twi_send(RTC_I2C_ADDR, 3, s_buf);
}
//-----------------------------------------------------------------------------
__inline BYTE set_alarm1(BYTE seconds, BYTE minutes, BYTE hours, BYTE day)
{
    s_buf[0] = RTC_CR_ALARM1;
    s_buf[1] = seconds;
    s_buf[2] = minutes;
    s_buf[3] = hours;
    s_buf[4] = day;
    twi_wait();
    BYTE result = twi_send(RTC_I2C_ADDR, 5, s_buf);
    if(result)
    {
        return result;
    }
    s_buf[0] = RTC_CR_CONFIG;
    twi_wait();
    RTC_receive(RTC_CR_CONFIG, 2, s_buf + 1);
    set_bit(s_buf[1], A1IE);
    clr_bit(s_buf[2], A1F);
    twi_wait();
    return twi_send(RTC_I2C_ADDR, 3, s_buf);
}
//-----------------------------------------------------------------------------
BYTE DS3231Alarm1::set_every_second()
{
    return set_alarm1(0x81, 0x81, 0x81, 0x81);
}
//-----------------------------------------------------------------------------
BYTE DS3231Alarm1::set(BYTE seconds)
{
    return set_alarm1(BCD(seconds), 0x81, 0x81, 0x81);
}
//-----------------------------------------------------------------------------
BYTE DS3231Alarm1::set(BYTE seconds, BYTE minutes)
{
    return set_alarm1(BCD(seconds), BCD(minutes), 0x81, 0x81);
}
//-----------------------------------------------------------------------------
BYTE DS3231Alarm1::set(BYTE seconds, BYTE minutes, BYTE hours)
{
    return set_alarm1(BCD(seconds), BCD(minutes), BCD(hours), 0x81);
}
//-----------------------------------------------------------------------------
BYTE DS3231Alarm1::set(BYTE seconds, BYTE minutes, BYTE hours, BYTE day, bool wday)
{
    day = BCD(day) | (wday << 6);
    return set_alarm1(BCD(seconds), BCD(minutes), BCD(hours), day);
}


//---------------------------------------------------------------------------//
//                                   Alarm 2                                 //
//---------------------------------------------------------------------------//
BYTE DS3231Alarm2::unset()
{
    s_buf[0] = RTC_CR_ALARM2;
    s_buf[1] = 0;
    s_buf[2] = 0;
    s_buf[3] = 0;
    twi_wait();
    BYTE result = twi_send(RTC_I2C_ADDR, 4, s_buf);
    if(result)
    {
        return result;
    }
    s_buf[0] = RTC_CR_CONFIG;
    twi_wait();
    RTC_receive(RTC_CR_CONFIG, 2, s_buf + 1);
    clr_bit(s_buf[1], A2IE);
    clr_bit(s_buf[2], A2F);
    twi_wait();
    return twi_send(RTC_I2C_ADDR, 3, s_buf);
}
//-----------------------------------------------------------------------------
__inline BYTE set_alarm2(BYTE minutes, BYTE hours, BYTE day)
{
    s_buf[0] = RTC_CR_ALARM2;
    s_buf[1] = minutes;
    s_buf[2] = hours;
    s_buf[3] = day;
    twi_wait();
    BYTE result = twi_send(RTC_I2C_ADDR, 4, s_buf);
    if(result)
    {
        return result;
    }
    s_buf[0] = RTC_CR_CONFIG;
    twi_wait();
    RTC_receive(RTC_CR_CONFIG, 2, s_buf + 1);
    set_bit(s_buf[1], A2IE);
    clr_bit(s_buf[2], A2F);
    twi_wait();
    return twi_send(RTC_I2C_ADDR, 3, s_buf);
}
//-----------------------------------------------------------------------------
BYTE DS3231Alarm2::set_every_minute()
{
    return set_alarm2(0x81, 0x81, 0x81);
}
//-----------------------------------------------------------------------------
BYTE DS3231Alarm2::set(BYTE minutes)
{
    return set_alarm2(BCD(minutes), 0x81, 0x81);
}
//-----------------------------------------------------------------------------
BYTE DS3231Alarm2::set(BYTE minutes, BYTE hours)
{
    return set_alarm2(BCD(minutes), BCD(hours), 0x81);
}
//-----------------------------------------------------------------------------
BYTE DS3231Alarm2::set(BYTE minutes, BYTE hours, BYTE day, bool wday)
{
    day = BCD(day) | (wday << 6);
    return set_alarm2(BCD(minutes), BCD(hours), day);
}
//-----------------------------------------------------------------------------

DS3231RTC RTC;
