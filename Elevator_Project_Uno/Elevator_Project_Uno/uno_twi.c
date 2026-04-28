#include "uno_twi.h"
#include <avr/io.h>
#include <avr/interrupt.h>

static volatile uint8_t g_rx_pending = 0u;
static volatile uint8_t g_rx_data = 0u;

/*
 * Polling-based TWI slave receive.
 * Returns 1 when a data byte has been received, otherwise returns 0.
 */
void twi_slave_init(uint8_t slave_address) /* initilizing slave */
{
    TWAR = (uint8_t)(slave_address << 1);
    TWCR = (1 << TWEA) | (1 << TWEN) | (1 << TWIE) | (1 << TWINT);
}
    /* 
    * TWAR is TWI address register
    * TWCR the TWI Control Register
    * TWINT interrupts when done/ready
    * TWEN turns on hardware
    */

uint8_t twi_slave_receive_byte(uint8_t *data) /* UNO (slave) communicating with MEGA (master) aka. recieving data from master*/
{
    uint8_t sreg;

    if (g_rx_pending == 0u) {
        return 0u;
    }

    sreg = SREG;
    cli();
    *data = g_rx_data;
    g_rx_pending = 0u;
    SREG = sreg;
    return 1u;
}

ISR(TWI_vect)
{
    uint8_t status;

    status = (uint8_t)(TWSR & 0xF8u);

    /* list of instructions send to slave (here) by master */
    switch (status) {
        case 0x60: /* own SLA+W received, ACK returned */
        case 0x68: /* arbitration lost, own SLA+W received */
        case 0x70: /* general call received */
        case 0x78:
            TWCR = (1 << TWEA) | (1 << TWEN) | (1 << TWIE) | (1 << TWINT); /* Acknowledge and waiting data */
            return;

        case 0x80: /* data received, ACK returned */
        case 0x90: /* general call data received */
            g_rx_data = TWDR;
            g_rx_pending = 1u;
            TWCR = (1 << TWEA) | (1 << TWEN) | (1 << TWIE) | (1 << TWINT);
            return;

        case 0x88: /* data received, NACK returned */
        case 0x98:
            g_rx_data = TWDR;
            g_rx_pending = 1u;
            TWCR = (1 << TWEA) | (1 << TWEN) | (1 << TWIE) | (1 << TWINT);
            return;

        case 0xA0: /* STOP or repeated START */
        default:
            TWCR = (1 << TWEA) | (1 << TWEN) | (1 << TWIE) | (1 << TWINT);
            return;
    }
}
