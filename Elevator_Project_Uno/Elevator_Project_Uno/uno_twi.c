#include "uno_twi.h"
#include <avr/io.h>

/*
 * Polling-based TWI slave receive.
 * Returns 1 when a data byte has been received, otherwise returns 0.
 */
void twi_slave_init(uint8_t slave_address)
{
    TWAR = (uint8_t)(slave_address << 1);
    TWCR = (1 << TWEA) | (1 << TWEN) | (1 << TWINT);
}

uint8_t twi_slave_receive_byte(uint8_t *data)
{
    uint8_t status;

    if (!(TWCR & (1 << TWINT))) {
        return 0u;
    }

    status = (uint8_t)(TWSR & 0xF8u);

    switch (status) {
        case 0x60: /* own SLA+W received, ACK returned */
        case 0x68: /* arbitration lost, own SLA+W received */
        case 0x70: /* general call received */
        case 0x78:
            TWCR = (1 << TWEA) | (1 << TWEN) | (1 << TWINT);
            return 0u;

        case 0x80: /* data received, ACK returned */
        case 0x90: /* general call data received */
            *data = TWDR;
            TWCR = (1 << TWEA) | (1 << TWEN) | (1 << TWINT);
            return 1u;

        case 0x88: /* data received, NACK returned */
        case 0x98:
            *data = TWDR;
            TWCR = (1 << TWEA) | (1 << TWEN) | (1 << TWINT);
            return 1u;

        case 0xA0: /* STOP or repeated START */
        default:
            TWCR = (1 << TWEA) | (1 << TWEN) | (1 << TWINT);
            return 0u;
    }
}
