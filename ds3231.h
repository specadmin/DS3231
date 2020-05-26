#ifndef DS3231_H
#define DS3231_H
//-----------------------------------------------------------------------------
#include "../avr-misc/avr-misc.h"
//-----------------------------------------------------------------------------
enum
{
    SEC, MIN, HOUR, DAY, MONTH, YEAR
};
//-----------------------------------------------------------------------------
enum SQWFrequency
{
    RTC_SQW_FREQ_1HZ,
    RTC_SQW_FREQ_1024HZ,
    RTC_SQW_FREQ_4096HZ,
    RTC_SQW_FREQ_8192HZ
};
//-----------------------------------------------------------------------------
struct DS3231Alarm1
{
    BYTE set_every_second();
    BYTE set(BYTE seconds);
    BYTE set(BYTE seconds, BYTE minutes);
    BYTE set(BYTE seconds, BYTE minutes, BYTE hours);
    BYTE set(BYTE seconds, BYTE minutes, BYTE hours, BYTE day, bool wday = false);
    BYTE unset();
};
//-----------------------------------------------------------------------------
struct DS3231Alarm2
{
    BYTE set_every_minute();
    BYTE set(BYTE minutes);
    BYTE set(BYTE minutes, BYTE hours);
    BYTE set(BYTE minutes, BYTE hours, BYTE day, bool week_day = false);
    BYTE unset();
};
//-----------------------------------------------------------------------------
struct DS3231RTC
{
    BYTE get_time_bcd(BYTE time[3]);
    BYTE get_time_bcd(void (*callback)(BYTE result, BYTE time[3]));

    BYTE get_time_bcd_HM(BYTE time[2]);
    BYTE get_time_bcd_HM(void (*callback)(BYTE result, BYTE time[2]));

    BYTE get_datetime_bcd(BYTE datetime[7]);
    BYTE get_datetime_bcd(void (*callback)(BYTE result, BYTE time[7]));

    BYTE get_date_bcd(BYTE datetime[3]);
    BYTE get_date_bcd(void (*callback)(BYTE result, BYTE time[3]));

    BYTE set_time(BYTE seconds, BYTE minutes, BYTE hours, void (*callback)(BYTE result) = NULL);
    BYTE set_time(BYTE minutes, BYTE hours, void (*callback)(BYTE result) = NULL);
    BYTE reset_seconds(void (*callback)(BYTE result) = NULL);

    BYTE set_datetime(BYTE datetime[6], void (*callback)(BYTE result) = NULL);

    bool time_lost();

    BYTE force_temp_conversion();
    int  get_temperature();

    BYTE enable_run_on_battery();
    BYTE disable_run_on_battery();
    BYTE enable_32kHz_output();
    BYTE disable_32kHz_output();
    BYTE enable_SQW_output(enum SQWFrequency freq, bool run_on_battery = false);
    BYTE disable_SQW_output();

    DS3231Alarm1 alarm1;
    DS3231Alarm2 alarm2;

    void clear_alarm_events(void (*callback)(BYTE alarms));
};
//-----------------------------------------------------------------------------
extern DS3231RTC RTC;
//-----------------------------------------------------------------------------
#endif
