#ifndef __SBL_DEVICE_CC2538_H__
#define __SBL_DEVICE_CC2538_H__
/******************************************************************************
*  Filename:       sbl_device_cc2538.h
*  Revised:        $Date$
*  Revision:       $Revision$
*
*  Description:    Serial Bootloader for CC2538 header file.
*
*  Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/
*
*
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
*
*    Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
*
*    Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
*
*    Neither the name of Texas Instruments Incorporated nor the names of
*    its contributors may be used to endorse or promote products derived
*    from this software without specific prior written permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
*  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
*  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
*  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
*  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
*  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
*  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
*  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
*  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
******************************************************************************/
#include <sbl_device.h>

//
// For more information about the CC2538 serial bootloader interface,
// please refer to the CC2538 ROM User's guide (http://www.ti.com/lit/swru333)
//

#define SBL_CC2538_PAGE_ERASE_SIZE          2048
#define SBL_CC2538_FLASH_START_ADDRESS      0x00200000
#define SBL_CC2538_RAM_START_ADDRESS        0x20000000
#define SBL_CC2538_ACCESS_WIDTH_4B          4
#define SBL_CC2538_ACCESS_WIDTH_1B          1
#define SBL_CC2538_PAGE_ERASE_TIME_MS       20
#define SBL_CC2538_MAX_BYTES_PER_TRANSFER   252
#define SBL_CC2538_DIECFG0                  0x400D3014
#define SBL_CC2538_BL_CONFIG_PAGE_OFFSET    2007
#define SBL_CC2538_BL_CONFIG_ENABLED_BM     0x10

class SblDeviceCC2538 : public SblDevice
{
public:
    SblDeviceCC2538();  // Constructor
    ~SblDeviceCC2538(); // Destructor

    enum {
        CMD_PING             = 0x20,
        CMD_DOWNLOAD         = 0x21,
        CMD_RUN              = 0x22,
        CMD_GET_STATUS       = 0x23,
        CMD_SEND_DATA        = 0x24,
        CMD_RESET            = 0x25,
        CMD_ERASE            = 0x26,
        CMD_CRC32            = 0x27,
        CMD_GET_CHIP_ID      = 0x28,
        CMD_SET_XOSC         = 0x29,
        CMD_MEMORY_READ      = 0x2A,
        CMD_MEMORY_WRITE     = 0x2B,
    };

    enum {
        CMD_RET_SUCCESS      = 0x40,
        CMD_RET_UNKNOWN_CMD  = 0x41,
        CMD_RET_INVALID_CMD  = 0x42,
        CMD_RET_INVALID_ADR  = 0x43,
        CMD_RET_FLASH_FAIL   = 0x44,
    };

protected:

    // Virtual functions from SblDevice
    uint32_t ping();

    uint32_t readStatus(uint32_t *pui32Status);
    uint32_t readDeviceId(uint32_t *pui32DeviceId);
    uint32_t readFlashSize(uint32_t *pui32FlashSize);
    uint32_t readRamSize(uint32_t *pui32RamSize);

    uint32_t run(uint32_t ui32Address);
    uint32_t reset();
    uint32_t eraseFlashRange(uint32_t ui32StartAddress, uint32_t ui32ByteCount);
    uint32_t writeFlashRange(uint32_t ui32StartAddress, uint32_t ui32ByteCount, const char *pcData);
    uint32_t readMemory32(uint32_t ui32StartAddress, uint32_t ui32UnitCount, uint32_t *pui32Data);
    uint32_t readMemory8(uint32_t ui32StartAddress, uint32_t ui32UnitCount, char *pcData);
    uint32_t writeMemory32(uint32_t ui32StartAddress, uint32_t ui32UnitCount, const uint32_t *pui32Data);
    uint32_t writeMemory8(uint32_t ui32StartAddress, uint32_t ui32UnitCount, const char *pcData);

    uint32_t calculateCrc32(uint32_t ui32StartAddress, uint32_t ui32ByteCount, uint32_t *pui32Crc);

    uint32_t sendCmd(uint32_t ui32Cmd, const char *pcSendData = NULL, uint32_t ui32SendLen = 0);
    uint32_t addressToPage(uint32_t ui32Address);
    bool     addressInRam(uint32_t ui32StartAddress, uint32_t ui32ByteCount = 1);
    bool     addressInFlash(uint32_t ui32StartAddress, uint32_t ui32ByteCount = 1);
    uint32_t setXosc();

private:
    uint32_t initCommunication(bool bSetXosc);
    uint32_t cmdDownload(uint32_t ui32Address, uint32_t ui32Size);
    uint32_t cmdSendData(const char *pcData, uint32_t ui32ByteCount);

    uint32_t getBootloaderEnableAddress();

    std::string getCmdString(uint32_t ui32Cmd);
    std::string getCmdStatusString(uint32_t ui32Status);
};

#endif // __SBL_DEVICE_CC2538_H__
