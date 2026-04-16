#ifndef MEGA_TWI_H
#define MEGA_TWI_H

#include <stdint.h>

void twi_master_init(void);
void twi_master_send_byte(uint8_t slave_address, uint8_t data);

#endif
