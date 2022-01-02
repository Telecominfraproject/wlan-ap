/******************************************************************************
*  Filename:       sbl_device_cc2650.cpp
*  Revised:        $Date: 2013-08-21 14:33:34 +0200 (on, 21 aug 2013) $
*  Revision:       $Revision: 27319 $
*
*  Description:    Serial Bootloader device file for CC2650
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
#include "sbl_device_cc2650.h"

#include "serialib.h"

#include <vector>

#if 1
static uint32_t min(uint32_t a, uint32_t b)
{
	if(a > b)
	{
		return b;
	}
	return a;
}		
#endif

/// Struct used when splitting long transfers
typedef struct {
    uint32_t startAddr;
    uint32_t byteCount;
    uint32_t startOffset;
    bool     bExpectAck;
} tTransfer;

static uint32_t getDeviceRev(uint32_t deviceId)
{
    uint32_t tmp = deviceId >> 28;
    switch(tmp)
    {
        // Early samples (Rev 1)
    case 0:
    case 1:
        return 1;
    default: 
        return 2;
    }
}


//-----------------------------------------------------------------------------
/** \brief Constructor
 */
//-----------------------------------------------------------------------------
SblDeviceCC2650::SblDeviceCC2650()
{
    m_deviceRev = 0;
    m_pageEraseSize = SBL_CC2650_PAGE_ERASE_SIZE;

    if(!m_pCom)
    {
        m_pCom = new serialib();
    }
}

//-----------------------------------------------------------------------------
/** \brief Destructor
 */
//-----------------------------------------------------------------------------
SblDeviceCC2650::~SblDeviceCC2650()
{
}

//-----------------------------------------------------------------------------
/** \brief This function sends ping command to device.
 *
 * \return
 *      Returns SBL_SUCCESS, ...
 */
//-----------------------------------------------------------------------------
uint32_t
SblDeviceCC2650::ping()
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
    if((retCode = sendCmd(SblDeviceCC2650::CMD_PING)) != SBL_SUCCESS)
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
SblDeviceCC2650::readStatus(uint32_t *pui32Status)
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
    if((retCode = sendCmd(SblDeviceCC2650::CMD_GET_STATUS)) != SBL_SUCCESS)
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
SblDeviceCC2650::readDeviceId(uint32_t *pui32DeviceId)
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
    if((retCode = sendCmd(SblDeviceCC2650::CMD_GET_CHIP_ID)) != SBL_SUCCESS)
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

    //
    // Store device revision (used internally, see sbl_device_cc2650.h)
    //
    m_deviceRev = getDeviceRev(m_deviceId);

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
SblDeviceCC2650::readFlashSize(uint32_t *pui32FlashSize)
{
	DEBUG_PRINT("\n");
    uint32_t retCode = SBL_SUCCESS;

    //
    // Read CC2650 DIECFG0 (contains FLASH size information)
    //
    uint32_t addr = SBL_CC2650_FLASH_SIZE_CFG;
    uint32_t value;
    if((retCode = readMemory32(addr, 1, &value)) != SBL_SUCCESS)
    {
        setState((tSblStatus)retCode, "Failed to read device FLASH size: %s", getLastError().c_str());
        return retCode;
    }
    //
    // Calculate flash size (The number of flash sectors are at bits [7:0])
    //
    value &= 0xFF;
    *pui32FlashSize = value * getPageEraseSize();

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
SblDeviceCC2650::readRamSize(uint32_t *pui32RamSize)
{
	DEBUG_PRINT("\n");
    int retCode = SBL_SUCCESS;

    //
    // Read CC2650 DIECFG0 (contains RAM size information
    //
    uint32_t addr = SBL_CC2650_RAM_SIZE_CFG;
    uint32_t value;
    if((retCode = readMemory32(addr, 1, &value)) != SBL_SUCCESS)
    {
        setState(retCode, "Failed to read device RAM size: %s", getLastError().c_str());
        return retCode;
    }

    //
    // Calculate RAM size in bytes (Ram size bits are at bits [1:0])
    //
    value &= 0x03;
    if(m_deviceRev == 1)
    {
        // Early samples has less RAM
        switch(value)
        {
        case 3: *pui32RamSize = 0x4000; break;    // 16 KB
        case 2: *pui32RamSize = 0x2000; break;    // 8 KB
        case 1: *pui32RamSize = 0x1000; break;    // 4 KB
        case 0:                                   // 2 KB
        default:*pui32RamSize = 0x0800; break;    // All invalid values are interpreted as 2 KB
        }
    }
    else
    {
        switch(value)
        {
        case 3: *pui32RamSize = 0x5000; break;    // 20 KB
        case 2: *pui32RamSize = 0x4000; break;    // 16 KB
        case 1: *pui32RamSize = 0x2800; break;    // 10 KB
        case 0:                                   // 4 KB
        default:*pui32RamSize = 0x1000; break;    // All invalid values are interpreted as 4 KB
        }
    }
    
    // CC1310F64 and CC1310F32 officially has 16KB SRAM, but 20KB enabled. Return official value.
    if((sm_chipType == 0x1310 && m_deviceRev > 1) && 
        (m_flashSize == (64*1024) || (m_flashSize == (32*1024)))) {
            *pui32RamSize = 0x4000;
    }

    //
    // Save RAM size internally
    //
    m_ramSize = *pui32RamSize;

    return retCode;
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
SblDeviceCC2650::reset()
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
    if((retCode = sendCmd(SblDeviceCC2650::CMD_RESET)) != SBL_SUCCESS)
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
 *      that includes the address <startAddress + byteCount>. CC13/CC26xx erase 
 *      size is 4KB.
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
SblDeviceCC2650::eraseFlashRange(uint32_t ui32StartAddress, 
                                 uint32_t ui32ByteCount)
{
	DEBUG_PRINT("\n");
    uint32_t retCode = SBL_SUCCESS;
    bool bSuccess = false;
    char pcPayload[4];
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
    uint32_t ui32PageCount = ui32ByteCount / getPageEraseSize();
    if( ui32ByteCount % getPageEraseSize()) ui32PageCount ++;
    setProgress( 0 );
    for(uint32_t i = 0; i < ui32PageCount; i++)
    {

        //
        // Build payload
        // - 4B address (MSB first)
        //
        ulToCharArray(ui32StartAddress + i * getPageEraseSize(), &pcPayload[0]);

        //
        // Send command
        //
        if((retCode = sendCmd(SblDeviceCC2650::CMD_SECTOR_ERASE, pcPayload, 4)) != SBL_SUCCESS)
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
        // Check device status (Flash failed if page(s) locked)
        //
        readStatus(&devStatus);
        if(devStatus != SblDeviceCC2650::CMD_RET_SUCCESS)
        {
            setState(SBL_ERROR, "Flash erase failed. (Status 0x%02X = '%s'). Flash pages may be locked.\n", devStatus, getCmdStatusString(devStatus).c_str());
            return SBL_ERROR;
        }

        setProgress( 100*(i+1)/ui32PageCount );
    }
  

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
SblDeviceCC2650::readMemory32(uint32_t ui32StartAddress, uint32_t ui32UnitCount, 
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

    unsigned char pcPayload[6];
    uint32_t responseData[SBL_CC2650_MAX_MEMREAD_WORDS];
    uint32_t chunkCount = ui32UnitCount / SBL_CC2650_MAX_MEMREAD_WORDS;
    if(ui32UnitCount % SBL_CC2650_MAX_MEMREAD_WORDS) chunkCount++;
    uint32_t remainingCount = ui32UnitCount;

    for(uint32_t i = 0; i < chunkCount; i++)
    {
        uint32_t dataOffset = (i * SBL_CC2650_MAX_MEMREAD_WORDS);
        uint32_t chunkStart = ui32StartAddress + dataOffset;
#if 1
		uint32_t chunkSize  = min(remainingCount, SBL_CC2650_MAX_MEMREAD_WORDS);
#else
		uint32_t chunkSize;
		if(remainingCount > SBL_CC2650_MAX_MEMREAD_WORDS)
		{
			chunkSize = SBL_CC2650_MAX_MEMREAD_WORDS;
		} else {
			chunkSize = remainingCount;
		}
#endif
        remainingCount -= chunkSize;
    
        //
        // Build payload
        // - 4B address (MSB first)
        // - 1B access width
        // - 1B Number of accesses (in words)
        //
        ulToCharArray(chunkStart, (char *)&pcPayload[0]);
        pcPayload[4] = SBL_CC2650_ACCESS_WIDTH_32B;
        pcPayload[5] = chunkSize;
        //
        // Set progress
        //
        setProgress(((i * 100) / chunkCount));

        //
        // Send command
        //
        if((retCode = sendCmd(SblDeviceCC2650::CMD_MEMORY_READ, (char *)pcPayload, 6)) != SBL_SUCCESS)
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
        uint32_t expectedBytes = chunkSize * 4;
        uint32_t recvBytes = expectedBytes;
        if((retCode = getResponseData((char*)responseData, recvBytes)) != SBL_SUCCESS)
        {
            //
            // Respond with NAK
            //
            sendCmdResponse(false);
            return retCode;
        }
    
        if(recvBytes != expectedBytes)
        {
            //
            // Respond with NAK
            //
            sendCmdResponse(false);
            setState(SBL_ERROR, "Didn't receive 4 B.\n");
            return SBL_ERROR;
        }

        memcpy(&pui32Data[dataOffset], responseData, expectedBytes);
        //delete [] responseData;
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
SblDeviceCC2650::readMemory8(uint32_t ui32StartAddress, uint32_t ui32UnitCount, 
                              char *pcData)
{
	DEBUG_PRINT("\n");
    int retCode = SBL_SUCCESS;
    bool bSuccess = false;

    //
    // Check input arguments
    //
    if(ui32UnitCount == 0)
    {
        setState(SBL_ARGUMENT_ERROR, "readMemory8(): Read count is zero. Must be at least 1.\n");
        return SBL_ARGUMENT_ERROR;
    }

    if(!isConnected())
    {
        return SBL_PORT_ERROR;
    }

    unsigned char pcPayload[6];
    uint32_t chunkCount = ui32UnitCount / SBL_CC2650_MAX_MEMREAD_BYTES;
    if(ui32UnitCount % SBL_CC2650_MAX_MEMREAD_BYTES) chunkCount++;
    uint32_t remainingCount = ui32UnitCount;

    for(uint32_t i = 0; i < chunkCount; i++)
    {
        uint32_t dataOffset = (i * SBL_CC2650_MAX_MEMREAD_BYTES);
        uint32_t chunkStart = ui32StartAddress + dataOffset;
        uint32_t chunkSize  = min(remainingCount, SBL_CC2650_MAX_MEMREAD_BYTES);
        remainingCount -= chunkSize;
    
        //
        // Build payload
        // - 4B address (MSB first)
        // - 1B access width
        // - 1B number of accesses (in bytes)
        //
        ulToCharArray(chunkStart, (char *)&pcPayload[0]);
        pcPayload[4] = SBL_CC2650_ACCESS_WIDTH_8B;
        pcPayload[5] = chunkSize;

        //
        // Set progress
        //
        setProgress( ((i*100) / chunkCount));

        //
        // Send command
        //
        if((retCode = sendCmd(SblDeviceCC2650::CMD_MEMORY_READ, (char *)pcPayload, 6)) != SBL_SUCCESS)
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
        // Receive response
        //
        uint32_t expectedBytes = chunkSize;
        if((retCode = getResponseData(&pcData[dataOffset], chunkSize)) != SBL_SUCCESS)
        {
            //
            // Respond with NAK
            //
            sendCmdResponse(false);
            return retCode;            
        }

        if(chunkSize != expectedBytes)
        {
            //
            // Respond with NAK
            //
            sendCmdResponse(false);
            setState(SBL_ERROR, "readMemory8(): Received %d bytes (%d B expected) in iteration %d.\n", chunkSize, expectedBytes, i);
            return SBL_ERROR;
        }

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
/** \brief This function writes \e unitCount words of data to device SRAM.
 *      Max 61 32-bit words supported. Source array is 32 bit wide. 
 *        \e ui32StartAddress must be 4 byte aligned.
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
SblDeviceCC2650::writeMemory32(uint32_t ui32StartAddress, 
                               uint32_t ui32UnitCount, 
                               const uint32_t *pui32Data)
{
	DEBUG_PRINT("\n");
    uint32_t retCode = SBL_SUCCESS;
    bool bSuccess = false;
    
    //
    // Check input arguments
    //
    if((ui32StartAddress & 0x03))
    {
        setState(SBL_ARGUMENT_ERROR, "writeMemory32(): Start address (0x%08X) must 4 byte aligned.\n", ui32StartAddress);
        return SBL_ARGUMENT_ERROR;
    }
    if(addressInBLWorkMemory(ui32StartAddress, ui32UnitCount * 4))
    {
        // Issue warning
        setState(SBL_ARGUMENT_ERROR, "writeMemory32(): Writing to bootloader work memory/stack:\n(0x%08X-0x%08X, 0x%08X-0x%08X)\n",
            SBL_CC2650_BL_WORK_MEMORY_START,SBL_CC2650_BL_WORK_MEMORY_END, SBL_CC2650_BL_STACK_MEMORY_START,SBL_CC2650_BL_STACK_MEMORY_END);
        return SBL_ARGUMENT_ERROR;
    }

    if(!isConnected())
    {
        return SBL_PORT_ERROR;
    }

    uint32_t chunkCount = (ui32UnitCount / SBL_CC2650_MAX_MEMWRITE_WORDS);
    if(ui32UnitCount % SBL_CC2650_MAX_MEMWRITE_WORDS) chunkCount++;
    uint32_t remainingCount = ui32UnitCount;
    char* pcPayload = new char[5 + (SBL_CC2650_MAX_MEMWRITE_WORDS*4)];

    for(uint32_t i = 0; i < chunkCount; i++)
    {
        uint32_t chunkOffset = i * SBL_CC2650_MAX_MEMWRITE_WORDS;
        uint32_t chunkStart  = ui32StartAddress + (chunkOffset * 4);
        uint32_t chunkSize   = min(remainingCount, SBL_CC2650_MAX_MEMWRITE_WORDS);
        remainingCount -= chunkSize;

        //
        // Build payload
        // - 4B address (MSB first)
        // - 1B access width
        // - 1-SBL_CC2650_MAX_MEMWRITE_WORDS data (MSB first)
        //
        ulToCharArray(chunkStart, &pcPayload[0]);
        pcPayload[4] = SBL_CC2650_ACCESS_WIDTH_32B;
        for(uint32_t j = 0; j < chunkSize; j++)
        {
            ulToCharArray(pui32Data[j + chunkOffset], &pcPayload[5 + j*4]);
        }

        //
        // Set progress
        //
        setProgress( ((i * 100) / chunkCount) );

        //
        // Send command
        //
        if((retCode = sendCmd(SblDeviceCC2650::CMD_MEMORY_WRITE, pcPayload, 5 + chunkSize*4)) != SBL_SUCCESS)
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
            setState(SBL_ERROR, "writeMemory32(): Device NAKed command for address 0x%08X.\n", chunkStart);
            return SBL_ERROR;
        }
    }
    
    //
    // Set progress
    //
    setProgress(100);

    //
    // Cleanup
    //
    delete [] pcPayload;

    
    return SBL_SUCCESS;
}


//-----------------------------------------------------------------------------
/** \brief Write \e unitCount words of data to device SRAM. Source array is
 *      8 bit wide. Max 244 bytes of data. Source array is 32 bit wide.  
 *        Parameters \e startAddress and \e unitCount must be a a multiple of 4.
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
SblDeviceCC2650::writeMemory8(uint32_t ui32StartAddress, 
                              uint32_t ui32UnitCount, 
                              const char *pcData)
{
	DEBUG_PRINT("\n");
    uint32_t retCode = SBL_SUCCESS;
    bool bSuccess = false;
    
    //
    // Check input arguments
    //
    if(addressInBLWorkMemory(ui32StartAddress, ui32UnitCount))
    {
        // Issue warning
        setState(SBL_ARGUMENT_ERROR, "writeMemory8(): Writing to bootloader work memory/stack:\n(0x%08X-0x%08X, 0x%08X-0x%08X)\n",
            SBL_CC2650_BL_WORK_MEMORY_START,SBL_CC2650_BL_WORK_MEMORY_END, SBL_CC2650_BL_STACK_MEMORY_START,SBL_CC2650_BL_STACK_MEMORY_END);
        return SBL_ARGUMENT_ERROR;
    }

    if(!isConnected())
    {
        return SBL_PORT_ERROR;
    }

    uint32_t chunkCount = (ui32UnitCount / SBL_CC2650_MAX_MEMWRITE_BYTES);
    if(ui32UnitCount % SBL_CC2650_MAX_MEMWRITE_BYTES) chunkCount++;
    uint32_t remainingCount = ui32UnitCount;
    char* pcPayload = new char[5 + SBL_CC2650_MAX_MEMWRITE_BYTES];

    for(uint32_t i = 0; i < chunkCount; i++)
    {
        uint32_t chunkOffset = i * SBL_CC2650_MAX_MEMWRITE_BYTES;
        uint32_t chunkStart  = ui32StartAddress + chunkOffset;
        uint32_t chunkSize   = min(remainingCount, SBL_CC2650_MAX_MEMWRITE_BYTES);
        remainingCount -= chunkSize;

        //
        // Build payload
        // - 4B address (MSB first)
        // - 1B access width
        // - 1-SBL_CC2650_MAX_MEMWRITE_BYTES bytes data
        //
        ulToCharArray(chunkStart, &pcPayload[0]);
        pcPayload[4] = SBL_CC2650_ACCESS_WIDTH_8B;
        memcpy(&pcPayload[5], &pcData[chunkOffset], chunkSize);

        //
        // Set progress
        //
        setProgress( ((i * 100) / chunkCount) );

        //
        // Send command
        //
        if((retCode = sendCmd(SblDeviceCC2650::CMD_MEMORY_WRITE, pcPayload, 5 + chunkSize)) != SBL_SUCCESS)
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
            setState(SBL_ERROR, "writeMemory8(): Device NAKed command for address 0x%08X.\n", chunkStart);
            return SBL_ERROR;
        }
    }
    
    //
    // Set progress
    //
    setProgress(100);

    //
    // Cleanup
    //
    delete [] pcPayload;

    
    return SBL_SUCCESS;
}


//-----------------------------------------------------------------------------
/** \brief Calculate CRC over \e byteCount bytes, starting at address
 *      \e startAddress.
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
SblDeviceCC2650::calculateCrc32(uint32_t ui32StartAddress, 
                                uint32_t ui32ByteCount, uint32_t *pui32Crc)
{
	DEBUG_PRINT("\n");
    uint32_t retCode = SBL_SUCCESS;
    bool bSuccess = false;
    char pcPayload[12];
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
    pcPayload[8] = 0x00;
    pcPayload[9] = 0x00;
    pcPayload[10] = 0x00;
    pcPayload[11] = 0x00;
    //
    // Send command
    //
#if 0
	{
		int i;
		printf("@@send CMD_CRC32:");
		for(i=0;i<12;i++) printf(" %02x",(unsigned char)pcPayload[i]);
		printf("\n");
	}
#endif	
    if((retCode = sendCmd(SblDeviceCC2650::CMD_CRC32, pcPayload, 12)) != SBL_SUCCESS)
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
#if 0
	{
		int i;
		printf("@@recive CRC:");
		for(i=0;i<4;i++) printf(" %02x",(unsigned char)pcPayload[i]);
		printf("\n");
	}
#endif	
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
/** \brief Write \e unitCount words of data to device FLASH. Source array is
 *      8 bit wide. Parameters \e startAddress and \e unitCount must be a
 *      a multiple of 4. This function does not erase the flash before writing 
 *      data, this must be done using e.g. eraseFlashRange().
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
SblDeviceCC2650::writeFlashRange(uint32_t ui32StartAddress, 
                                 uint32_t ui32ByteCount, const char *pcData)
{
	DEBUG_PRINT("\n");
    uint32_t devStatus = SblDeviceCC2650::CMD_RET_UNKNOWN_CMD;
    uint32_t retCode = SBL_SUCCESS;
    uint32_t bytesLeft, dataIdx, bytesInTransfer;
    uint32_t transferNumber = 1;
    bool bIsRetry = false;
    bool bBlToBeDisabled = false;
    std::vector<tTransfer> pvTransfer;
    uint32_t ui32TotChunks = (ui32ByteCount / SBL_CC2650_MAX_BYTES_PER_TRANSFER);
    if(ui32ByteCount % SBL_CC2650_MAX_BYTES_PER_TRANSFER) ui32TotChunks++;
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
        if(((pcData[ui32BlCfgDataIdx]) & 0xFF) != SBL_CC2650_BL_CONFIG_ENABLED_BM)
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
        if(devStatus != SblDeviceCC2650::CMD_RET_SUCCESS)
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
#if 1
            bytesInTransfer = min(SBL_CC2650_MAX_BYTES_PER_TRANSFER, bytesLeft);
#else
			if(bytesLeft > SBL_CC2650_MAX_BYTES_PER_TRANSFER)
			{
				bytesInTransfer = SBL_CC2650_MAX_BYTES_PER_TRANSFER;
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
                if(devStatus != SblDeviceCC2650::CMD_RET_SUCCESS)
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
/** \brief Erases all customer accessible flash sectors not protected by FCFG1
 *
 * \return
 *      Returns SBL_SUCCESS, ...
 */
//-----------------------------------------------------------------------------
uint32_t
SblDeviceCC2650::eraseFlashBank()
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
    if((retCode = sendCmd(SblDeviceCC2650::CMD_BANK_ERASE)) != SBL_SUCCESS)
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
/** \brief Writes the CC26xx defined CCFG fields to the flash CCFG area with
 *      the values received in the data bytes of this command.
 *
 * \param[in] ui32Field
 *      CCFG Field ID which identifies the CCFG parameter to be written.
 * \param[in] ui32FieldValue
 *      Field value to be programmed.
 * \return
 *      Returns SBL_SUCCESS, ...
 */
//-----------------------------------------------------------------------------

uint32_t SblDeviceCC2650::setCCFG(uint32_t ui32Field, uint32_t ui32FieldValue){
	DEBUG_PRINT("\n");
    int retCode = SBL_SUCCESS;
    bool bSuccess = false;

    if(!isConnected())
    {
        return SBL_PORT_ERROR;
    }

    //
    // Generate payload
    // - 4B Field ID
    // - 4B Field value
    //
    char pcPayload[8];
    ulToCharArray(ui32Field, &pcPayload[0]);
    ulToCharArray(ui32FieldValue, &pcPayload[4]);

    //
    // Send command
    //
    if((retCode = sendCmd(SblDeviceCC2650::CMD_SET_CCFG, pcPayload, 8)) != SBL_SUCCESS)
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
        setState(SBL_ERROR, "Set CCFG command NAKed by device.\n");
        return SBL_ERROR;
    }

    
    return SBL_SUCCESS;
}

//-----------------------------------------------------------------------------
/** \brief Send command.
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
SblDeviceCC2650::sendCmd(uint32_t ui32Cmd, const char *pcSendData/* = NULL*/, 
                         uint32_t ui32SendLen/* = 0*/)
{
	DEBUG_PRINT("\n");
    //
    // Handle command ID for early versions
    //
    ui32Cmd = convertCmdForEarlySamples(ui32Cmd);


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
        setState(SBL_PORT_ERROR, "\nWriting to device failed (Command 0x%04x:'%s').\n", ui32Cmd,getCmdString(ui32Cmd).c_str());
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
/** \brief Initialize connection to the CC2650 device. 
 *
 * \param[in] bSetXosc
 *      If true, try to enable device XOSC.
 * \return
 *      Returns SBL_SUCCESS, ...
 */
//-----------------------------------------------------------------------------
uint32_t
SblDeviceCC2650::initCommunication(bool bSetXosc)
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


    m_bCommInitialized = true;
    return SBL_SUCCESS;
}


//-----------------------------------------------------------------------------
/** \brief This function returns a string with the device command name of
 *      \e ui32Cmd.
 *
 * \param[out] ui32Cmd
 *      The serial bootloader command.
 * \return
 *      Returns std::string with name of device command.
 */
//-----------------------------------------------------------------------------
std::string
SblDeviceCC2650::getCmdString(uint32_t ui32Cmd)
{
	DEBUG_PRINT("\n");
    switch(ui32Cmd)
    {
    case SblDeviceCC2650::CMD_PING:             return "CMD_PING"; break;
    case SblDeviceCC2650::CMD_CRC32:            return "CMD_CRC32"; break;
    case SblDeviceCC2650::CMD_DOWNLOAD:         return "CMD_DOWNLOAD"; break;
    case SblDeviceCC2650::CMD_GET_CHIP_ID:      return "CMD_GET_CHIP_ID"; break;
    case SblDeviceCC2650::CMD_GET_STATUS:       return "CMD_GET_STATUS"; break;
    case SblDeviceCC2650::CMD_MEMORY_READ:      return "CMD_MEMORY_READ"; break;
    case SblDeviceCC2650::CMD_MEMORY_WRITE:     return "CMD_MEMORY_WRITE"; break;
    case SblDeviceCC2650::CMD_RESET:            return "CMD_RESET"; break;
    default: return "Unknown command"; break;
    }
}


//-----------------------------------------------------------------------------
/** \brief This function returns a string with the device status name of
 *      \e ui32Status serial bootloader status value.
 *
 * \param[out] ui32Status
 *      The serial bootloader status value.
 * \return
 *      Returns std::string with name of device status.
 */
//-----------------------------------------------------------------------------
std::string
SblDeviceCC2650::getCmdStatusString(uint32_t ui32Status)
{
	DEBUG_PRINT("\n");
    switch(ui32Status)
    {
    case SblDeviceCC2650::CMD_RET_FLASH_FAIL:   return "FLASH_FAIL"; break;
    case SblDeviceCC2650::CMD_RET_INVALID_ADR:  return "INVALID_ADR"; break;
    case SblDeviceCC2650::CMD_RET_INVALID_CMD:  return "INVALID_CMD"; break;
    case SblDeviceCC2650::CMD_RET_SUCCESS:      return "SUCCESS"; break;
    case SblDeviceCC2650::CMD_RET_UNKNOWN_CMD:  return "UNKNOWN_CMD"; break;
    default: return "Unknown status"; break;
    }
}


//-----------------------------------------------------------------------------
/** \brief This function sends the CC2650 download command and handles the
 *      device response.
 *
 * \param[in] ui32Address
 *      The start address in CC2650 flash.
 * \param[in] ui32Size
 *      The total number of bytes to program on the device.
 *
 * \return
 *      Returns SBL_SUCCESS if command and response was successful.
 */
//-----------------------------------------------------------------------------
uint32_t 
SblDeviceCC2650::cmdDownload(uint32_t ui32Address, uint32_t ui32Size)
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
    if((retCode = sendCmd(SblDeviceCC2650::CMD_DOWNLOAD, pcPayload, 8)) != SBL_SUCCESS)
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
/** \brief This function sends the CC2650 SendData command and handles the
 *      device response.
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
SblDeviceCC2650::cmdSendData(const char *pcData, uint32_t ui32ByteCount)
{   
	DEBUG_PRINT("\n");
    uint32_t retCode = SBL_SUCCESS;
    bool bSuccess = false;

    //
    // Check input arguments
    //
    if(ui32ByteCount > SBL_CC2650_MAX_BYTES_PER_TRANSFER)
    {
        setState(SBL_ERROR, "Error: Byte count (%d) exceeds maximum transfer size %d.\n", ui32ByteCount, SBL_CC2650_MAX_BYTES_PER_TRANSFER);
        return SBL_ERROR;
    }

    //
    // Send command
    //
    if((retCode = sendCmd(SblDeviceCC2650::CMD_SEND_DATA, pcData, ui32ByteCount)) != SBL_SUCCESS)
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
/** \brief This function returns the page within which address \e ui32Address
 *      is located.
 *
 * \param[in] ui32Address
 *      The address.
 *
 * \return
 *      Returns the flash page within which an address is located.
 */
//-----------------------------------------------------------------------------
uint32_t
SblDeviceCC2650::addressToPage(uint32_t ui32Address)
{
	DEBUG_PRINT("\n");
    return ((ui32Address - SBL_CC2650_FLASH_START_ADDRESS) / getPageEraseSize() );
}


//-----------------------------------------------------------------------------
/** \brief This function checks if the specified \e ui32StartAddress (and range)
 *      is located within the device RAM area.
 *
 * \param[in] ui32StartAddress
 *      The start address of the range
 * \param[in] pui32Bytecount
 *      (Optional) The number of bytes in the range.
 * \return
 *      Returns true if the address/range is within the device RAM.
 */
//-----------------------------------------------------------------------------
bool 
SblDeviceCC2650::addressInRam(uint32_t ui32StartAddress, 
                              uint32_t ui32ByteCount/* = 1*/)
{
	DEBUG_PRINT("\n");
    uint32_t ui32EndAddr = ui32StartAddress + ui32ByteCount;

    if(ui32StartAddress < SBL_CC2650_RAM_START_ADDRESS)
    {
        return false;
    }
    if(ui32EndAddr > (SBL_CC2650_RAM_START_ADDRESS + getRamSize()))
    {
        return false;
    }
    return true;
}


//-----------------------------------------------------------------------------
/** \brief This function checks if the specified \e ui32StartAddress (and range)
 *      is located within the device FLASH area.
 *
 * \param[in] ui32StartAddress
 *      The start address of the range
 * \param[in] pui32Bytecount
 *      (Optional) The number of bytes in the range.
 *
 * \return
 *      Returns true if the address/range is within the device RAM.
 */
//-----------------------------------------------------------------------------
bool
SblDeviceCC2650::addressInFlash(uint32_t ui32StartAddress, 
                                         uint32_t ui32ByteCount/* = 1*/)
{
	DEBUG_PRINT("\n");
    uint32_t ui32EndAddr = ui32StartAddress + ui32ByteCount;

    if(ui32EndAddr > (SBL_CC2650_FLASH_START_ADDRESS + getFlashSize()))
    {
        return false;
    }
    
    return true;
}

//-----------------------------------------------------------------------------
/** \brief This function returns the FLASH address of the bootloader enable 
*      configuration.
*
* \return
*      Returns true if the address/range is within the device RAM.
*/
//-----------------------------------------------------------------------------
uint32_t 
SblDeviceCC2650::getBootloaderEnableAddress()
{
	DEBUG_PRINT("\n");
    return SBL_CC2650_FLASH_START_ADDRESS + getFlashSize() - getPageEraseSize() + SBL_CC2650_BL_CONFIG_PAGE_OFFSET;
}

//-----------------------------------------------------------------------------
/** \brief This function checks if the specified \e ui32StartAddress (and range)
 *      overlaps the bootloader's working memory or stack area. 
 *        
 *        The bootloader does not protect against writing to these ranges, but 
 *        doing so is almost guaranteed to crash the bootloader and requires a 
 *        reboot. SRAM ranges used by the bootloader:
 *        \li Work memory @ 0x20000000-0x2000016F
 *        \li Stack area  @ 0x20000FC0-0x20000FFF
 *
 * \param[in] ui32StartAddress
 *      The start address of the range
 * \param[in] pui32Bytecount
 *      (Optional) The number of bytes in the range.
 *
 * \return
 *      Returns true if the address/range is within the device RAM.
 */
//-----------------------------------------------------------------------------
bool 
SblDeviceCC2650::addressInBLWorkMemory(uint32_t ui32StartAddress, 
                                       uint32_t ui32ByteCount/* = 1*/)
{
	DEBUG_PRINT("\n");
    uint32_t ui32EndAddr = ui32StartAddress + ui32ByteCount;

    if(ui32StartAddress <= SBL_CC2650_BL_WORK_MEMORY_END)
    {
        return true;
    }
    if((ui32StartAddress >= SBL_CC2650_BL_STACK_MEMORY_START) && 
       (ui32StartAddress <= SBL_CC2650_BL_STACK_MEMORY_END))
    {
        return true;
    }
    if((ui32EndAddr >= SBL_CC2650_BL_STACK_MEMORY_START) && 
       (ui32EndAddr <= SBL_CC2650_BL_STACK_MEMORY_END))
    {
        return true;
    }
    return false;
}


//-----------------------------------------------------------------------------
/** \brief This function converts the command ID if the connected device
 *         is an early version. Affected commands are
 *         \li CMD_MEMORY_READ
 *         \li CMD_MEMORY_WRITE
 *         \li CMD_SET_CCFG
 *         \li CMD_BANK_ERASE
 *
 * \param[in] ui32Cmd
 *      The command that must be converted.
 *
 * \return
 *      Returns the correct command ID for the connected device.
 */
//-----------------------------------------------------------------------------
uint32_t SblDeviceCC2650::convertCmdForEarlySamples(uint32_t ui32Cmd)
{
	DEBUG_PRINT("\n");
    if(m_deviceRev != 1)
    {
        // No conversion needed, return early.
        return ui32Cmd;
    }
    switch(ui32Cmd)
    {
    case SblDeviceCC2650::CMD_MEMORY_READ: 
        return SblDeviceCC2650::REV1_CMD_MEMORY_READ;
    case SblDeviceCC2650::CMD_MEMORY_WRITE:
        return SblDeviceCC2650::REV1_CMD_MEMORY_WRITE;
    case SblDeviceCC2650::CMD_SET_CCFG:
        return SblDeviceCC2650::REV1_CMD_SET_CCFG;
    case SblDeviceCC2650::CMD_BANK_ERASE:
        return SblDeviceCC2650::REV1_CMD_BANK_ERASE;
    default: 
        // No conversion needed, return original command.
        return ui32Cmd;
    }
}
