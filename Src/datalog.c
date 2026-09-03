/* datalog.c — CSV logging of the fused sensor stream onto the SD card. */
#include <stdio.h>
#include "ff.h"
#include "rtc.h"
#include "datalog.h"

/* FatFs buffers writes, so nothing is safely on the card until f_sync(). Every
 * 25 rows is 5 s at the 200 ms tick: it bounds what a card-yank loses to 5 s
 * while amortising the directory + FAT rewrite across 25 samples, instead of
 * paying it at 5 Hz and risking a stall that overruns the tick. */
#define DATALOG_SYNC_ROWS  25

#define CSV_HEADER "timestamp,t_ms,temp_c,press_hpa,accel_x_mg,accel_y_mg,accel_z_mg\r\n"

static FATFS    fs;
static FIL      file;
static char     fname[13];      /* 8.3 plus NUL */
static int      open_ok;
static uint32_t rows_since_sync;

/* Integer/fraction split throughout — the firmware has no float printf path.
 * Environmental columns are written empty, not repeated, when the barometer
 * did not run this tick: a held value would look like a measurement that was
 * never taken, and empty is what pandas and Excel already read as missing. */
static int format_row(char *out, size_t cap, const datalog_row_t *r) {
    /* Sized for the full int32/Q24.8 range, not the sensor's actual span, so
     * the formatting can never truncate silently. */
    char tbuf[16] = "";
    char pbuf[16] = "";

    if (r->env_valid) {
        int32_t t = r->temp_c100;
        const char *sign = (t < 0) ? "-" : "";
        if (t < 0) t = -t;
        snprintf(tbuf, sizeof tbuf, "%s%ld.%02ld",
                 sign, (long)(t / 100), (long)(t % 100));

        uint32_t pa = r->press_q24_8 >> 8;   /* Q24.8 -> whole Pa */
        snprintf(pbuf, sizeof pbuf, "%lu.%02lu",
                 (unsigned long)(pa / 100), (unsigned long)(pa % 100));
    }

    /* ±2g full-res -> 256 LSB/g */
    int xm = (r->x * 1000) / 256;
    int ym = (r->y * 1000) / 256;
    int zm = (r->z * 1000) / 256;

    /* Wall clock for humans, t_ms for ordering: the RTC only resolves to its
     * sub-second tick and could in principle be stepped, while t_ms is
     * monotonic from boot. */
    rtc_time_t w;
    rtc_now(&w);

    return snprintf(out, cap,
                    "%04u-%02u-%02uT%02u:%02u:%02u.%03u,%lu,%s,%s,%d,%d,%d\r\n",
                    w.year, w.month, w.day, w.hour, w.min, w.sec, w.ms,
                    (unsigned long)r->t_ms, tbuf, pbuf, xm, ym, zm);
}

int datalog_open(void) {
    FRESULT fr = f_mount(&fs, "", 1);   /* 1 = mount now, so errors surface here */
    if (fr != FR_OK) return -(int)fr;

    /* Probe upward for the first free name so a reset does not clobber the
     * previous session. 8.3 names, so LOGnnnnn.CSV is the whole budget. */
    FILINFO info;
    for (unsigned n = 1; n <= 99999; n++) {
        snprintf(fname, sizeof fname, "LOG%05u.CSV", n);

        FRESULT st = f_stat(fname, &info);
        if (st == FR_OK) continue;              /* name taken, try the next */
        if (st != FR_NO_FILE) return -(int)st;  /* a real error, not absence */

        fr = f_open(&file, fname, FA_WRITE | FA_CREATE_NEW);
        if (fr != FR_OK) return -(int)fr;

        UINT bw;
        fr = f_write(&file, CSV_HEADER, sizeof(CSV_HEADER) - 1, &bw);
        if (fr == FR_OK) fr = f_sync(&file);
        if (fr != FR_OK) { f_close(&file); return -(int)fr; }

        open_ok = 1;
        return 0;
    }

    fname[0] = '\0';
    return -1;   /* every name in the series is taken */
}

int datalog_write_row(const datalog_row_t *row) {
    if (!open_ok) return -1;

    char line[128];
    int n = format_row(line, sizeof line, row);
    if (n <= 0 || (size_t)n >= sizeof line) return -1;   /* formatting truncated */

    UINT bw;
    FRESULT fr = f_write(&file, line, (UINT)n, &bw);
    if (fr != FR_OK) return -(int)fr;
    if (bw != (UINT)n) return -1;                        /* short write: card full */

    if (++rows_since_sync >= DATALOG_SYNC_ROWS) {
        rows_since_sync = 0;
        fr = f_sync(&file);
        if (fr != FR_OK) return -(int)fr;
    }
    return 0;
}

const char *datalog_filename(void) {
    return fname;
}
