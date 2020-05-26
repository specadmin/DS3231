//-----------------------------------------------------------------------------
#include <stdlib.h>
#include "lib/avr-misc/avr-misc.h"
#include "lib/avr-debug/debug.h"
#include "lib/avr-twi/twi.h"
#include "lib/DS3231/ds3231.h"
//-----------------------------------------------------------------------------
void time_changed(__unused BYTE result, BYTE time[3])
{
    BYTE hours = time[HOUR];
    BYTE minutes = time[MIN];
    BYTE seconds = time[SEC];
    DHEX8(3, hours, minutes, seconds);
}
//-----------------------------------------------------------------------------
int main()
{
    DEBUG_INIT();
    twi_init();

    // enabe pn-change interrupt
    set_bit(PCICR, PCIE0);
    set_bit(PCMSK0, PCINT0);

    enable_interrupts();

    RTC.enable_run_on_battery();
    RTC.disable_32kHz_output();
    RTC.disable_SQW_output();

    // RTC.set_time(38, 10);

    RTC.alarm1.unset();
    RTC.alarm2.set_every_minute();

    while(1);

    return 0;
}
//-----------------------------------------------------------------------------
void alarms_receiever(BYTE alarms)
{
    DVAR(alarms);
    RTC.get_time_bcd(time_changed);
}
//-----------------------------------------------------------------------------
ISR(PCINT0_vect)
{
    if(!test_bit(PINA, 0))
    {
        RTC.clear_alarm_events(&alarms_receiever);
    }
}
//-----------------------------------------------------------------------------
