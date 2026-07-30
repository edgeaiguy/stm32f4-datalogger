#include <stdint.h>
#include "stm32f407xx.h"
#include "i2c.h"
#include "bmp280.h"

/* Every calibration word is stored LSB first. */
static uint16_t rd_u16(const uint8_t *b, int i) {
  return (uint16_t)(b[i] | (b[i + 1] << 8));
}

static int16_t rd_s16(const uint8_t *b, int i) {
  return (int16_t)rd_u16(b, i);  // reinterpret, not convert - two's complement already
}

int bmp280_read_id(uint8_t *id) {
    return i2c_read_register(BMP280_ADDR, BMP280_REG_ID, id);
}

int bmp280_init(bmp280_calib_t *calib) {
    // Soft reset first, so we start from a known configuration regardless of what
    // a previous run left in ctrl_meas/config.
    if (i2c_write_register(BMP280_ADDR, BMP280_REG_RESET, BMP280_RESET_CMD) != 0) return -1;

    // The sensor is unreachable for up to 2 ms after a reset and then copies its
    // calibration out of NVM (im_update = 1 while that runs). Failed reads during
    // that window are expected, not fatal.
    uint8_t status;
    uint32_t tries = 0;
    do {
        if (++tries > 1000) return -1;
        if (i2c_read_register(BMP280_ADDR, BMP280_REG_STATUS, &status) != 0) {
            status = BMP280_STATUS_IM_UPDATE;  // not answering yet - keep waiting
        }
    } while (status & BMP280_STATUS_IM_UPDATE);

    uint8_t raw[BMP280_CALIB_LEN];
    if (i2c_read_registers(BMP280_ADDR, BMP280_REG_CALIB, raw, BMP280_CALIB_LEN) != 0) return -1;

    calib->dig_T1 = rd_u16(raw, 0);
    calib->dig_T2 = rd_s16(raw, 2);
    calib->dig_T3 = rd_s16(raw, 4);
    calib->dig_P1 = rd_u16(raw, 6);
    calib->dig_P2 = rd_s16(raw, 8);
    calib->dig_P3 = rd_s16(raw, 10);
    calib->dig_P4 = rd_s16(raw, 12);
    calib->dig_P5 = rd_s16(raw, 14);
    calib->dig_P6 = rd_s16(raw, 16);
    calib->dig_P7 = rd_s16(raw, 18);
    calib->dig_P8 = rd_s16(raw, 20);
    calib->dig_P9 = rd_s16(raw, 22);
    calib->t_fine = 0;

    // dig_T1/dig_P1 are unsigned and never legitimately zero, so an all-zero or
    // all-0xFF block means we read the bus, not the sensor's NVM
    if (calib->dig_T1 == 0 || calib->dig_P1 == 0) return -1;
    if (calib->dig_T1 == 0xFFFF || calib->dig_P1 == 0xFFFF) return -1;

    // IIR filter off: it only helps in normal mode, where consecutive samples
    // can be averaged. The device is in sleep after power-up, which is the only
    // time config may be written.
    return i2c_write_register(BMP280_ADDR, BMP280_REG_CONFIG, 0x00);
}

/* Datasheet section 3.11.3, 32-bit fixed-point variant. Returns hundredths of
 * a degree C and stores t_fine for the pressure calculation. */
int32_t bmp280_compensate_temp(bmp280_calib_t *calib, int32_t adc_T) {
    int32_t var1, var2;

    var1 = ((((adc_T >> 3) - ((int32_t)calib->dig_T1 << 1))) * ((int32_t)calib->dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((int32_t)calib->dig_T1)) *
              ((adc_T >> 4) - ((int32_t)calib->dig_T1))) >> 12) *
            ((int32_t)calib->dig_T3)) >> 14;

    calib->t_fine = var1 + var2;
    return (calib->t_fine * 5 + 128) >> 8;
}

/* Datasheet section 3.11.3, 64-bit fixed-point variant. Returns Pa in Q24.8.
 * The 64-bit intermediates are required - the 32-bit variant in the datasheet
 * trades ~1 Pa of accuracy to avoid them, and we have no reason to. */
uint32_t bmp280_compensate_press(const bmp280_calib_t *calib, int32_t adc_P) {
    int64_t var1, var2, p;

    var1 = ((int64_t)calib->t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)calib->dig_P6;
    var2 = var2 + ((var1 * (int64_t)calib->dig_P5) << 17);
    var2 = var2 + (((int64_t)calib->dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)calib->dig_P3) >> 8) + ((var1 * (int64_t)calib->dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)calib->dig_P1) >> 33;

    if (var1 == 0) return 0;  // would divide by zero

    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)calib->dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)calib->dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)calib->dig_P7) << 4);

    return (uint32_t)p;
}

int bmp280_read(bmp280_calib_t *calib, int32_t *temp_c100, uint32_t *press_q24_8) {
    // forced mode: take one measurement, then fall back to sleep by itself
    if (i2c_write_register(BMP280_ADDR, BMP280_REG_CTRL_MEAS, BMP280_CTRL_MEAS_FORCED) != 0) {
        return -1;
    }

    // Wait for the conversion. Polling status.measuring alone is racy: the sensor
    // takes a moment to assert it, so an early read sees 0 and we would return the
    // register's previous (or power-on) contents. ctrl_meas.mode has no such gap -
    // we just wrote 01 into it ourselves, and the sensor clears it to 00 when the
    // forced measurement completes. Check both.
    //
    // status (0xF3) and ctrl_meas (0xF4) are adjacent, so one 2-byte read gets both.
    // ~0.5 ms per read at 100 kHz vs a ~43 ms conversion, so ~90 polls; 1000 is a
    // generous ceiling.
    uint8_t s[2];
    uint32_t polls = 0;
    do {
        if (++polls > 1000) return -1;
        if (i2c_read_registers(BMP280_ADDR, BMP280_REG_STATUS, s, 2) != 0) return -1;
    } while ((s[0] & BMP280_STATUS_MEASURING) || (s[1] & BMP280_MODE_MASK));

    // Burst-read all 6 bytes so temperature and pressure come from the same
    // measurement; reading them separately could straddle two conversions.
    uint8_t d[6];
    if (i2c_read_registers(BMP280_ADDR, BMP280_REG_PRESS_MSB, d, 6) != 0) return -1;

    // 20-bit results: msb[7:0] lsb[7:0] xlsb[7:4]
    int32_t adc_P = ((int32_t)d[0] << 12) | ((int32_t)d[1] << 4) | ((int32_t)d[2] >> 4);
    int32_t adc_T = ((int32_t)d[3] << 12) | ((int32_t)d[4] << 4) | ((int32_t)d[5] >> 4);

    // order matters: temperature sets t_fine, which pressure depends on
    *temp_c100 = bmp280_compensate_temp(calib, adc_T);
    *press_q24_8 = bmp280_compensate_press(calib, adc_P);
    return 0;
}
