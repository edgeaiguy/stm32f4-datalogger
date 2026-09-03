#ifndef RTC_H
#define RTC_H

#include <stdint.h>

typedef struct {
    uint16_t year;    /* full year, e.g. 2026 */
    uint8_t  month;   /* 1-12 */
    uint8_t  day;     /* 1-31 */
    uint8_t  hour;    /* 0-23 */
    uint8_t  min;
    uint8_t  sec;
    uint16_t ms;      /* derived from the sub-second register */
} rtc_time_t;

typedef enum {
    RTC_SRC_NONE = 0,
    RTC_SRC_LSE,      /* 32.768 kHz crystal — accurate, needs the part fitted */
    RTC_SRC_LSI,      /* internal RC — always available, drifts badly */
} rtc_src_t;

/* Bring the RTC up, seeding the calendar from the firmware build time only on a
 * cold start. Returns the clock source that actually started. */
rtc_src_t rtc_init(void);

rtc_src_t   rtc_source(void);
const char *rtc_source_name(void);

/* Non-zero if the calendar was already running when rtc_init() was called,
 * i.e. it survived the reset rather than being re-seeded. */
int rtc_was_running(void);

void rtc_now(rtc_time_t *t);

#endif // RTC_H
