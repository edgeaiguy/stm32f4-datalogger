#ifndef I2C_H
#define I2C_H

#include <stdint.h>

void i2c_init(void);
int i2c_start(void);
void i2c_stop(void);
int i2c_send_address(uint8_t addr, uint8_t rw);
int i2c_write_byte(uint8_t data);
int i2c_read_byte(uint8_t *data, int ack);
int i2c_write_register(uint8_t dev_addr, uint8_t reg_addr, uint8_t data);
int i2c_read_register(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data);

#endif // I2C_H