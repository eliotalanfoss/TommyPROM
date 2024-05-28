/*****************************************************************************/
/*****************************************************************************/
/**
 *
 * XMODEM CRC Communication
 *
 * Simple implementation of read and write using XMODEM CRC.  This is tied
 * directly to the PROM code, so the receive function writes the data to the
 * PROM device as each packet is received.  The complete file is not kept
 * in memory.
 */
#ifndef INCLUDE_CONFIGURE_H
#define INCLUDE_CONFIGURE_H

#include "Arduino.h"
#include "Configure.h"

// The original TommyPROM code used XModem CRC protocol but this requires parameters to be
// changed for the host communication program.  The default is now to use basic XModem
// with the 8-bit checksum instead of the 16-bit CRC error checking.  This shouldn't
// matter because communication errors aren't likely to happen on a 3 foot USB cable like
// they did over the long distance dail-up lines that XModem was designed for.  Uncomment
// the XMODEM_CRC_PROTOCOL line below to restore the original XMODEM CRC support.
// ELIOT moved to header file so that it is visible in main ino file.
#define XMODEM_CRC_PROTOCOL

//class PromDevice;
class CmdStatus;

class XModem
{
  public:
    XModem(PromDevice & pd, CmdStatus & cs) : promDevice(pd), cmdStatus(cs) {}
    uint32_t ReceiveFile(uint32_t address);
    bool SendFile(uint32_t address, uint32_t fileSize);
    void Cancel();

  private:
    PromDevice & promDevice;
    CmdStatus & cmdStatus;

    int GetChar(int msWaitTime = 3000);
    uint16_t UpdateCrc(uint16_t crc, uint8_t data);
    bool StartReceive();
    int ReceivePacket(uint8_t buffer[], unsigned bufferSize, uint8_t seq, uint32_t destAddr);
    void SendPacket(uint32_t address, uint8_t seq);
};

#endif // #define INCLUDE_CONFIGURE_H
