#include "mega_twi.h"
#include <avr/io.h>

/*
 * TWI/I2C master implementation based on the Exercise 10 pattern.
 * SCL is configured for approximately 100 kHz at F_CPU = 16 MHz.
 */
void twi_master_init(void)
{
    TWSR = 0x00;     /* prescaler = 1 */
    TWBR = 72;       /* ~100 kHz */
    TWCR = (1 << TWEN);
}

void twi_master_send_byte(uint8_t slave_address, uint8_t data)
{
    /* START */
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT))) { }

    /* SLA+W */
    TWDR = (uint8_t)((slave_address << 1) | 0u);
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT))) { }

    /* DATA */
    TWDR = data;
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT))) { }

    /* STOP */
    TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWSTO);
}
