/*
 * Minimale <time.h> voor PSn00bSDK.
 *
 * PSn00bSDK levert geen time.h mee, maar peanut_gb.h doet #include <time.h>
 * en gebruikt alleen `struct tm` (in gb_set_rtc). Dit bestand is puur een
 * shim zodat peanut_gb.h ONGEWIJZIGD kan blijven. De RTC wordt in deze
 * versie niet gebruikt (Pokemon Red heeft geen RTC).
 */
#ifndef PS1_COMPAT_TIME_H
#define PS1_COMPAT_TIME_H

struct tm {
	int tm_sec;
	int tm_min;
	int tm_hour;
	int tm_mday;
	int tm_mon;
	int tm_year;
	int tm_wday;
	int tm_yday;
	int tm_isdst;
};

#endif
