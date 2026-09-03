/* rtc.c — wall-clock time from the STM32F407's RTC.
 *
 * Three separate locks guard this peripheral, and missing any of them makes
 * writes fail silently rather than fault:
 *   1. APB1ENR.PWREN  — the power interface clock must be running at all
 *   2. PWR_CR.DBP     — backup-domain write protection, gates RCC_BDCR and RTC
 *   3. RTC_WPR        — the RTC's own lock, opened with 0xCA then 0x53
 */
#include <stdint.h>
#include "stm32f407xx.h"
#include "systick.h"
#include "rtc.h"

/* Marks the calendar as ours and already seeded. The backup domain survives a
 * system reset, so this is what stops every reflash from resetting the clock. */
#define RTC_BKP_MAGIC   0x32F4C10CUL

#define LSE_TIMEOUT_MS  2000   /* a crystal that is not fitted never reports ready */
#define LSI_TIMEOUT_MS    10
#define SYNC_TIMEOUT_MS  200

/* 1 Hz = source / ((PREDIV_A + 1) * (PREDIV_S + 1)) */
#define PRER_LSE  ((127U << 16) | 255U)   /* 32768 / 128 / 256 */
#define PRER_LSI  ((127U << 16) | 249U)   /* 32000 / 128 / 250, nominal only */

static rtc_src_t src;
static int was_running;

#define BCD2BIN(v)  ((((v) >> 4) & 0xF) * 10 + ((v) & 0xF))
#define BIN2BCD(v)  ((((v) / 10) << 4) | ((v) % 10))

static void wpr_unlock(void) { RTC_WPR = 0xCA; RTC_WPR = 0x53; }
static void wpr_lock(void)   { RTC_WPR = 0xFF; }

static int wait_flag(volatile uint32_t *reg, uint32_t mask, int want, uint32_t ms) {
    uint32_t deadline = systick_millis() + ms;
    while ((int32_t)(systick_millis() - deadline) < 0) {
        int now = (*reg & mask) != 0;
        if (now == want) return 0;
    }
    return -1;
}

/* __DATE__ is "Aug 26 2026", __TIME__ is "13:32:07". Parsed at runtime because
 * that is far less code than doing it in the preprocessor, and it runs once. */
static void build_timestamp(rtc_time_t *t) {
    static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    const char *d = __DATE__;
    const char *c = __TIME__;

    t->month = 1;
    for (int i = 0; i < 12; i++) {
        if (d[0] == months[i*3] && d[1] == months[i*3+1] && d[2] == months[i*3+2]) {
            t->month = (uint8_t)(i + 1);
            break;
        }
    }
    t->day   = (uint8_t)((d[4] == ' ' ? 0 : (d[4] - '0') * 10) + (d[5] - '0'));
    t->year  = (uint16_t)((d[7]-'0')*1000 + (d[8]-'0')*100 + (d[9]-'0')*10 + (d[10]-'0'));
    t->hour  = (uint8_t)((c[0]-'0')*10 + (c[1]-'0'));
    t->min   = (uint8_t)((c[3]-'0')*10 + (c[4]-'0'));
    t->sec   = (uint8_t)((c[6]-'0')*10 + (c[7]-'0'));
    t->ms    = 0;
}

/* Get the named oscillator running. LSEON lives in RCC_BDCR, inside the backup
 * domain, so it survives a system reset. LSION lives in RCC_CSR, which does NOT
 * - every reset clears it. A warm start therefore has to switch LSI back on or
 * the calendar loses its clock and RSF never sets, which reads back as a
 * stopped clock showing 2000-01-01. */
static int ensure_running(rtc_src_t s) {
    if (s == RTC_SRC_LSE) {
        RCC_BDCR |= RCC_BDCR_LSEON;
        return wait_flag(&RCC_BDCR, RCC_BDCR_LSERDY, 1, LSE_TIMEOUT_MS);
    }
    if (s == RTC_SRC_LSI) {
        RCC_CSR |= RCC_CSR_LSION;
        return wait_flag(&RCC_CSR, RCC_CSR_LSIRDY, 1, LSI_TIMEOUT_MS);
    }
    return -1;
}

/* Start LSE if the crystal is fitted, otherwise fall back to the internal RC. */
static rtc_src_t start_clock_source(void) {
    if (ensure_running(RTC_SRC_LSE) == 0) {
        RCC_BDCR = (RCC_BDCR & ~RCC_BDCR_RTCSEL_MASK) | (1U << RCC_BDCR_RTCSEL_SHIFT);
        return RTC_SRC_LSE;
    }

    /* No crystal fitted on this board. LSI is spec'd 17-47 kHz, so the 1 Hz
     * derived below is nominal only and the calendar drifts by roughly an hour
     * a day. Deliberately accepted: t_ms carries relative timing, and the
     * timestamp is for ordering and human orientation, not for absolute time. */
    RCC_BDCR &= ~RCC_BDCR_LSEON;
    if (ensure_running(RTC_SRC_LSI) != 0) return RTC_SRC_NONE;

    RCC_BDCR = (RCC_BDCR & ~RCC_BDCR_RTCSEL_MASK) | (2U << RCC_BDCR_RTCSEL_SHIFT);
    return RTC_SRC_LSI;
}

rtc_src_t rtc_init(void) {
    /* Lock 1: without the power interface clock, PWR_CR reads and writes as 0. */
    RCC_APB1ENR |= RCC_APB1ENR_PWREN;
    volatile uint32_t tmp = RCC_APB1ENR; (void)tmp;

    /* Lock 2: opens RCC_BDCR and the whole RTC block for writing. */
    PWR_CR |= PWR_CR_DBP;

    if (RTC_BKP0R == RTC_BKP_MAGIC) {
        /* Warm start: the calendar kept running through the reset. Recover which
         * source is live rather than touching anything. */
        was_running = 1;
        uint32_t sel = (RCC_BDCR & RCC_BDCR_RTCSEL_MASK) >> RCC_BDCR_RTCSEL_SHIFT;
        src = (sel == 1) ? RTC_SRC_LSE : (sel == 2) ? RTC_SRC_LSI : RTC_SRC_NONE;

        if (ensure_running(src) != 0) { src = RTC_SRC_NONE; return src; }

        /* Shadow registers reload two RTCCLK cycles after the clock resumes;
         * reading TR/DR before RSF sets returns the reset value, not the time. */
        RTC_ISR &= ~RTC_ISR_RSF;
        wait_flag(&RTC_ISR, RTC_ISR_RSF, 1, SYNC_TIMEOUT_MS);
        return src;
    }

    /* Cold start. RTCSEL is write-once per backup-domain reset, so a stale
     * non-zero value would make the selection below silently do nothing.
     * Asserting BDRST wipes the domain and guarantees the field is writable. */
    RCC_BDCR |= RCC_BDCR_BDRST;
    RCC_BDCR &= ~RCC_BDCR_BDRST;

    src = start_clock_source();
    if (src == RTC_SRC_NONE) return RTC_SRC_NONE;

    RCC_BDCR |= RCC_BDCR_RTCEN;

    /* Lock 3 */
    wpr_unlock();

    RTC_ISR |= RTC_ISR_INIT;
    if (wait_flag(&RTC_ISR, RTC_ISR_INITF, 1, SYNC_TIMEOUT_MS) != 0) {
        wpr_lock();
        return RTC_SRC_NONE;
    }

    /* RM0090 requires the two prescaler halves in separate writes, async first. */
    uint32_t prer = (src == RTC_SRC_LSE) ? PRER_LSE : PRER_LSI;
    RTC_PRER = prer & 0x7F0000U;
    RTC_PRER = prer;

    rtc_time_t t;
    build_timestamp(&t);
    RTC_TR = ((uint32_t)BIN2BCD(t.hour) << 16) |
             ((uint32_t)BIN2BCD(t.min)  << 8)  |
              (uint32_t)BIN2BCD(t.sec);
    RTC_DR = ((uint32_t)BIN2BCD(t.year % 100) << 16) |
             ((uint32_t)BIN2BCD(t.month)      << 8)  |
              (uint32_t)BIN2BCD(t.day);

    RTC_CR &= ~(1U << 6);          /* FMT = 0: 24-hour format */

    RTC_ISR &= ~RTC_ISR_INIT;      /* leaving init mode restarts the counter */
    wait_flag(&RTC_ISR, RTC_ISR_RSF, 1, SYNC_TIMEOUT_MS);

    RTC_BKP0R = RTC_BKP_MAGIC;
    wpr_lock();
    return src;
}

void rtc_now(rtc_time_t *t) {
    /* Read order is mandatory: with shadow registers enabled, touching SSR or TR
     * freezes the calendar until DR is read. Skip DR and every later read is
     * stale. */
    uint32_t ssr = RTC_SSR;
    uint32_t tr  = RTC_TR;
    uint32_t dr  = RTC_DR;

    t->sec   = (uint8_t)BCD2BIN(tr & 0x7F);
    t->min   = (uint8_t)BCD2BIN((tr >> 8) & 0x7F);
    t->hour  = (uint8_t)BCD2BIN((tr >> 16) & 0x3F);
    t->day   = (uint8_t)BCD2BIN(dr & 0x3F);
    t->month = (uint8_t)BCD2BIN((dr >> 8) & 0x1F);
    t->year  = (uint16_t)(2000 + BCD2BIN((dr >> 16) & 0xFF));

    /* SSR counts DOWN from PREDIV_S, so elapsed fraction is (PREDIV_S - SSR). */
    uint32_t prediv_s = RTC_PRER & 0x7FFFU;
    t->ms = (uint16_t)(((prediv_s - (ssr & 0x7FFFU)) * 1000U) / (prediv_s + 1U));
}

rtc_src_t rtc_source(void) { return src; }
int rtc_was_running(void)  { return was_running; }

const char *rtc_source_name(void) {
    switch (src) {
    case RTC_SRC_LSE: return "LSE 32.768 kHz crystal";
    case RTC_SRC_LSI: return "LSI internal RC - gains ~1 h/day, absolute time is approximate";
    default:          return "none - RTC not running";
    }
}
