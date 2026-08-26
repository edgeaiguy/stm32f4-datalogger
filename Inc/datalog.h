#ifndef DATALOG_H
#define DATALOG_H

#include <stdint.h>

typedef struct {
    uint32_t t_ms;
    int      env_valid;    /* 0 -> temp/press columns are written empty */
    int32_t  temp_c100;    /* hundredths of a degree C */
    uint32_t press_q24_8;  /* Pa in Q24.8 */
    int16_t  x, y, z;      /* raw ADXL345 counts, 256 LSB/g */
} datalog_row_t;

/* Mount the card and open the next unused LOGnnnnn.CSV with a header row.
 * Returns 0 on success, or the FatFs FRESULT as a negative value. */
int datalog_open(void);

/* Append one row. Syncs to the card periodically; see DATALOG_SYNC_ROWS. */
int datalog_write_row(const datalog_row_t *row);

/* Name chosen by datalog_open(), or "" if it never succeeded. */
const char *datalog_filename(void);

#endif // DATALOG_H
