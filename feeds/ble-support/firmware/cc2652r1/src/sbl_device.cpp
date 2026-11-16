/******************************************************************************
*  Filename:       sbl_device.cpp
*  Revised:        $Date$
*  Revision:       $Revision$
*
*  Description:    Serial Bootloader device file.
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

#include <sbllib.h>
#include "sbl_device.h"
#include "sbl_device_cc2538.h"
#include "sbl_device_cc2650.h"
#include "sbl_device_cc2652.h"

#if 0
	#include <ComPort.h>
	#include <ComPortElement.h>
#else
	#include "serialib.h"
#endif

#include <stdarg.h>

// Static  variables
//
std::string     SblDevice::sm_csLastError;
uint32_t        SblDevice::sm_progress = 0;
tProgressFPTR   SblDevice::sm_pProgressFunction = NULL;
tStatusFPTR     SblDevice::sm_pStatusFunction = NULL;
uint32_t        SblDevice::sm_chipType = 0;

//-----------------------------------------------------------------------------
/** \brief Constructor
*/
//-----------------------------------------------------------------------------
SblDevice::SblDevice()
{
    m_pCom = NULL;
    m_lastDeviceStatus = -1;
    m_lastSblStatus = SBL_SUCCESS;
    m_bCommInitialized = false;
    m_deviceId = 0;
    m_ramSize = 0;
    m_flashSize = 0;
    m_pageEraseSize = 0;
}


//-----------------------------------------------------------------------------
/** \brief Destructor
*/
//-----------------------------------------------------------------------------
SblDevice::~SblDevice()
{
    if (m_pCom != nullptr) 
    { 
        delete m_pCom; 
        m_pCom = nullptr; 
    }

    m_lastDeviceStatus = -1;
    m_bCommInitialized = false;
    m_ramSize = -1;
    m_flashSize = -1;
}


//-----------------------------------------------------------------------------
/** \brief Create Serial Bootloader Device
*
* \param[in] ui32ChipType
*      Chip type the object should be created for, e.g. 0x2650 for CC2650.
*/
//-----------------------------------------------------------------------------
/*static*/SblDevice *
SblDevice::Create(uint32_t ui32ChipType)
{
    if (ui32ChipType == 0)
    {
        return NULL;
    }

    sm_chipType = ui32ChipType;

    switch (ui32ChipType)
    {
    case 0x2538:
        return (SblDevice *)new SblDeviceCC2538();
    case 0x1350:
    case 0x1310:
    case 0x2670:
    case 0x2650:
    case 0x2640:
    case 0x2630:
    case 0x2620:
        return (SblDevice *)new SblDeviceCC2650();
    case 0x1312:
    case 0x1352:
    case 0x2642:
    case 0x2652:
        return (SblDevice *)new SblDeviceCC2652();
    default:
        return NULL;
    }
}


#if 0
//-----------------------------------------------------------------------------
/** \brief Enumerate COM port devices
*
* \param[in/out]   pComPortElements
*      Pointer to array where enumerated COM devices are stored
* \param[in/out]   numElements
*      Maximum number of elements to enumerate. Is populated with number
*      of devices enumerated.
*
* \return
*
*/
//-----------------------------------------------------------------------------
/*static*/uint32_t
SblDevice::enumerate(ComPortElement *pComPortElements, int &numElements)
{
    ComPort com;
    if (com.enumerate(pComPortElements, numElements) != ComPort::COMPORT_SUCCESS)
    {
        printf("Failed to enumerate COM devices.\n");
        return SBL_ENUM_ERROR;
    }

    return SBL_SUCCESS;
}
#endif
//-----------------------------------------------------------------------------
/** \brief Connect to given port number at specified baud rate.
*
* \param[in] csPortNum
*      String containing the COM port to use
* \param[in] ui32BaudRate
*      Baud rate to use for talking to the device.
* \param[in] bEnableXosc (optional)
*      If true, try to enable device XOSC. Defaults to false. This option is
*      not available for all device types.
*
* \return
*      Returns SBL_SUCCESS, ...
*/
//-----------------------------------------------------------------------------
uint32_t
SblDevice::connect(std::string csPortNum, uint32_t ui32BaudRate,
    bool bEnableXosc/* = false*/)
{
	DEBUG_PRINT("\n");
    int retCode = SBL_SUCCESS;

    //
    // Check input arguments
    //
    if (csPortNum.empty() || ui32BaudRate == 0)
    {
        setState(SBL_ARGUMENT_ERROR, "Cannot connect. Port number '%s' or baud rate '%d' is invalid.\n",
            csPortNum.c_str(), ui32BaudRate);
        return SBL_ARGUMENT_ERROR;
    }

    // Try to connect to the specified port at the specified baud rate
    if (m_pCom != NULL)
    {
        // Try to open port
#if 1	
		char result = m_pCom->openDevice(csPortNum.c_str(),ui32BaudRate);
		if(result < 0)
		{
            setState(SBL_PORT_ERROR, "SBL: Unable to open %s. Error: %d.\n", csPortNum.c_str(), result);
            return SBL_PORT_ERROR;
		}
#else		
        if (int result = m_pCom->open(csPortNum,
            ui32BaudRate,
            SBL_DEFAULT_READ_TIMEOUT,
            SBL_DEFAULT_WRITE_TIMEOUT) != ComPort::COMPORT_SUCCESS)
        {
            setState(SBL_PORT_ERROR, "SBL: Unable to open %s. Error: %d.\n", csPortNum.c_str(), result);
            return SBL_PORT_ERROR;
        }
#endif
        m_csComPort = csPortNum;
        m_baudRate = ui32BaudRate;
    }
    // Check if device is responding at the given baud rate
    if ((retCode = initCommunication(bEnableXosc)) != SBL_SUCCESS)
    {
        return retCode;
    }

    //
    // Read device ID
    //
    uint32_t tmp;
    if ((retCode = readDeviceId(&tmp)) != SBL_SUCCESS)
    {
        setState(retCode, "Failed to read device ID during initial connect.\n");
        return retCode;
    }
    //
    // Read device flash size
    //   
    if ((retCode = readFlashSize(&tmp)) != SBL_SUCCESS)
    {
        setState(retCode, "Failed to read flash size during initial connect.\n");
        return retCode;
    }
    //
    // Read device ram size
    //
    if ((retCode = readRamSize(&tmp)) != SBL_SUCCESS)
    {
        setState(retCode, "Failed to read RAM size during initial connect.\n");
        return retCode;
    }
    return SBL_SUCCESS;
}


//-----------------------------------------------------------------------------
/** \brief Send auto baud.
*
* \param[out] bBaudSetOk
*      True if response is ACK, false otherwise
*
* \return
*      Returns SBL_SUCCESS, ...
*/
//-----------------------------------------------------------------------------
uint32_t
SblDevice::sendAutoBaud(bool &bBaudSetOk)
{
	DEBUG_PRINT("\n");
    bBaudSetOk = false;
    //
    // Send 0x55 0x55 and expect ACK
    //
    char pData[2];
    memset(pData, 0x55, 2);
    if (m_pCom->writeBytes(pData, 2) != 2)
    {
        setState(SBL_PORT_ERROR, "Communication initialization failed. Failed to send data.\n");
        return SBL_PORT_ERROR;
    }

    if (getCmdResponse(bBaudSetOk, 2, true) != SBL_SUCCESS)
    {
        // No response received. Invalid baud rate?
        setState(SBL_PORT_ERROR, "No response from device. Device may not be in bootloader mode. Reset device and try again.\nIf problem persists, check connection and baud rate.\n");
        return SBL_PORT_ERROR;
    }

    return SBL_SUCCESS;
}


//-----------------------------------------------------------------------------
/** \brief Get ACK/NAK from the boot loader.
*
* \param[out] bAck
*      True if response is ACK, false if response is NAK.
* \param[in] ui32MaxRetries (optional)
*      How many times ComPort::readBytes() can time out before fail is issued.
* \param[in] bQuietTimeout (optional)
*      Do not set error if no command response is received.
*
* \return
*      Returns SBL_SUCCESS, ...
*/
//-----------------------------------------------------------------------------
uint32_t
SblDevice::getCmdResponse(bool &bAck,
    uint32_t ui32MaxRetries/* = SBL_DEFAULT_RETRY_COUNT*/,
    bool bQuietTimeout/* = false*/)
{
	DEBUG_PRINT("\n");
    unsigned char pIn[2];
    memset(pIn, 0, 2);
    uint32_t numBytes = 0;
    uint32_t retry = 0;
    bAck = false;
    uint32_t bytesRecv = 0;

    //
    // Expect 2 bytes (ACK or NAK)
    //
    do
    {
        numBytes = m_pCom->readBytes(pIn, 2);
        bytesRecv += numBytes;
        retry++;
    } while ((bytesRecv < 2) && (retry < ui32MaxRetries));

    if (bytesRecv < 2)
    {
        if (!bQuietTimeout) setState(SBL_TIMEOUT_ERROR, "Timed out waiting for ACK/NAK. No response from device.\n");
        return SBL_TIMEOUT_ERROR;
    }
    else
    {
        if (pIn[0] == 0x00 && pIn[1] == 0xCC)
        {
            bAck = true;
            return setState(SBL_SUCCESS);
        }
        else if (pIn[0] == 0x00 && pIn[1] == 0x33)
        {
            return setState(SBL_SUCCESS);
        }
        else
        {
            setState(SBL_ERROR, "ACK/NAK not received. Expected 0x00 0xCC or 0x00 0x33, received 0x%02X 0x%02X. bytesRecv=%d\n", pIn[0], pIn[1], bytesRecv);
            return SBL_ERROR;
        }
    }
    return SBL_ERROR;
}


//-----------------------------------------------------------------------------
/** \brief Send command response (ACK/NAK).
*
* \param[in] bAck
*      True if response is ACK, false if response is NAK.
* \return
*      Returns SBL_SUCCESS, ...
*/
//-----------------------------------------------------------------------------
uint32_t
SblDevice::sendCmdResponse(bool bAck)
{
	DEBUG_PRINT("\n");
    //
    // Send response
    //
    char pData[2];
    pData[0] = 0x00;
    pData[1] = (bAck) ? 0xCC : 0x33;

    if (m_pCom->writeBytes(pData, 2) != 2)
    {
        setState(SBL_PORT_ERROR, "Failed to send ACK/NAK response over %s\n",
            m_csComPort.c_str());
        return SBL_PORT_ERROR;
    }
    return SBL_SUCCESS;
}

//-----------------------------------------------------------------------------
/** \brief Get response data from device.
*
* \param[out] pcData
*      Pointer to where received data will be stored.
* \param[in|out] ui32MaxLen
*      Max number of bytes that can be received. Is populated with the actual
*      number of bytes received.
* \param[in] ui32MaxRetries (optional)
*      How many times ComPort::readBytes() can time out before fail is issued.
* \return
*      Returns SBL_SUCCESS, ...
*/
//-----------------------------------------------------------------------------
uint32_t
SblDevice::getResponseData(char *pcData, uint32_t &ui32MaxLen,
    uint32_t ui32MaxRetries/* = SBL_DEFAULT_RETRY_COUNT*/)
{
	DEBUG_PRINT("\n");
    uint32_t numBytes = 0;
    uint32_t retry = 0;
    unsigned char pcHdr[2];
    uint32_t numPayloadBytes;
    uint8_t hdrChecksum, dataChecksum;
    uint32_t bytesRecv = 0;

    setState(SBL_SUCCESS);
    //
    // Read length and checksum
    //
    memset(pcHdr, 0, 2);
    do
    {
        bytesRecv += m_pCom->readBytes(&pcHdr[bytesRecv], (2 - bytesRecv));
        retry++;
    } while ((bytesRecv < 2) && retry < ui32MaxRetries);

    //
    // Check that we've received 2 bytes
    //
    if (bytesRecv < 2)
    {
        setState(SBL_TIMEOUT_ERROR, "Timed out waiting for data header from device.\n");
        return SBL_TIMEOUT_ERROR;
    }
    numPayloadBytes = pcHdr[0] - 2;
    hdrChecksum = pcHdr[1];

    //
    // Check if length byte is too long.
    //
    if (numPayloadBytes > ui32MaxLen)
    {
        setState(SBL_ERROR, "Error: Device sending more data than expected. \nMax expected was %d, sent was %d.\n", (uint32_t)ui32MaxLen, (numPayloadBytes + 2));
#if 1
        m_pCom->flushReceiver();
#endif		
        return SBL_ERROR;
    }

    //
    // Read the payload data
    //
    bytesRecv = 0;
    do
    {
        bytesRecv += m_pCom->readBytes(&pcData[bytesRecv], (numPayloadBytes - bytesRecv));
        retry++;
    } while (bytesRecv < numPayloadBytes && retry < ui32MaxRetries);

    //
    // Have we received what we expected?
    //
    if (bytesRecv < numPayloadBytes)
    {
        ui32MaxLen = bytesRecv;
        setState(SBL_TIMEOUT_ERROR, "Timed out waiting for data from device.\n");
        return SBL_TIMEOUT_ERROR;
    }

    //
    // Verify data checksum
    //
    dataChecksum = generateCheckSum(0, pcData, numPayloadBytes);
    if (dataChecksum != hdrChecksum)
    {
        setState(SBL_ERROR, "Checksum verification error. Expected 0x%02X, got 0x%02X.\n", hdrChecksum, dataChecksum);
        return SBL_ERROR;
    }

    ui32MaxLen = bytesRecv;
    return SBL_SUCCESS;
}


//-----------------------------------------------------------------------------
/** \brief Are we connected to the device?
*
* \return
*      Returns true if connected to device.
*      Returns false if not connected to device.
*/
//-----------------------------------------------------------------------------
bool
SblDevice::isConnected()
{
	DEBUG_PRINT("\n");
    if (!m_pCom)
    {
        return false;
    }
    return true;
}


//-----------------------------------------------------------------------------
/** \brief This function generates the bootloader protocol checksum.
*
* \param[in] ui32Cmd
*      The bootloader command
* \param[in] pcData
*      Pointer to the command data.
* \param[in] ui32DataLen
*      Data length in bytes.
*
* \return
*      Returns the generated checksum.
*/
//-----------------------------------------------------------------------------
uint8_t
SblDevice::generateCheckSum(uint32_t ui32Cmd, const char *pcData,
    uint32_t ui32DataLen)
{
	DEBUG_PRINT("\n");
    uint8_t ui8CheckSum = (uint8_t)ui32Cmd;
    for (uint32_t i = 0; i < ui32DataLen; i++)
    {
        ui8CheckSum += pcData[i];
    }
    return ui8CheckSum;
}


//-----------------------------------------------------------------------------
/** \brief This function sets the SBL status and the SBL error string.
*
* \param[in] ui32Status
*      The new SBL status. SBL_SUCCESS, SBL_ERROR, ...
* \param[in] pcFormat
*      'printf' like format string.
* \param[in] ...
*      Input variables to the \e pcFormat string.
*
* \return
*      Returns SBL_SUCCESS, ...
*/
//-----------------------------------------------------------------------------
uint32_t
SblDevice::setState(const uint32_t &ui32Status, char *pcFormat, ...)
{
	DEBUG_PRINT("\n");
    va_list args;
    char text[2048];

    m_lastSblStatus = ui32Status;

    // Attempt to do a sanity check. Not possible to say how long
    // the formatted text will be, but if we reserve half the space for 
    // formatted arguments it should be sufficient.
    if (strlen(pcFormat) > 2048 / 2) {
        return SBL_ERROR;
    }

    va_start(args, pcFormat);
#if 0	
    vsprintf_s(text, pcFormat, args);
#else
	vsprintf(text, pcFormat, args);
#endif
    sm_csLastError = text;
    va_end(args);

    if (SblDevice::sm_pStatusFunction != NULL)
    {
        bool error = (m_lastSblStatus == SBL_SUCCESS) ? false : true;
        sm_pStatusFunction((char *)sm_csLastError.c_str(), error);
    }

    return SBL_SUCCESS;
}


//-----------------------------------------------------------------------------
/** \brief Utility function for converting 4 elements in char array into
*      32 bit variable. Data are converted MSB, that is. \e pcSrc[0] is the
*      most significant byte.
*
* \param pcSrc[in]
*      A pointer to the source array.
*
* \return
*      Returns the 32 bit variable.
*/
//-----------------------------------------------------------------------------
/*static */uint32_t
SblDevice::charArrayToUL(const char *pcSrc)
{
	DEBUG_PRINT("\n");
    uint32_t ui32Val = (unsigned char)pcSrc[3];
    ui32Val += (((unsigned long)pcSrc[2]) & 0xFF) << 8;
    ui32Val += (((unsigned long)pcSrc[1]) & 0xFF) << 16;
    ui32Val += (((unsigned long)pcSrc[0]) & 0xFF) << 24;
    return (ui32Val);
}


//-----------------------------------------------------------------------------
/** \brief Utility function for splitting 32 bit variable into char array
*      (4 elements). Data are converted MSB, that is, \e pcDst[0] is the
*      most significant byte.
*
* \param[in] ui32Src
*      The 32 bit variable to convert.
*
* \param[out] pcDst
*      Pointer to the char array where the data will be stored.
*
* \return
*      void
*/
//-----------------------------------------------------------------------------
/*static */void
SblDevice::ulToCharArray(const uint32_t ui32Src, char *pcDst)
{
	DEBUG_PRINT("\n");
    // MSB first
    pcDst[0] = (uint8_t)(ui32Src >> 24);
    pcDst[1] = (uint8_t)(ui32Src >> 16);
    pcDst[2] = (uint8_t)(ui32Src >> 8);
    pcDst[3] = (uint8_t)(ui32Src >> 0);
}


//-----------------------------------------------------------------------------
/** \brief Utility function for swapping the byte order of a 4B char array.
*
* \param[in|out] pcArray
*      The char array to byte swap.
*
* \return
*      void
*/
//-----------------------------------------------------------------------------
/*static */void
SblDevice::byteSwap(char *pcArray)
{
	DEBUG_PRINT("\n");
    uint8_t tmp[2] = { (uint8_t)pcArray[0], (uint8_t)pcArray[1] };
    pcArray[0] = pcArray[3];
    pcArray[1] = pcArray[2];
    pcArray[2] = tmp[1];
    pcArray[3] = tmp[0];
}


//-----------------------------------------------------------------------------
/** \brief This functions sets the SBL progress.
*
* \param[in] ui32Progress
*      The current progress, typically in percent [0-100].
*
* \return
*      void
*/
//-----------------------------------------------------------------------------
/*static*/uint32_t
SblDevice::setProgress(uint32_t ui32Progress)
{
	DEBUG_PRINT("\n");
    if (sm_pProgressFunction)
    {
        sm_pProgressFunction(ui32Progress);
    }

    sm_progress = ui32Progress;

    return SBL_SUCCESS;
}
