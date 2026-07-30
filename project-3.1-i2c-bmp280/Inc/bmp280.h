#ifndef BMP280_H
#define BMP280_H

#include <stdint.h>

#define BMP280_ADDR        0x76   // default I2C address (SDO tied to GND)
#define BMP280_CHIP_ID     0x58   // expected value of the ID register

/* register map */
#define BMP280_REG_CALIB     0x88 // first calibration register (dig_T1 LSB)
#define BMP280_CALIB_LEN     24   // 0x88..0x9F: dig_T1..dig_P9
#define BMP280_REG_ID        0xD0 // chip ID
#define BMP280_REG_RESET     0xE0 // write 0xB6 for a power-on reset
#define BMP280_REG_STATUS    0xF3 // bit 3 = measuring, bit 0 = NVM copy in progress
#define BMP280_REG_CTRL_MEAS 0xF4 // osrs_t[7:5] | osrs_p[4:2] | mode[1:0]
#define BMP280_REG_CONFIG    0xF5 // t_sb[7:5] | filter[4:2] | spi3w_en[0]
#define BMP280_REG_PRESS_MSB 0xF7 // start of the 6-byte press+temp burst

#define BMP280_STATUS_MEASURING (1U << 3)
#define BMP280_STATUS_IM_UPDATE (1U << 0)
#define BMP280_MODE_MASK        0x03U // ctrl_meas mode[1:0]; reads back 00 once forced mode completes
#define BMP280_RESET_CMD        0xB6U // the only value the reset register accepts

/* osrs_t x2 (010<<5), osrs_p x16 (101<<2), forced mode (01).
 * Worst-case conversion time at this oversampling is ~43 ms. */
#define BMP280_CTRL_MEAS_FORCED 0x55

/* Factory calibration read out of the sensor's NVM at startup. t_fine is not
 * calibration data - it is the shared intermediate that the temperature
 * calculation produces and the pressure calculation consumes. */
typedef struct {
  uint16_t dig_T1;
  int16_t  dig_T2;
  int16_t  dig_T3;
  uint16_t dig_P1;
  int16_t  dig_P2;
  int16_t  dig_P3;
  int16_t  dig_P4;
  int16_t  dig_P5;
  int16_t  dig_P6;
  int16_t  dig_P7;
  int16_t  dig_P8;
  int16_t  dig_P9;
  int32_t  t_fine;
} bmp280_calib_t;

int bmp280_read_id(uint8_t *id);

/* Read the calibration block and put the sensor in a known configuration. */
int bmp280_init(bmp280_calib_t *calib);

/* Trigger one forced-mode measurement, wait for it, and compensate the result.
 * temp_c100 is in hundredths of a degree C (2534 = 25.34 C).
 * press_q24_8 is Pa in Q24.8 format (24674867 = 24674867/256 = 96386.2 Pa). */
int bmp280_read(bmp280_calib_t *calib, int32_t *temp_c100, uint32_t *press_q24_8);

/* Exposed for testing against known datasheet vectors. Temperature must be
 * compensated first - it is what sets calib->t_fine. */
int32_t  bmp280_compensate_temp(bmp280_calib_t *calib, int32_t adc_T);
uint32_t bmp280_compensate_press(const bmp280_calib_t *calib, int32_t adc_P);

#endif
