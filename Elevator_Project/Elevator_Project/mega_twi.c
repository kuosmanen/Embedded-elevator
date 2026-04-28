#include "mega_twi.h"
#include <avr/io.h>

/*
 * TWI/I2C master implementation based on the Exercise 10 pattern.
 * SCL is configured for approximately 100 kHz at F_CPU = 16 MHz.
 */
void twi_master_init(void) //initializing MEGA (master)
{
    /*Initializing clock*/
    TWSR = 0x00;     /* prescaler = 1 */
    TWBR = 72;       /* ~100 kHz */
    TWCR = (1 << TWEN); //Enable TWI hardware
}

void twi_master_send_byte(uint8_t slave_address, uint8_t data) //MEGA (master) communicates with UNO (slave) aka. sends data
{
    /*Sends 1 byte to slave device*/
    // TWCR the TWI Control Register
    // TWINT interrupt when done/ready
    // TWEN turns on hardware
    // TWSTA send START when communication with slave starts
    // TWSTO send STOP ends communication
    
    /* START */
    //Sends START signal to bus
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT))) { }

    /* SLA+W */
    //Sends slave address, and write or read mode
    TWDR = (uint8_t)((slave_address << 1) | 0u);
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT))) { }

    /* DATA */
    //Sending data
    TWDR = data;
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT))) { }

    /* STOP */
    //Stopping
    TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWSTO);
}
