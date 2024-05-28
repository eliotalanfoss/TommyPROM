#include "Configure.h"
#if defined(PROM_IS_28C)

#include "PromAddressDriver.h"

// IO lines for the EEPROM device control
// Pins D2..D9 are used for the data bus.
// Edited to fit different wiring ELIOT
#define WE              A0
#define CE              A2
#define OE              A1

// Lookup table to help reverse bit order in a byte
// to reverse byte n do: (lookup[n&0b1111] << 4) | lookup[n>>4]
static uint8_t lookup[16] = {
0x0, 0x8, 0x4, 0xc, 0x2, 0xa, 0x6, 0xe,
0x1, 0x9, 0x5, 0xd, 0x3, 0xb, 0x7, 0xf, };

// Set the status of the device control pins
static void enableChip()       { digitalWrite(CE, LOW); }
static void disableChip()      { digitalWrite(CE, HIGH);}
static void enableOutput()     { digitalWrite(OE, LOW); }
static void disableOutput()    { digitalWrite(OE, HIGH);}
static void enableWrite()      { digitalWrite(WE, LOW); }
static void disableWrite()     { digitalWrite(WE, HIGH);}


PromDevice28C::PromDevice28C(uint32_t size, word blockSize, unsigned maxWriteTime, bool polling)
    : PromDevice(size, blockSize, maxWriteTime, polling)
{
}


void PromDevice28C::begin()
{
    // Define the data bus as input initially so that it does not put out a
    // signal that could collide with output on the data pins of the EEPROM.
    setDataBusMode(INPUT);

    // Define the EEPROM control pins as output, making sure they are all
    // in the disabled state.
    digitalWrite(OE, HIGH);
    pinMode(OE, OUTPUT);
    digitalWrite(CE, HIGH);
    pinMode(CE, OUTPUT);
    digitalWrite(WE, HIGH);
    pinMode(WE, OUTPUT);

    // This chip uses the shift register hardware for addresses, so initialize that.
    PromAddressDriver::begin();
}


// Write the special six-byte code to turn off Software Data Protection.
ERET PromDevice28C::disableSoftwareWriteProtect()
{
    disableOutput();
    disableWrite();
    enableChip();
    setDataBusMode(OUTPUT);

    setByte(0x55, 0x5555);
    setByte(0xaa, 0x2aaa);
    setByte(0x01, 0x5555);
    setByte(0x55, 0x5555);
    setByte(0xaa, 0x2aaa);
    setByte(0x04, 0x5555);

    setDataBusMode(INPUT);
    disableChip();

    return RET_OK;
}


// Write the special three-byte code to turn on Software Data Protection.
ERET PromDevice28C::enableSoftwareWriteProtect()
{
    disableOutput();
    disableWrite();
    enableChip();
    setDataBusMode(OUTPUT);

    setByte(0x55, 0x5555);
    setByte(0xaa, 0x2aaa);
    setByte(0x05, 0x5555);

    setDataBusMode(INPUT);
    disableChip();

    return RET_OK;
}

// Write the special six-byte code to erase entire EEPROM.
ERET PromDevice28C::wipe() {

    disableOutput();
    disableWrite();
    enableChip();
    setDataBusMode(OUTPUT);

    setByte(0x55, 0x5555);
    setByte(0xaa, 0x2aaa);
    setByte(0x01, 0x5555);
    setByte(0x55, 0x5555);
    setByte(0xaa, 0x2aaa);
    setByte(0x08, 0x5555);

    setDataBusMode(INPUT);
    disableChip();
    
    // Wait for chip erase to finish
    delay(25);
    return RET_OK;
}


// BEGIN PRIVATE METHODS
//

byte PromDevice28C::bit_reverse_byte (byte byte_to_reverse) {
  return (lookup[byte_to_reverse&0b1111] << 4) | lookup[byte_to_reverse>>4];
}

// Use the PromAddressDriver to set an address in the two address shift registers.
void PromDevice28C::setAddress(uint32_t address)
{
    PromAddressDriver::setAddress(address);
}


// Read a byte from a given address
byte PromDevice28C::readByte(uint32_t address)
{
    byte data = 0;
    setAddress(address);
    setDataBusMode(INPUT);
    disableOutput();
    disableWrite();
    enableChip();
    enableOutput();
    data = readDataBus();
    disableOutput();
    disableChip();

    data = bit_reverse_byte(data);

    return data;
}


// Burn a byte to the chip and verify that it was written.
bool PromDevice28C::burnByte(byte value, uint32_t address)
{
    value = bit_reverse_byte(value);
    
    bool status = false;

    disableOutput();
    disableWrite();

    setAddress(address);
    setDataBusMode(OUTPUT);
    writeDataBus(value);

    enableChip();
    delayMicroseconds(1);
    enableWrite();
    delayMicroseconds(1);
    disableWrite();

    status = waitForWriteCycleEnd(value);

    disableChip();

    return status;
}


bool PromDevice28C::burnBlock(byte data[], uint32_t len, uint32_t address)
{
    bool status = false;
    if (len == 0)  return true;

    byte data_to_write[len];
    for (uint32_t ix = 0; (ix < len); ix++) {
        data_to_write[ix] = bit_reverse_byte(data[ix]);
    }

    ++debugBlockWrites;
    disableOutput();
    disableWrite();
    enableChip();

    // Write all of the bytes in the block out to the chip.  The chip will
    // program them all at once as long as they are written fast enough.
    setDataBusMode(OUTPUT);
    for (uint32_t ix = 0; (ix < len); ix++)
    {
        setAddress(address + ix);
        writeDataBus(data_to_write[ix]);

        delayMicroseconds(1);
        enableWrite();
        delayMicroseconds(1);
        disableWrite();
    }

    status = waitForWriteCycleEnd(data_to_write[len - 1]);
    disableChip();

    if (!status) {
        debugLastAddress = address + len - 1;
    }
    return status;
}


bool PromDevice28C::waitForWriteCycleEnd(byte lastValue)
{
    if (mSupportsDataPoll)
    {
        // Verify programming complete by reading the last value back until it matches the
        // value written twice in a row.  The D7 bit will read the inverse of last written
        // data and the D6 bit will toggle on each read while in programming mode.
        //
        // This loop code takes about 18uSec to execute.  The max readcount is set to the
        // device's maxReadTime (in uSecs) divided by ten rather than eighteen to ensure
        // that it runs at least as long as the chip's timeout value, even if some code
        // optimizations are made later. In actual practice, the loop will terminate much
        // earlier because it will detect the end of the write well before the max time.
        byte b1=0, b2=0;
        setDataBusMode(INPUT);
        delayMicroseconds(1);
        for (unsigned readCount = 1; (readCount < (mMaxWriteTime * 100)); readCount++)
        {
            enableChip();
            enableOutput();
            delayMicroseconds(1);
            b1 = readDataBus();
            disableOutput();
            disableChip();
            enableChip();
            enableOutput();
            delayMicroseconds(1);
            b2 = readDataBus();
            disableOutput();
            disableChip();
            if ((b1 == b2) && (b1 == lastValue))
            {
                return true;
            }
        }

        debugLastExpected = lastValue;
        debugLastReadback = b2;
        return false;
    }
    else
    {
        // No way to detect success.  Just wait the max write time.
        delayMicroseconds(mMaxWriteTime * 1000L);
        return true;
    }
}


// Set an address and data value and toggle the write control.  This is used
// to write control sequences, like the software write protect.  This is not a
// complete byte write function because it does not set the chip enable or the
// mode of the data bus.
void PromDevice28C::setByte(byte value, uint32_t address)
{
    setAddress(address);
    writeDataBus(value);

    delayMicroseconds(1);
    enableWrite();
    delayMicroseconds(1);
    disableWrite();
}

#endif // #if defined(PROM_IS_28C)
