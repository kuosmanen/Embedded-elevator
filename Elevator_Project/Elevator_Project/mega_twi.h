#ifndef MEGA_TWI_H
#define MEGA_TWI_H

#include <stdint.h>

void twi_master_init(void); //initializing Mega (master)
void twi_master_send_byte(uint8_t slave_address, uint8_t data); // // MEGA (master) communicates with UNO (slave) aka. sends data

#endif
