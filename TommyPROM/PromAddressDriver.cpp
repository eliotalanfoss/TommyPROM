// This controls the shift register that generates the address lines for A0..A15 for most
// chip families.  This is not used by the PromDevice8755A code.
//
// Note that this uses direct port control instead of digitalWrite calls so that the code
// can run fast enough to meet the tBLC requirements for SDP and block writes.  This
// sacrifices portability and readability for speed. //
//
// This code will only work on Arduino Uno and Nano hardware.  The ports for other
// Arduinos map to different IO pins.

#include "PromAddressDriver.h"

// ELIOT changed to only one shift clock and changed pins
#define ADDR_CLK        2
#define ADDR_DATA       3

// Define masks for the address clk and data lines on PD2..PD3 for direct port control.
// ELIOT changed to only one shift clock and dif pins as above
#define ADDR_CLK_MASK       0x08
#define ADDR_DATA_MASK      0x04

// For larger ROMs, address lines A16..A18 are controlled by D10..D12 (PB2..PB4).
// ELIOT not supported
#define UPPER_ADDR_MASK     0x00

// When using the 74LS595 shift registers, the RCLK lines of both shift registers can be
// connected to D4 (PD4).
// ELIOT changed to D4
#define RCLK_595_MASK       0x10


void PromAddressDriver::begin()
{
    // The address control pins are always outputs.
    pinMode(ADDR_DATA, OUTPUT);
    pinMode(ADDR_CLK, OUTPUT);
    digitalWrite(ADDR_DATA, LOW);
    digitalWrite(ADDR_CLK, LOW);
    DDRD |= RCLK_595_MASK; // Set D4 as output
}


void PromAddressDriver::setAddress(uint32_t addr)
{
    byte mask = 0;
    mask = ADDR_CLK_MASK;

    // Make sure the clock is low to start.
    PORTD &= ~mask;

    PORTD &= ~RCLK_595_MASK;

    // Shift 8 bits in, starting with the MSB.
    for (int ix = 0; (ix < 16); ix++)
    {
        // Set the data bit
        if (addr & 0x8000)
        {
            PORTD |= ADDR_DATA_MASK;
        }
        else
        {
            PORTD &= ~ADDR_DATA_MASK;
        }

        // Toggle the clock high then low
        PORTD |= mask;
        delayMicroseconds(3);
        PORTD &= ~mask;
        addr <<= 1;
    }

    // Toggle the RCLK pin to output the data for 74LS595 shift registers.  This pin is
    // not connected when using 74LS164 shift registers.
    PORTD |= RCLK_595_MASK;
    delayMicroseconds(1);
    PORTD &= ~RCLK_595_MASK;
}
