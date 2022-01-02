#ifndef __SBL_DEVICE_H__
#define __SBL_DEVICE_H__
/******************************************************************************
*  Filename:       sbl_device.h
*  Revised:        $Date$
*  Revision:       $Revision$
*
*  Description:    Serial Bootloader device header file.
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
#include "serialib.h"
//
// Typedefs for callback functions to report status and progress to application
//
typedef void (*tStatusFPTR)(char *pcText, bool bError);
typedef void (*tProgressFPTR)(uint32_t ui32Value);

class SblDevice
{
public:
    // Constructor
    ~SblDevice();

    // Static functions
    static SblDevice *Create(uint32_t ui32ChipType);
    virtual uint32_t connect(std::string csPortNum, uint32_t ui32BaudRate, bool bEnableXosc = false);
    virtual uint32_t ping() = 0;
    virtual uint32_t readStatus(uint32_t *pui32Status) = 0;
    virtual uint32_t readDeviceId(uint32_t *pui32DeviceId) = 0;
    virtual uint32_t readFlashSize(uint32_t *pui32FlashSize) = 0;
    virtual uint32_t readRamSize(uint32_t *pui32RamSize) = 0;
    virtual uint32_t run(uint32_t ui32Address) { (void)ui32Address; return SBL_UNSUPPORTED_FUNCTION; };
    virtual uint32_t reset() = 0;
    virtual uint32_t eraseFlashRange(uint32_t ui32StartAddress, uint32_t ui32ByteCount) { (void)ui32StartAddress; (void)ui32ByteCount; return 0; };
    virtual uint32_t writeFlashRange(uint32_t ui32StartAddress, uint32_t ui32ByteCount, const char *pcData) { (void)ui32StartAddress; (void)ui32ByteCount; (void)pcData; return 0; };
    virtual uint32_t readMemory32(uint32_t ui32StartAddress, uint32_t ui32UnitCount, uint32_t *pui32Data) = 0;
    virtual uint32_t readMemory8(uint32_t ui32StartAddress, uint32_t ui32UnitCount, char *pcData) = 0;
    virtual uint32_t writeMemory32(uint32_t ui32StartAddress, uint32_t ui32UnitCount, const uint32_t *pui32Data) = 0;
    virtual uint32_t writeMemory8(uint32_t ui32StartAddress, uint32_t ui32UnitCount, const char *pcData) = 0;
    virtual uint32_t calculateCrc32(uint32_t ui32StartAddress, uint32_t ui32ByteCount, uint32_t *pui32Crc) = 0;

    // CC2650 specific
    virtual uint32_t eraseFlashBank(){ return SBL_UNSUPPORTED_FUNCTION; };
    virtual uint32_t setCCFG(uint32_t ui32Field, uint32_t ui32FieldValue) { (void)ui32Field; (void)ui32FieldValue; return SBL_UNSUPPORTED_FUNCTION; };

    // CC2538 specific
    virtual uint32_t setXosc() { return SBL_UNSUPPORTED_FUNCTION; };

    // Utility functions
    bool isConnected();
    uint32_t getDeviceId() { return m_deviceId; } 
    uint32_t getFlashSize() { return m_flashSize; }
    uint32_t getRamSize() { return m_ramSize; }
    uint32_t getBaudRate() { return m_baudRate; }
    uint32_t getLastStatus() {return m_lastSblStatus; }
    uint32_t getLastDeviceStatus() { return m_lastDeviceStatus; }
    uint32_t getPageEraseSize() { return m_pageEraseSize; }
    static std::string &getLastError(void) { return sm_csLastError;}
    static uint32_t getProgress() { return sm_progress; }
    static uint32_t setProgress(uint32_t ui32Progress);
    static void setCallBackStatusFunction(tStatusFPTR pSf) {sm_pStatusFunction = pSf; }
    static void setCallBackProgressFunction(tProgressFPTR pPf) {sm_pProgressFunction = pPf; }

protected:
    // Constructor
    SblDevice();

    virtual uint32_t initCommunication(bool bSetXosc) = 0;
    virtual uint32_t sendCmd(uint32_t ui32Cmd, const char *pcSendData = NULL, uint32_t ui32SendLen = 0) = 0;
    virtual uint32_t sendAutoBaud(bool &bBaudSetOk);
    virtual uint32_t getCmdResponse(bool &bAck, uint32_t ui32MaxRetries = SBL_DEFAULT_RETRY_COUNT, bool bQuiet = false);
    virtual uint32_t sendCmdResponse(bool bAck);
    virtual uint32_t getResponseData(char *pcData, uint32_t &ui32MaxLen, uint32_t ui32MaxRetries = SBL_DEFAULT_RETRY_COUNT);

    virtual uint8_t generateCheckSum(uint32_t ui32Cmd, const char *pcData, uint32_t ui32DataLen);
    virtual uint32_t addressToPage(uint32_t ui32Address) = 0;
    virtual bool addressInRam(uint32_t ui32StartAddress, uint32_t ui32ByteCount = 1) = 0;
    virtual bool addressInFlash(uint32_t ui32StartAddress, uint32_t ui32ByteCount = 1) = 0;

    virtual uint32_t getBootloaderEnableAddress() = 0;

    uint32_t setState(const uint32_t &ui32Status) { m_lastSblStatus = ui32Status; return m_lastSblStatus;}
    uint32_t setState(const uint32_t &ui32Status, char *pcFormat, ...);

    // Utility
    static uint32_t charArrayToUL(const char *pcSrc);
    static void ulToCharArray(const uint32_t ui32Src, char *pcDst);
    static void byteSwap(char *pcArray);

	serialib *m_pCom;
	std::string m_csComPort;
    bool        m_bCommInitialized;
    uint32_t    m_baudRate;

    static uint32_t sm_chipType;
    uint32_t    m_deviceId;
    uint32_t    m_flashSize;
    uint32_t    m_ramSize;
    uint32_t    m_pageEraseSize;

    // Status and progress variables
    int32_t                 m_lastDeviceStatus;
    int32_t                 m_lastSblStatus;
    static uint32_t         sm_progress;
    static std::string      sm_csLastError;
    static tProgressFPTR    sm_pProgressFunction;
    static tStatusFPTR      sm_pStatusFunction;

private:

};

#endif // __SBL_DEVICE_H__
