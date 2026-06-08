#ifndef I2C_H
#define I2C_H

#include <stdint.h>

void i2c_init(void);
int i2c_start(void);
void i2c_stop(void);
int i2c_send_address(uint8_t addr, uint8_t rw);

#endif // I2C_H