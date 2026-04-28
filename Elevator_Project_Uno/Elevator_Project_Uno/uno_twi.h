#ifndef UNO_TWI_H
#define UNO_TWI_H

#include <stdint.h>

void twi_slave_init(uint8_t slave_address); /* initilizing UNO (slave) */
uint8_t twi_slave_receive_byte(uint8_t *data); /* UNO (slave) communicating with MEGA (master) aka. recieving data from master*/

#endif
