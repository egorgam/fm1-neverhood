#!/usr/bin/env python3
"""Optional cached battery ADC API for the pinned fm1-base (MPL-2.0).

Only APP_BATTERY opts in. The input scan retains ownership when analog key
sensing is enabled. Never change the base template or touch device flash.
"""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
base=ROOT/'.deps/fm1-base'
def insert(file,old,new):
 p=base/file;s=p.read_text()
 if new in s:return
 assert s.count(old)==1,(file,'unexpected pinned platform source')
 p.write_text(s.replace(old,new))
insert('include/plat.h','uint32_t plat_master(void);',
       'int32_t plat_battery(void);           /* cached raw ADC ch3, 0..1023; -1 if unavailable (APP_BATTERY) */\nuint32_t plat_master(void);')
insert('device/plat_fm1.c','uint32_t plat_ms(void) { return fm1_ms; }', '''uint32_t plat_ms(void) { return fm1_ms; }
#ifdef APP_BATTERY
static int32_t battery_raw = -1;
static uint32_t battery_read_at;
int32_t plat_battery(void) { return battery_raw; }
static void battery_poll(void)
{
    /* No conversion may overlap the analog-key scanner's ownership. */
    if (!fm1_an.con && !fm1_an.pend &&
        (battery_raw < 0 || (uint32_t)(fm1_ms-battery_read_at) >= 1000u)) {
        int32_t a = fm1_adc_read(FM1_ADC_BATT);
        battery_read_at = fm1_ms;
        if (a >= 0) battery_raw = a;
    }
}
#endif''')
insert('device/main_fm1.c','    app_ui_init();\n\n    for (;;) {','''#ifdef APP_BATTERY
    battery_poll();
#endif
    app_ui_init();

    for (;;) {''')
insert('device/main_fm1.c',"        fm1b_dbg.stage = 2;                         /* (breadcrumbs: where a watchdog reset found the loop) */",'''#ifdef APP_BATTERY
        battery_poll();
#endif
        fm1b_dbg.stage = 2;                         /* (breadcrumbs: where a watchdog reset found the loop) */''')
insert('host/plat_host.c','static const char *plat_host_line = "";', '''/* The host has no physical battery; scenarios can inject its raw reading. */
int32_t plat_battery(void)
{
    const char *value = getenv("FM1_BATTERY_ADC");
    return value ? atoi(value) : 640;
}
static const char *plat_host_line = "";''')
