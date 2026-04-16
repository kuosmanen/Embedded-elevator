#ifndef UNO_TWI_H
#define UNO_TWI_H

#include <stdint.h>

void twi_slave_init(uint8_t slave_address);
uint8_t twi_slave_receive_byte(uint8_t *data);

#endif
