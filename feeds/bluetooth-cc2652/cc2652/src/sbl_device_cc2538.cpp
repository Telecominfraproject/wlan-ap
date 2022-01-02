/******************************************************************************
*  Filename:       sbl_device_cc2538.cpp
*  Revised:        $Date$
*  Revision:       $Revision$
*
*  Description:    Serial Bootloader device file for CC2538
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
#include "serialib.h"
//#include <ComPort.h>

#include <vector>


/// Struct used when splitting long transfers
typedef struct {
    uint32_t startAddr;
    uint32_t byteCount;
    uint32_t startOffset;
    bool     bExpectAck;
} tTransfer;

//-----------------------------------------------------------------------------
/** \brief Constructor
 */
//-----------------------------------------------------------------------------
SblDeviceCC2538::SblDeviceCC2538()
{
	DEBUG_PRINT("\n");
    m_pageEraseSize = SBL_CC2538_PAGE_ERASE_SIZE;
}

//-----------------------------------------------------------------------------
/** \brief Destructor
 */
//-----------------------------------------------------------------------------
SblDeviceCC2538::~SblDeviceCC2538()
{
	DEBUG_PRINT("\n");
}


//-----------------------------------------------------------------------------
/** \brief This function sends ping command to device.
 *
 * \return
 *      Returns SBL_SUCCESS, ...
 */
//-----------------------------------------------------------------------------
uint32_t
SblDeviceCC2538::ping()
{
	DEBUG_PRINT("\n");
    int retCode = SBL_SUCCESS;
    bool bResponse = false;

    if(!isConnected())
    {
        return SBL_PORT_ERROR;
    }

    //
    // Send command
    //
    if((retCode = sendCmd(SblDeviceCC2538::CMD_PING)) != SBL_SUCCESS)
    {
        return retCode;
    }

    //
    // Get response
    //
    if((retCode = getCmdResponse(bResponse)) != SBL_SUCCESS)
    {
        return retCode;
    }

    return (bResponse) ? SBL_SUCCESS : SBL_ERROR;
}


//-----------------------------------------------------------------------------
/** \brief This function gets status from device.
 *
 * \param[out] pStatus
 *      Pointer to where status is stored.
 * \return
 *      Returns SBL_SUCCESS, ...
 */
//-----------------------------------------------------------------------------
uint32_t
SblDeviceCC2538::readStatus(uint32_t *pui32Status)
{
	DEBUG_PRINT("\n");
    uint32_t retCode = SBL_SUCCESS;
    bool bSuccess = false;

    if(!isConnected())
    {
        return SBL_PORT_ERROR;
    }

    //
    // Send command
    //
    if((retCode = sendCmd(SblDeviceCC2538::CMD_GET_STATUS)) != SBL_SUCCESS)
    {
        return retCode;        
    }

    //
    // Receive command response
    //
    if((retCode = getCmdResponse(bSuccess)) != SBL_SUCCESS)
    {
        return retCode;
    }

    if(!bSuccess)
    {
        return SBL_ERROR;
    }

    //
    // Receive command response data
    //
    char status = 0;
    uint32_t ui32NumBytes = 1;
    if((retCode = getResponseData(&status, ui32NumBytes)) != SBL_SUCCESS)
    {
        //
        // Respond with NAK
        //
        sendCmdResponse(false);
        return retCode;
    }

    //
    // Respond with ACK
    //
    sendCmdResponse(true);

    m_lastDeviceStatus = status;
    *pui32Status = status;
    return SBL_SUCCESS;
}


//-----------------------------------------------------------------------------
/** \brief This function reads device ID.
 *
 * \param[out] pui32DeviceId
 *      Pointer to where device ID is stored.
 * \return
 *      Returns SBL_SUCCESS, ...
 */
//-----------------------------------------------------------------------------
uint32_t 
SblDeviceCC2538::readDeviceId(uint32_t *pui32DeviceId)
{
	DEBUG_PRINT("\n");
    int retCode = SBL_SUCCESS;
    bool bSuccess = false;

    if(!isConnected())
    {
        return SBL_PORT_ERROR;
    }

    //
    // Send command
    //
    if((retCode = sendCmd(SblDeviceCC2538::CMD_GET_CHIP_ID)) != SBL_SUCCESS)
    {
        return retCode;        
    }

    //
    // Receive command response (ACK/NAK)
    //
    if((retCode = getCmdResponse(bSuccess)) != SBL_SUCCESS)
    {
        return retCode;
    }
    if(!bSuccess)
    {
        return SBL_ERROR;
    }

    //
    // Receive response data
    //
    char pId[4];
    memset(pId, 0, 4);
    uint32_t numBytes = 4;
    if((retCode = getResponseData(pId, numBytes)) != SBL_SUCCESS)
    {
        //
        // Respond with NAK
        //
        sendCmdResponse(false);
        return retCode;
    }

    if(numBytes != 4)
        {
            //
            // Respond with NAK
            //
            sendCmdResponse(false);
            setState(SBL_ERROR, "Didn't receive 4 B.\n");
            return SBL_ERROR;
        }

    //
    // Respond with ACK
    //
    sendCmdResponse(true);

    //
    // Store retrieved ID and report success
    //
    *pui32DeviceId = charArrayToUL(pId);
    m_deviceId = *pui32DeviceId;
    return SBL_SUCCESS;
}


//-----------------------------------------------------------------------------
/** \brief This function reads device FLASH size in bytes.
 *
 * \param[out] pui32FlashSize
 *      Pointer to where FLASH size is stored.
 * \return
 *      Returns SBL_SUCCESS, ...
 */
//-----------------------------------------------------------------------------
uint32_t 
SblDeviceCC2538::readFlashSize(uint32_t *pui32FlashSize)
{
	DEBUG_PRINT("\n");
    uint32_t retCode = SBL_SUCCESS;

    //
    // Read CC2538 DIECFG0 (contains FLASH size information)
    //
    uint32_t addr = SBL_CC2538_DIECFG0;
    uint32_t value;
    if((retCode = readMemory32(addr, 1, &value)) != SBL_SUCCESS)
    {
        setState((tSblStatus)retCode, "Failed to read device FLASH size: %s", getLastError().c_str());
        return retCode;
    }

    //
    // Calculate FLASH size (FLASH size bits are at bits [6:4])
    //
    value = ((value >> 4) & 0x07);
    switch(value)
    {
    case 1: *pui32FlashSize = 0x20000; break;   // 128 KB
    case 2: *pui32FlashSize = 0x40000; break;   // 256 KB
    case 3: *pui32FlashSize = 0x60000; break;   // 384 KB
    case 4: *pui32FlashSize = 0x80000; break;   // 512 KB
    case 0:                                 //  64 KB
    default:*pui32FlashSize = 0x10000; break;   // All invalid values are interpreted as 64 KB
    }

    m_flashSize = *pui32FlashSize;

    return SBL_SUCCESS;
}


//-----------------------------------------------------------------------------
/** \brief This function reads device RAM size in bytes.
 *
 * \param[out] pui32RamSize
 *      Pointer to where RAM size is stored.
 * \return
 *      Returns SBL_SUCCESS, ...
 */
//-----------------------------------------------------------------------------
uint32_t 
SblDeviceCC2538::readRamSize(uint32_t *pui32RamSize)
{
	DEBUG_PRINT("\n");
    int retCode = SBL_SUCCESS;

    //
    // Read CC2538 DIECFG0 (contains RAM size information
    //
    uint32_t addr = SBL_CC2538_DIECFG0;
    uint32_t value;
    if((retCode = readMemory32(addr, 1, &value)) != SBL_SUCCESS)
    {
        setState(retCode, "Failed to read device RAM size: %s", getLastError().c_str());
        return retCode;
    }

    //
    // Calculate RAM size in bytes (Ram size bits are at bits [9:7])
    //
    value = ((value >> 7) & 0x07);
    switch(value)
    {
    case 4: *pui32RamSize = 0x8000; break;    // 32 KB
    case 0: *pui32RamSize = 0x4000; break;    // 16 KB
    case 1: *pui32RamSize = 0x2000; break;    // 8 KB
    default:*pui32RamSize = 0x2000; break;    // All invalid values are interpreted as 8 KB
    }

    m_ramSize = *pui32RamSize;

    return retCode;
}


//-----------------------------------------------------------------------------
/** \brief This function makes the device run from the address given by
 *      \u ui32Address, transferring execution control away from the 
 *      bootloader. No further bootloader access will be possible.
 *
 * \parameter[in] ui32Address
 *      The device address where execution will be transferred to.
 * \return
 *      Returns SBL_SUCCESS, ...
 */
//-----------------------------------------------------------------------------
uint32_t
SblDeviceCC2538::run(uint32_t ui32Address)
{
	DEBUG_PRINT("\n");
    int retCode = SBL_SUCCESS;
    bool bSuccess = false;

    if(!isConnected())
    {
        return SBL_PORT_ERROR;
    }

    //
    // Generate payload
    // - 4B address
    //
    char pcPayload[4];
    ulToCharArray(ui32Address, &pcPayload[0]);

    //
    // Send command
    //
    if((retCode = sendCmd(SblDeviceCC2538::CMD_RUN, pcPayload, 4)) != SBL_SUCCESS)
    {
        return retCode;        
    }

    //
    // Receive command response (ACK/NAK)
    //
    if((retCode = getCmdResponse(bSuccess)) != SBL_SUCCESS)
    {
        return retCode;
    }
    if(!bSuccess)
    {
        setState(SBL_ERROR, "Run command NAKed by device.\n");
        return SBL_ERROR;
    }

    m_bCommInitialized = false;
    return SBL_SUCCESS;

}

//-----------------------------------------------------------------------------
/** \brief This function reset the device. Communication to the device must be 
 *      reinitialized after calling this function.
 *
 * \return
 *      Returns SBL_SUCCESS, ...
 */
//-----------------------------------------------------------------------------
uint32_t
SblDeviceCC2538::reset()
{
	DEBUG_PRINT("\n");
    int retCode = SBL_SUCCESS;
    bool bSuccess = false;

    if(!isConnected())
    {
        return SBL_PORT_ERROR;
    }

    //
    // Send command
    //
    if((retCode = sendCmd(SblDeviceCC2538::CMD_RESET)) != SBL_SUCCESS)
    {
        return retCode;        
    }

    //
    // Receive command response (ACK/NAK)
    //
    if((retCode = getCmdResponse(bSuccess)) != SBL_SUCCESS)
    {
        return retCode;
    }
    if(!bSuccess)
    {
        setState(SBL_ERROR, "Reset command NAKed by device.\n");
        return SBL_ERROR;
    }

    m_bCommInitialized = false;
    return SBL_SUCCESS;
}


//-----------------------------------------------------------------------------
/** \brief This function erases device flash pages. Starting page is the page 
 *      that includes the address in \e startAddress. Ending page is the page 
 *      that includes the address <startAddress + byteCount>. CC2538 erase 
 *      size is 2KB.
 *
 * \param[in] ui32StartAddress
 *      The start address in flash.
 * \param[in] ui32ByteCount
 *      The number of bytes to erase.
 *
 * \return
 *      Returns SBL_SUCCESS, ...
 */
//-----------------------------------------------------------------------------
uint32_t
SblDeviceCC2538::eraseFlashRange(uint32_t ui32StartAddress, 
                                 uint32_t ui32ByteCount)
{
	DEBUG_PRINT("\n");
    uint32_t retCode = SBL_SUCCESS;
    bool bSuccess = false;
    char pcPayload[8];
    uint32_t devStatus;

    //
    // Initial check
    // 
    if(!isConnected())
    {
        return SBL_PORT_ERROR;
    }

    //
    // Calculate retry count
    //
    uint32_t ui32PageCount = ui32ByteCount / SBL_CC2538_PAGE_ERASE_SIZE;
    if( ui32ByteCount % SBL_CC2538_PAGE_ERASE_SIZE) ui32PageCount ++;
    uint32_t ui32TryCount = (((ui32PageCount * SBL_CC2538_PAGE_ERASE_TIME_MS) / \
                             SBL_DEFAULT_READ_TIMEOUT) + 1);

    //
    // Build payload
    // - 4B address (MSB first)
    // - 4B byte count (MSB first)
    //
    ulToCharArray(ui32StartAddress, &pcPayload[0]);
    ulToCharArray(ui32ByteCount, &pcPayload[4]);

    //
    // Set progress
    //
    setProgress(0);

    //
    // Send command
    //
    if((retCode = sendCmd(SblDeviceCC2538::CMD_ERASE, pcPayload, 8)) != SBL_SUCCESS)
    {
        return retCode;        
    }

    //
    // Receive command response (ACK/NAK)
    //
    if((retCode = getCmdResponse(bSuccess, ui32TryCount)) != SBL_SUCCESS)
    {
        return retCode;
    }
    if(!bSuccess)
    {
        return SBL_ERROR;
    }

    //
    // Check device status (Flash failed if page(s) locked)
    //
    readStatus(&devStatus);
    if(devStatus != SblDeviceCC2538::CMD_RET_SUCCESS)
    {
        setState(SBL_ERROR, "Flash erase failed. (Status 0x%02X = '%s'). Flash pages may be locked.\n", devStatus, getCmdStatusString(devStatus).c_str());
        return SBL_ERROR;
    }

    //
    // Set progress
    //
    setProgress(100);

    return SBL_SUCCESS;
}


//-----------------------------------------------------------------------------
/** \brief This function reads \e ui32UnitCount (32 bit) words of data from 
 *      device. Destination array is 32 bit wide. The start address must be 4 
 *      byte aligned.
 *
 * \param[in] ui32StartAddress
 *      Start address in device (must be 4 byte aligned).
 * \param[in] ui32UnitCount
 *      Number of data words to read.
 * \param[out] pcData
 *      Pointer to where read data is stored.
 * \return
 *      Returns SBL_SUCCESS, ...
 */
//-----------------------------------------------------------------------------
uint32_t 
SblDeviceCC2538::readMemory32(uint32_t ui32StartAddress, uint32_t ui32UnitCount, 
                              uint32_t *pui32Data)
{
	DEBUG_PRINT("\n");
    int retCode = SBL_SUCCESS;
    bool bSuccess = false;

    //
    // Check input arguments
    //
    if((ui32StartAddress & 0x03))
    {
        setState(SBL_ARGUMENT_ERROR, "readMemory32(): Start address (0x%08X) must be a multiple of 4.\n", ui32StartAddress);
        return SBL_ARGUMENT_ERROR;
    }

    //
    // Set progress
    //
    setProgress(0);

    if(!isConnected())
    {
        return SBL_PORT_ERROR;
    }

    char pcPayload[5];
    uint32_t recvCount = 0;
    for(uint32_t i = 0; i < ui32UnitCount; i++)
    {
        //
        // Build payload
        // - 4B address (MSB first)
        // - 1B access width
        //
        ulToCharArray((ui32StartAddress + (i*4)), &pcPayload[0]);
        pcPayload[4] = SBL_CC2538_ACCESS_WIDTH_4B;

        //
        // Set progress
        //
        setProgress( ((100 * i)/ui32UnitCount) );

        //
        // Send command
        //
        if((retCode = sendCmd(SblDeviceCC2538::CMD_MEMORY_READ, pcPayload, 5)) != SBL_SUCCESS)
        {
            return retCode;        
        }

        //
        // Receive command response (ACK/NAK)
        //
        if((retCode = getCmdResponse(bSuccess)) != SBL_SUCCESS)
        {
            return retCode;
        }
        if(!bSuccess)
        {
            return SBL_ERROR;
        }

        //
        // Receive 4B response
        //
        recvCount = 4;
        if((retCode = getResponseData(pcPayload, recvCount)) != SBL_SUCCESS)
        {
            //
            // Respond with NAK
            //
            sendCmdResponse(false);
            return retCode;            
        }
        if(recvCount != 4)
        {
            //
            // Respond with NAK
            //
            sendCmdResponse(false);
            setState(SBL_ERROR, "Didn't receive 4 B.\n");
            return SBL_ERROR;
        }
        pui32Data[i] = charArrayToUL(pcPayload);

        //
        // Respond with ACK
        //
        sendCmdResponse(true);
    }

    //
    // Set progress
    //
    setProgress(100);

    return SBL_SUCCESS;
}


//-----------------------------------------------------------------------------
/** \brief This function reads \e unitCount bytes of data from device. 
 *      Destination array is 8 bit wide.
 *
 * \param[in] ui32StartAddress
 *      Start address in device.
 * \param[in] ui32UnitCount
 *      Number of bytes to read.
 * \param[out] pcData
 *      Pointer to where read data is stored.
 * \return
 *      Returns SBL_SUCCESS, ...
 */
//-----------------------------------------------------------------------------
uint32_t 
SblDeviceCC2538::readMemory8(uint32_t ui32StartAddress, uint32_t ui32UnitCount, 
                             char *pcData)
{
	DEBUG_PRINT("\n");
    int retCode = SBL_SUCCESS;
    bool bSuccess = false;

    //
    // Using 4B access width. Expanding byte count to include bytes at start 
    // and end of sequence.
    //
    uint32_t begCnt = ui32StartAddress % 4;
    uint32_t endCnt = (ui32StartAddress + ui32UnitCount) % 4;
    if(endCnt) { endCnt = 4 - endCnt; }
    uint32_t totByteCnt = begCnt + ui32UnitCount + endCnt;


    //
    // Word align start address
    //
    ui32StartAddress -= begCnt;

    //
    // Create temporary vector with enough space.
    //
    std::vector<char> tmpBuf (totByteCnt);

    //
    // Set progress
    //
    setProgress(0);

    if(!isConnected())
    {
        return SBL_PORT_ERROR;
    }

    char pcPayload[5];
    uint32_t recvCount = 0;
    for(uint32_t i = 0; i < totByteCnt/4; i++)
    {
        //
        // Build payload
        // - 4B address (MSB first)
        // - 1B access width
        //
        ulToCharArray((ui32StartAddress + (i*4)), &pcPayload[0]);
        pcPayload[4] = SBL_CC2538_ACCESS_WIDTH_4B;

        //
        // Set progress
        //
        setProgress( ((100 * i)/(totByteCnt/4)) );

        //
        // Send command
        //
        if((retCode = sendCmd(SblDeviceCC2538::CMD_MEMORY_READ, pcPayload, 5)) != SBL_SUCCESS)
        {
            return retCode;        
        }

        //
        // Receive command response (ACK/NAK)
        //
        if((retCode = getCmdResponse(bSuccess)) != SBL_SUCCESS)
        {
            return retCode;
        }
        if(!bSuccess)
        {
            return SBL_ERROR;
        }

        //
        // Receive 4B response
        //
        recvCount = 4;
        if((retCode = getResponseData(pcPayload, recvCount)) != SBL_SUCCESS)
        {
            //
            // Respond with NAK
            //
            sendCmdResponse(false);
            return retCode;
        }
        if(recvCount != 4)
        {
            //
            // Respond with NAK
            //
            sendCmdResponse(false);
            setState(SBL_ERROR, "Didn't receive 4 B.\n");
            return SBL_ERROR;
        }
        byteSwap(pcPayload);
        memcpy(&tmpBuf[4*i], pcPayload, 4);

        //
        // Respond with ACK
        //
        sendCmdResponse(true);
    }

    //
    // Copy wanted data to pcData
    //
    for(uint32_t i = 0; i < ui32UnitCount; i++) 
    {
        pcData[i] = tmpBuf.at(i + begCnt);
    }

    //
    // Set progress
    //
    setProgress(100);

    return SBL_SUCCESS;
}


//-----------------------------------------------------------------------------
/** \brief This function writes \e unitCount words of data to device SRAM. 
 *      Source array is 32 bit wide. \e ui32StartAddress must be 4 byte
 *      aligned.
 *
 * \param[in] ui32StartAddress
 *      Start address in device.
 * \param[in] ui32UnitCount
 *      Number of data words (32bit) to write.
 * \param[in] pui32Data
 *      Pointer to source data.
 * \return
 *      Returns SBL_SUCCESS, ...
 */
//-----------------------------------------------------------------------------
uint32_t 
SblDeviceCC2538::writeMemory32(uint32_t ui32StartAddress, 
                               uint32_t ui32UnitCount, 
                               const uint32_t *pui32Data)
{
	DEBUG_PRINT("\n");
    uint32_t retCode = SBL_SUCCESS;
    bool bSuccess = false;
    char pcPayload[9];
    uint32_t recvCount = 0;
    uint32_t currAddr;

    //
    // Check input arguments
    //
    if(!addressInRam(ui32StartAddress, ui32UnitCount*4))
    {
        setState(SBL_ARGUMENT_ERROR, "writeMemory32(): Address range (0x%08X + %d bytes) is not in device RAM.\n", ui32StartAddress, ui32UnitCount*4);
        return SBL_ARGUMENT_ERROR;
    }
    if((ui32StartAddress & 0x03))
    {
        setState(SBL_ARGUMENT_ERROR, "writeMemory32(): Start address (0x%08X) must be a multiple of 4.\n", ui32StartAddress);
        return SBL_ARGUMENT_ERROR;
    }

    if(!isConnected())
    {
        return SBL_PORT_ERROR;
    }

    //
    // Set progress
    //
    setProgress(0);

    for(uint32_t i = 0; i < ui32UnitCount; i++)
    {
        currAddr = ui32StartAddress + (i * 4);
        //
        // Build payload
        // - 4B address (MSB first)
        // - 4B data value (MSB first)
        // - 1B access width
        //
        ulToCharArray(currAddr, &pcPayload[0]);
        ulToCharArray(pui32Data[i], &pcPayload[4]);
        pcPayload[8] = SBL_CC2538_ACCESS_WIDTH_4B;

        //
        // Set progress
        //
        setProgress( ((100 * i)/ui32UnitCount) );

        //
        // Send command
        //
        if((retCode = sendCmd(SblDeviceCC2538::CMD_MEMORY_WRITE, pcPayload, 9)) != SBL_SUCCESS)
        {
            return retCode;        
        }

        //
        // Receive command response (ACK/NAK)
        //
        if((retCode = getCmdResponse(bSuccess, 5)) != SBL_SUCCESS)
        {
            return retCode;
        }
        if(!bSuccess)
        {
            setState(SBL_ERROR, "Device NAKed read command for address 0x%08X.\n", currAddr);
            return SBL_ERROR;
        }
    }

    //
    // Set progress
    //
    setProgress(100);

    return SBL_SUCCESS;
}


//-----------------------------------------------------------------------------
/** \brief This function writes \e unitCount words of data to device SRAM. 
 *      Source array is 8 bit wide. Parameters \e startAddress and \e unitCount 
 *      must be a a multiple of 4.
 *
 * \param[in] ui32StartAddress
 *      Start address in device.
 * \param[in] ui32UnitCount
 *      Number of bytes to write.
 * \param[in] pcData
 *      Pointer to source data.
 * \return
 *      Returns SBL_SUCCESS, ...
 */
//-----------------------------------------------------------------------------
uint32_t 
SblDeviceCC2538::writeMemory8(uint32_t ui32StartAddress, 
                              uint32_t ui32UnitCount, 
                              const char *pcData)
{
	DEBUG_PRINT("\n");
    uint32_t retCode = SBL_SUCCESS;
    bool bSuccess = false;
    char pcPayload[9];
    uint32_t recvCount = 0;
    uint32_t currAddr;

    //
    // Check input arguments
    //
    if(!addressInRam(ui32StartAddress, ui32UnitCount))
    {
        setState(SBL_ARGUMENT_ERROR, "writeMemory8(): Address range (0x%08X + %d bytes) is not in device RAM.\n", ui32StartAddress, ui32UnitCount*4);
        return SBL_ARGUMENT_ERROR;
    }
    if((ui32StartAddress & 0x03) || (ui32UnitCount & 0x03))
    {
        setState(SBL_ARGUMENT_ERROR, "writeMemory8(): Start address (0x%08X) and byte count (%d) must be a multiple of 4.\n", ui32StartAddress, ui32UnitCount);
        return SBL_ARGUMENT_ERROR;
    }

    if(!isConnected())
    {
        return SBL_PORT_ERROR;
    }

    //
    // Set progress
    //
    setProgress(0);

    for(uint32_t i = 0; i < ui32UnitCount/4; i++)
    {
        currAddr = ui32StartAddress + (i * 4);
        //
        // Build payload
        // - 4B address (MSB first)
        // - 4B data value (MSB first)
        // - 1B access width
        //
        ulToCharArray(currAddr, &pcPayload[0]);
        memcpy(&pcPayload[4], &pcData[(i * 4)], 4);
        pcPayload[8] = SBL_CC2538_ACCESS_WIDTH_4B;

        //
        // Set progress
        //
        setProgress( ((100 * i) / (ui32UnitCount / 4)) );

        //
        // Send command
        //
        if((retCode = sendCmd(SblDeviceCC2538::CMD_MEMORY_WRITE, pcPayload, 9)) != SBL_SUCCESS)
        {
            return retCode;        
        }

        //
        // Receive command response (ACK/NAK)
        //
        if((retCode = getCmdResponse(bSuccess, 5)) != SBL_SUCCESS)
        {
            return retCode;
        }
        if(!bSuccess)
        {
            setState(SBL_ERROR, "Device NAKed read command for address 0x%08X.\n", currAddr);
            return SBL_ERROR;
        }
    }

    //
    // Set progress
    //
    setProgress(100);

    return SBL_SUCCESS;
}


//-----------------------------------------------------------------------------
/** \brief This function calculates CRC over \e byteCount bytes on the device, 
 *      starting at address \e startAddress.
 *
 * \param[in] ui32StartAddress
 *      Start address in device.
 * \param[in] ui32ByteCount
 *      Number of bytes to calculate CRC32 over.
 * \param[out] pui32Crc
 *      Pointer to where checksum from device is stored.
 * \return
 *      Returns SBL_SUCCESS, ...
 */
//-----------------------------------------------------------------------------
uint32_t 
SblDeviceCC2538::calculateCrc32(uint32_t ui32StartAddress, 
                                uint32_t ui32ByteCount, uint32_t *pui32Crc)
{
	DEBUG_PRINT("\n");
    uint32_t retCode = SBL_SUCCESS;
    bool bSuccess = false;
    char pcPayload[8];
    uint32_t ui32RecvCount = 0;

    //
    // Check input arguments
    //
    if(!addressInFlash(ui32StartAddress, ui32ByteCount) &&
       !addressInRam(ui32StartAddress, ui32ByteCount))
    {
        setState(SBL_ARGUMENT_ERROR, "Specified address range (0x%08X + %d bytes) is not in device FLASH nor RAM.\n", ui32StartAddress, ui32ByteCount);
        return SBL_ARGUMENT_ERROR;
    }

    if(!isConnected())
    {
        return SBL_PORT_ERROR;
    }

    //
    // Set progress
    //
    setProgress(0);

    //
    // Build payload
    // - 4B address (MSB first)
    // - 4B byte count(MSB first)
    //
    ulToCharArray(ui32StartAddress, &pcPayload[0]);
    ulToCharArray(ui32ByteCount, &pcPayload[4]);

    //
    // Send command
    //
    if((retCode = sendCmd(SblDeviceCC2538::CMD_CRC32, pcPayload, 8)) != SBL_SUCCESS)
    {
        return retCode;        
    }

    //
    // Receive command response (ACK/NAK)
    //
    if((retCode = getCmdResponse(bSuccess, 5)) != SBL_SUCCESS)
    {
        return retCode;
    }
    if(!bSuccess)
    {
        setState(SBL_ERROR, "Device NAKed CRC32 command.\n");
        return SBL_ERROR;
    }

    //
    // Get data response
    //
    ui32RecvCount = 4;
    if((retCode = getResponseData(pcPayload, ui32RecvCount)) != SBL_SUCCESS)
    {
        sendCmdResponse(false);
        return retCode;
    }
    *pui32Crc = charArrayToUL(pcPayload);

    //
    // Send ACK/NAK to command
    //
    bool bAck = (ui32RecvCount == 4) ? true : false;
    sendCmdResponse(bAck);

    //
    // Set progress
    //
    setProgress(100);

    return SBL_SUCCESS;
}



//-----------------------------------------------------------------------------
/** \brief This function writes \e unitCount words of data to device FLASH. 
 *      Source array is 8 bit wide. Parameters \e startAddress and \e unitCount 
 *      must be a a multiple of 4. This function does not erase the flash 
 *      before writing data, this must be done using e.g. eraseFlashRange().
 *
 * \param[in] ui32StartAddress
 *      Start address in device. Must be a multiple of 4.
 * \param[in] ui32ByteCount
 *      Number of bytes to program. Must be a multiple of 4.
 * \param[in] pcData
 *      Pointer to source data.
 * \return
 *      Returns SBL_SUCCESS, ...
 */
//-----------------------------------------------------------------------------
uint32_t 
SblDeviceCC2538::writeFlashRange(uint32_t ui32StartAddress, 
                                 uint32_t ui32ByteCount, const char *pcData)
{
	DEBUG_PRINT("\n");
    uint32_t devStatus = SblDeviceCC2538::CMD_RET_UNKNOWN_CMD;
    uint32_t retCode = SBL_SUCCESS;
    uint32_t bytesLeft, dataIdx, bytesInTransfer;
    uint32_t transferNumber = 1;
    bool bIsRetry = false;
    bool bBlToBeDisabled = false;
    std::vector<tTransfer> pvTransfer;
    uint32_t ui32TotChunks = (ui32ByteCount / SBL_CC2538_MAX_BYTES_PER_TRANSFER);
    if(ui32ByteCount % SBL_CC2538_MAX_BYTES_PER_TRANSFER) ui32TotChunks++;
    uint32_t ui32CurrChunk = 0;

    //
    // Calculate BL configuration address (depends on flash size)
    //
    uint32_t ui32BlCfgAddr = getBootloaderEnableAddress();

    //
    // Calculate BL configuration buffer index
    //
    uint32_t ui32BlCfgDataIdx = ui32BlCfgAddr - ui32StartAddress;

    //
    // Is BL configuration part of buffer?
    //
    if(ui32BlCfgDataIdx <= ui32ByteCount)
    {
        if((pcData[ui32BlCfgDataIdx] & SBL_CC2538_BL_CONFIG_ENABLED_BM) == 0)
        {
            bBlToBeDisabled = true;
            setState(SBL_SUCCESS, "Warning: Bootloader will be disabled.\n");
        }
    }

    if(bBlToBeDisabled)
    {
        //
        // Split into two transfers
        //
        pvTransfer.resize(2);

        //
        // Main transfer (before lock bit)
        //
        pvTransfer[0].bExpectAck  = true;
        pvTransfer[0].startAddr   = ui32StartAddress;
        pvTransfer[0].byteCount   = (ui32BlCfgAddr - ui32StartAddress) & (~0x03);
        pvTransfer[0].startOffset = 0;

        //
        // The transfer locking the backdoor
        //
        pvTransfer[1].bExpectAck  = false;
        pvTransfer[1].startAddr   = ui32BlCfgAddr - (ui32BlCfgAddr % 4);
        pvTransfer[1].byteCount   = ui32ByteCount - pvTransfer[0].byteCount;
        pvTransfer[1].startOffset = ui32BlCfgDataIdx - (ui32BlCfgDataIdx % 4);

    }
    else
    {
        pvTransfer.resize(1);
        pvTransfer[0].bExpectAck  = true;
        pvTransfer[0].byteCount = ui32ByteCount;
        pvTransfer[0].startAddr = ui32StartAddress;
        pvTransfer[0].startOffset = 0;
    }

    //
    // For each transfer
    //
    for(uint32_t i = 0; i < pvTransfer.size(); i++)
    {
        //
        // Sanity check
        //
        if(pvTransfer[i].byteCount == 0)
        {
            continue;
        }
        
        //
        // Set progress
        //
        //setProgress(addressToPage(pvTransfer[i].startAddr));

        //
        // Send download command
        //
        if((retCode = cmdDownload(pvTransfer[i].startAddr, 
                                  pvTransfer[i].byteCount)) != SBL_SUCCESS)
        {
            return retCode;
        }

        //
        // Check status after download command
        //
        retCode = readStatus(&devStatus);
        if(retCode != SBL_SUCCESS)
        {
            setState(retCode, "Error during download initialization. Failed to read device status after sending download command.\n");
            return retCode;
        }
        if(devStatus != SblDeviceCC2538::CMD_RET_SUCCESS)
        {
            setState(SBL_ERROR, "Error during download initialization. Device returned status %d (%s).\n", devStatus, getCmdStatusString(devStatus).c_str());
            return SBL_ERROR;
        }

        //
        // Send data in chunks
        //
        bytesLeft = pvTransfer[i].byteCount;
        dataIdx   = pvTransfer[i].startOffset;
        while(bytesLeft)
        {
            //
            // Set progress
            //
            //setProgress(addressToPage(ui32StartAddress + dataIdx));
            setProgress( ((100*(++ui32CurrChunk))/ui32TotChunks) );

            //
            // Limit transfer count
            //
#if 0
            bytesInTransfer = min(SBL_CC2538_MAX_BYTES_PER_TRANSFER, bytesLeft);
#else
			if(bytesLeft > SBL_CC2538_MAX_BYTES_PER_TRANSFER)
			{
				bytesInTransfer = SBL_CC2538_MAX_BYTES_PER_TRANSFER;
			} else {
				bytesInTransfer = bytesLeft;
			}
#endif

            //
            // Send Data command
            //
            if(retCode = cmdSendData(&pcData[dataIdx], bytesInTransfer) != SBL_SUCCESS)
            {
                setState(retCode, "Error during flash download. \n- Start address 0x%08X (page %d). \n- Tried to transfer %d bytes. \n- This was transfer %d.\n", 
                         (ui32StartAddress+dataIdx), 
                         addressToPage(ui32StartAddress+dataIdx),
                         bytesInTransfer, 
                         (transferNumber));
                return retCode;
            }

            if(pvTransfer[i].bExpectAck)
            {
                //
                // Check status after send data command
                //
                devStatus = 0;
                retCode = readStatus(&devStatus);
                if(retCode != SBL_SUCCESS)
                {
                    setState(retCode, "Error during flash download. Failed to read device status.\n- Start address 0x%08X (page %d). \n- Tried to transfer %d bytes. \n- This was transfer %d in chunk %d.\n", 
                                 (ui32StartAddress+dataIdx), 
                                 addressToPage(ui32StartAddress + dataIdx),
                                 (bytesInTransfer), (transferNumber), 
                                 (i));
                    return retCode;
                }
                if(devStatus != SblDeviceCC2538::CMD_RET_SUCCESS)
                {
                    setState(SBL_SUCCESS, "Device returned status %s\n", getCmdStatusString(devStatus).c_str());
                    if(bIsRetry)
                    {
                        //
                        // We have failed a second time. Aborting.
                        setState(SBL_ERROR, "Error retrying flash download.\n- Start address 0x%08X (page %d). \n- Tried to transfer %d bytes. \n- This was transfer %d in chunk %d.\n", 
                                 (ui32StartAddress+dataIdx), 
                                 addressToPage(ui32StartAddress + dataIdx),
                                 (bytesInTransfer), (transferNumber), 
                                 (i));
                        return SBL_ERROR;
                    }

                    //
                    // Retry to send data one more time.
                    //
                    bIsRetry = true;
                    continue;
                }
            }
            else
            {
                //
                // We're locking device and will lose access
                //
                m_bCommInitialized = false;

            }

            //
            // Update index and bytesLeft
            //
            bytesLeft -= bytesInTransfer;
            dataIdx += bytesInTransfer;
            transferNumber++;
            bIsRetry = false;
        }
    }

    return SBL_SUCCESS;
}


//-----------------------------------------------------------------------------
/** \brief This function sends the specified bootloader command.
 *
 * \param[in] ui32Cmd
 *      The command to send.
 * \param[in] pcSendData
 *      Pointer to the data to send with the command.
 * \param[in] ui32SendLen
 *      The number of bytes to send from \e pcSendData.
 * \return
 *      Returns SBL_SUCCESS, ...
 */
//-----------------------------------------------------------------------------
uint32_t 
SblDeviceCC2538::sendCmd(uint32_t ui32Cmd, const char *pcSendData/* = NULL*/, 
                         uint32_t ui32SendLen/* = 0*/)
{
	DEBUG_PRINT("\n");
    unsigned char pktLen = ui32SendLen + 3; // +3 => <1b Length>, <1B checksum>, <1B cmd>
    std::vector<char> pvPkt((pktLen));
    unsigned char pktSum = generateCheckSum(ui32Cmd, pcSendData, ui32SendLen);

    //
    // Build packet
    //
    pvPkt.at(0) = pktLen;
    pvPkt.at(1) = pktSum;
    pvPkt.at(2) = (unsigned char)ui32Cmd;
    if(ui32SendLen)
    {
        memcpy(&pvPkt[3], pcSendData, ui32SendLen);
    }

    //
    // Send packet
    //
    if(m_pCom->writeBytes(&pvPkt[0], pvPkt.size()) < 1)
    {
        setState(SBL_PORT_ERROR, "Writing to device failed (Command '%s').\n", getCmdString(ui32Cmd).c_str());
        return SBL_PORT_ERROR;
    }
    

    //
    // Empty and deallocate vector
    //
    pvPkt.clear();
    std::vector<char>().swap(pvPkt);    
        
    return SBL_SUCCESS;
}


//-----------------------------------------------------------------------------
/** \brief This function initializes connection to the CC2538 device. 
 *
 * \param[in] bSetXosc
 *      If true, try to enable device XOSC.
 * \return
 *      Returns SBL_SUCCESS, ...
 */
//-----------------------------------------------------------------------------
uint32_t
SblDeviceCC2538::initCommunication(bool bSetXosc)
{
	DEBUG_PRINT("\n");
    bool bSuccess, bBaudSetOk;
    int retCode = SBL_ERROR;

    //
    // Send dummy command to see if device is already initialized at
    // this baud rate.
    //
    if(sendCmd(0) != SBL_SUCCESS)
    {
        return SBL_ERROR;
    }

    //
    // Do we get a response (ACK/NAK)?
    //
    bSuccess = false;
    if(getCmdResponse(bSuccess, SBL_DEFAULT_RETRY_COUNT, true) != SBL_SUCCESS)
    {
        //
        // No response received. Try auto baud
        //
        if(retCode = sendAutoBaud(bBaudSetOk) != SBL_SUCCESS)
        {
            return retCode;
        }
    }

    if(bSetXosc)
    {
        setState(SBL_SUCCESS, "Trying to set device XOSC.\n");
        //
        // Try to enable XOSC
        //
        if((retCode = setXosc()) != SBL_SUCCESS)
        {
            //
            // setXosc returned error
            //
            setState(retCode, "Failed to activate device XOSC.\n");
            return retCode;
        }

        //
        // Send dummy command
        //
        bSuccess = false;
        sendCmd(0);
        if(retCode = getCmdResponse(bSuccess, SBL_DEFAULT_RETRY_COUNT, true) != SBL_SUCCESS)
        {
            //
            // Send auto baud again
            //
            if((retCode = sendAutoBaud(bBaudSetOk)) != SBL_SUCCESS)
            {
                setState((tSblStatus)retCode, "Auto baud detection failed after starting XOSC.\n");
                return retCode;
            }
            setState(SBL_SUCCESS, "Device XOSC activated.\n");
        }
    }

    m_bCommInitialized = true;
    return SBL_SUCCESS;
}


//-----------------------------------------------------------------------------
/** \brief This function returns a string representation of the \e ui32Cmd
 *      command.
 *
 * \param[out] ui32Cmd
 *      The serial bootloader command.
 * \return
 *      Returns std::string with name of device command.
 */
//-----------------------------------------------------------------------------
std::string
SblDeviceCC2538::getCmdString(uint32_t ui32Cmd)
{
	DEBUG_PRINT("\n");
    switch(ui32Cmd)
    {
    case SblDeviceCC2538::CMD_PING:         return "CMD_PING"; break;
    case SblDeviceCC2538::CMD_CRC32:        return "CMD_CRC32"; break;
    case SblDeviceCC2538::CMD_DOWNLOAD:     return "CMD_DOWNLOAD"; break;
    case SblDeviceCC2538::CMD_ERASE:        return "CMD_ERASE"; break;
    case SblDeviceCC2538::CMD_GET_CHIP_ID:  return "CMD_GET_CHIP_ID"; break;
    case SblDeviceCC2538::CMD_GET_STATUS:   return "CMD_GET_STATUS"; break;
    case SblDeviceCC2538::CMD_MEMORY_READ:  return "CMD_MEMORY_READ"; break;
    case SblDeviceCC2538::CMD_MEMORY_WRITE: return "CMD_MEMORY_WRITE"; break;
    case SblDeviceCC2538::CMD_RESET:        return "CMD_RESET"; break;
    default: return "Unknown command"; break;
    }
}


//-----------------------------------------------------------------------------
/** \brief This function returns a string representation of the 
 *      \e ui32Status serial bootloader status value.
 *
 * \param[out] ui32Status
 *      The serial bootloader status value.
 * \return
 *      Returns std::string of device status.
 */
//-----------------------------------------------------------------------------
std::string
SblDeviceCC2538::getCmdStatusString(uint32_t ui32Status)
{
	DEBUG_PRINT("\n");
    switch(ui32Status)
    {
    case SblDeviceCC2538::CMD_RET_FLASH_FAIL:   return "FLASH_FAIL"; break;
    case SblDeviceCC2538::CMD_RET_INVALID_ADR:  return "INVALID_ADR"; break;
    case SblDeviceCC2538::CMD_RET_INVALID_CMD:  return "INVALID_CMD"; break;
    case SblDeviceCC2538::CMD_RET_SUCCESS:      return "SUCCESS"; break;
    case SblDeviceCC2538::CMD_RET_UNKNOWN_CMD:  return "UNKNOWN_CMD"; break;
    default: return "Unknown status"; break;
    }
}


//-----------------------------------------------------------------------------
/** \brief This function sends the CC2538 download command and handles the
 *      device response. \e ui32ByteCount must be a multiple of 4.
 *
 * \param[in] ui32Address
 *      The start address in CC2538 flash.
 * \param[in] ui32ByteCount
 *      The total number of bytes to program on the device.
 *
 * \return
 *      Returns SBL_SUCCESS if command and response was successful.
 */
//-----------------------------------------------------------------------------
uint32_t 
SblDeviceCC2538::cmdDownload(uint32_t ui32Address, uint32_t ui32Size)
{
	DEBUG_PRINT("\n");
    int retCode = SBL_SUCCESS;
    bool bSuccess = false;


    //
    // Check input arguments
    //
    if(!addressInFlash(ui32Address, ui32Size))
    {
        setState(SBL_ARGUMENT_ERROR, "Flash download: Address range (0x%08X + %d bytes) is not in device FLASH nor RAM.\n", ui32Address, ui32Size);
        return SBL_ARGUMENT_ERROR;
    }
    if(ui32Size & 0x03)
    {
        setState(SBL_ARGUMENT_ERROR, "Flash download: Byte count must be a multiple of 4\n");
        return SBL_ARGUMENT_ERROR;
    }

    //
    // Generate payload
    // - 4B Program address
    // - 4B Program size
    //
    char pcPayload[8];
    ulToCharArray(ui32Address, &pcPayload[0]);
    ulToCharArray(ui32Size, &pcPayload[4]);

    //
    // Send command
    //
    if((retCode = sendCmd(SblDeviceCC2538::CMD_DOWNLOAD, pcPayload, 8)) != SBL_SUCCESS)
    {
        return retCode;        
    }

    //
    // Receive command response (ACK/NAK)
    //
    if((retCode = getCmdResponse(bSuccess)) != SBL_SUCCESS)
    {
        return retCode;
    }

    //
    // Return command response
    //
    return (bSuccess) ? SBL_SUCCESS : SBL_ERROR;
}


//-----------------------------------------------------------------------------
/** \brief This function sends the CC2538 SendData command and handles the
 *      device response. \e ui32ByteCount is limited by 
 *      SBL_CC2538_MAX_BYTES_PER_TRANSFER.
 *
 * \param[in] pcData
 *      Pointer to the data to send.
 * \param[in] ui32ByteCount
 *      The number of bytes to send.
 *
 * \return
 *      Returns SBL_SUCCESS if command and response was successful.
 */
//-----------------------------------------------------------------------------
uint32_t
SblDeviceCC2538::cmdSendData(const char *pcData, uint32_t ui32ByteCount)
{   
	DEBUG_PRINT("\n");
    uint32_t retCode = SBL_SUCCESS;
    bool bSuccess = false;

    //
    // Check input arguments
    //
    if(ui32ByteCount > SBL_CC2538_MAX_BYTES_PER_TRANSFER)
    {
        setState(SBL_ERROR, "Error: Byte count (%d) exceeds maximum transfer size %d.\n", ui32ByteCount, SBL_CC2538_MAX_BYTES_PER_TRANSFER);
        return SBL_ERROR;
    }

    //
    // Send command
    //
    if((retCode = sendCmd(SblDeviceCC2538::CMD_SEND_DATA, pcData, ui32ByteCount)) != SBL_SUCCESS)
    {
        return retCode;        
    }

    //
    // Receive command response (ACK/NAK)
    //
    if((retCode = getCmdResponse(bSuccess, 3)) != SBL_SUCCESS)
    {
        return retCode;
    }
    if(!bSuccess)
    {
        return SBL_ERROR;
    }

    return SBL_SUCCESS;
}


//-----------------------------------------------------------------------------
/** \brief This function returns the FLASH address of the bootloader enable
*      configuration.
*
* \return
*      Returns true if the address/range is within the device RAM.
*/
//-----------------------------------------------------------------------------
uint32_t SblDeviceCC2538::getBootloaderEnableAddress()
{
	DEBUG_PRINT("\n");
    return (SBL_CC2538_FLASH_START_ADDRESS + getFlashSize() - getPageEraseSize() + SBL_CC2538_BL_CONFIG_PAGE_OFFSET);
}

//-----------------------------------------------------------------------------
/** \brief This function sends the CC2538 SetXosc command and handles the 
 *      response from the device.
 *
 * \return
 *      Returns SBL_SUCCESS if command and response was successful.
 */
//-----------------------------------------------------------------------------
uint32_t 
SblDeviceCC2538::setXosc()
{
	DEBUG_PRINT("\n");
    int retCode = SBL_SUCCESS;
    bool bSuccess = false;

    //
    // Send command
    //
    if((retCode = sendCmd(SblDeviceCC2538::CMD_SET_XOSC)) != SBL_SUCCESS)
    {
        return retCode;        
    }

    //
    // Receive command response (ACK/NAK)
    //
    if((retCode = getCmdResponse(bSuccess, 5, true)) != SBL_SUCCESS)
    {
        return retCode;
    }
    if(!bSuccess)
    {
        return SBL_ERROR;
    }

    return SBL_SUCCESS;
}


//-----------------------------------------------------------------------------
/** \brief This function returns the address within which the specified
 *      \e ui32Address is  located.
 *
 * \return
 *      Returns the flash page within which an address is located.
 */
//-----------------------------------------------------------------------------
uint32_t
SblDeviceCC2538::addressToPage(uint32_t ui32Address)
{
	DEBUG_PRINT("\n");
    return ((ui32Address - SBL_CC2538_FLASH_START_ADDRESS) /                  \
            SBL_CC2538_PAGE_ERASE_SIZE);
}


//-----------------------------------------------------------------------------
/** \brief This function checks if the specified \e ui32StartAddress (or range)
 *      is located within the device RAM area.
 *
 * \return
 *      Returns true if the address/range is within the device RAM.
 */
//-----------------------------------------------------------------------------
bool 
SblDeviceCC2538::addressInRam(uint32_t ui32StartAddress, 
                              uint32_t ui32ByteCount/* = 1*/)
{
	DEBUG_PRINT("\n");
    uint32_t ui32EndAddr = ui32StartAddress + ui32ByteCount;

    if(ui32StartAddress < SBL_CC2538_RAM_START_ADDRESS)
    {
        return false;
    }
    if(ui32EndAddr > (SBL_CC2538_RAM_START_ADDRESS + getRamSize()))
    {
        return false;
    }
    return true;
}


//-----------------------------------------------------------------------------
/** \brief This function checks if the specified \e ui32StartAddress (or range)
 *      is located within the device FLASH area.
 *
 * \return
 *      Returns true if the address/range is within the device FLASH.
 */
//-----------------------------------------------------------------------------
bool
SblDeviceCC2538::addressInFlash(uint32_t ui32StartAddress, 
                                         uint32_t ui32ByteCount/* = 1*/)
{
	DEBUG_PRINT("\n");
    uint32_t ui32EndAddr = ui32StartAddress + ui32ByteCount;

    if(ui32StartAddress < SBL_CC2538_FLASH_START_ADDRESS)
    {
        return false;
    }
    if(ui32EndAddr > (SBL_CC2538_FLASH_START_ADDRESS + getFlashSize()))
    {
        return false;
    }
    
    return true;
}
