/******************************************************************************
*  Filename:       sbl_device_cc2652.cpp
*  Revised:        $Date$
*  Revision:       $Revision$
*
*  Description:    Serial Bootloader device file for CC13x2/CC26x2
*
*  Copyright (C) 2018 Texas Instruments Incorporated - http://www.ti.com/
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
#include "sbl_device_cc2652.h"

#include "serialib.h"


//-----------------------------------------------------------------------------
/** \brief Constructor
*/
//-----------------------------------------------------------------------------
SblDeviceCC2652::SblDeviceCC2652() : SblDeviceCC2650()
{
	DEBUG_PRINT("\n");
    m_pageEraseSize = SBL_CC2652_PAGE_ERASE_SIZE;
}

//-----------------------------------------------------------------------------
/** \brief Destructor
*/
//-----------------------------------------------------------------------------
SblDeviceCC2652::~SblDeviceCC2652()
{
	DEBUG_PRINT("\n");
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
SblDeviceCC2652::readFlashSize(uint32_t *pui32FlashSize)
{
	DEBUG_PRINT("\n");
    uint32_t retCode = SBL_SUCCESS;

    //
    // Read CC2652 DIECFG0 (contains FLASH size information)
    //
    uint32_t addr = SBL_CC2652_FLASH_SIZE_CFG;
    uint32_t value;
    if ((retCode = readMemory32(addr, 1, &value)) != SBL_SUCCESS)
    {
        setState((tSblStatus)retCode, "Failed to read device FLASH size: %s", getLastError().c_str());
        return retCode;
    }

    //
    // Calculate flash size (The number of flash sectors are at bits [7:0])
    //
    value &= 0xFF;
    *pui32FlashSize = value*SBL_CC2652_PAGE_ERASE_SIZE;

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
SblDeviceCC2652::readRamSize(uint32_t *pui32RamSize)
{
	DEBUG_PRINT("\n");
    int retCode = SBL_SUCCESS;

    uint32_t addr = SBL_CC2652_RAM_SIZE_CFG;
    uint32_t value;

    if ((retCode = readMemory32(addr, 1, &value)) != SBL_SUCCESS)
    {
        setState(retCode, "Failed to read device RAM size: %s", getLastError().c_str());
        return retCode;
    }

    uint32_t ramSizeInfo = (value & CHIP_SRAM_SIZE_INFO_M) >> CHIP_SRAM_SIZE_INFO_S;

    m_ramSize = calculateRamSize(ramSizeInfo);

    if (*pui32RamSize != NULL)
    {
        *pui32RamSize = m_ramSize;
    }

    return retCode;
}

//-----------------------------------------------------------------------------
/** \brief  Calculate RAM size.
*
*   \param[in] ramSizeInfo
*        Register value for RAM size configuration (TOP:PRCM:RAMHWOPT). 
*        The argument is optional and the default value is 3 (RAM size 80 KB)
*   \returns  uint32_t
*       Ram size
*/
//-----------------------------------------------------------------------------
uint32_t SblDeviceCC2652::calculateRamSize(uint32_t ramSizeInfo)
{
	DEBUG_PRINT("\n");
    uint32_t ramSize;

    switch (ramSizeInfo)
    {
    case 0:
        ramSize = (32 * 1024);
        break;
    case 1:
        ramSize = (48 * 1024);
        break;
    case 2:
        ramSize = (64 * 1024);
        break;
    case 3:
    default:
        ramSize = (80 * 1024);
        break;
    }

    return ramSize;

}

//-----------------------------------------------------------------------------
/** \brief This function returns the FLASH address of the bootloader enable
*      configuration.
*
* \return
*      Returns true if the address/range is within the device RAM.
*/
//-----------------------------------------------------------------------------
uint32_t SblDeviceCC2652::getBootloaderEnableAddress()
{
	DEBUG_PRINT("\n");
    return SBL_CC2652_FLASH_START_ADDRESS + getFlashSize() - getPageEraseSize() + SBL_CC2652_BL_CONFIG_PAGE_OFFSET;
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
SblDeviceCC2652::getCmdString(uint32_t ui32Cmd)
{
	DEBUG_PRINT("\n");
    std::string cmd = SblDeviceCC2650::getCmdString(ui32Cmd);
    if (cmd.find("Unknown") != std::string::npos)
    {
        switch (ui32Cmd)
        {
        case SblDeviceCC2652::CMD_DOWNLOAD_CRC:     cmd = "CMD_DOWNLOAD_CRC"; break;
        default: cmd = "Unknown command"; break;
        }
    }

    return cmd;
}


//-----------------------------------------------------------------------------
/** \brief This function sends the CC2652 download CRC command and handles the
*      device response.
*
* \param[in] ui32Address
*      The start address in CC2652 flash.
* \param[in] ui32Size
*      Number of bytes to be sent.
* \param[in] ui32Crc
*      Total number of bytes to be programmed. 
*
* \return
*      Returns SBL_SUCCESS if command and response was successful.
*/
//-----------------------------------------------------------------------------
uint32_t SblDeviceCC2652::cmdDownloadCrc(uint32_t ui32Address, uint32_t ui32Size, uint32_t ui32Crc)
{
	DEBUG_PRINT("\n");
    int retCode = SBL_SUCCESS;
    bool bSuccess = false;

    //
    // Check input arguments
    //
    if (!addressInFlash(ui32Address, ui32Size) &&
        !addressInRam(ui32Address, ui32Size))
    {
        setState(SBL_ARGUMENT_ERROR, "Specified address range (0x%08X + %d bytes) is not in device FLASH nor RAM.\n", ui32Address, ui32Size);
        return SBL_ARGUMENT_ERROR;
    }

    //
    // Generate payload
    // - 4B Program address
    // - 4B Program data
    // - 4B CRC
    //
    char pcPayload[12];
    ulToCharArray(ui32Address, &pcPayload[0]);
    ulToCharArray(ui32Size, &pcPayload[4]);
    ulToCharArray(ui32Crc, &pcPayload[8]);

    //
    // Send command
    //
    if ((retCode = sendCmd(SblDeviceCC2652::CMD_DOWNLOAD_CRC, pcPayload, 12) != SBL_SUCCESS))
    {
        return retCode;
    }

    //
    // Receive command response (ACK/NAK)
    //
    if ((retCode = getCmdResponse(bSuccess)) != SBL_SUCCESS)
    {
        return retCode;
    }

    //
    // Return command response
    //
    return (bSuccess) ? SBL_SUCCESS : SBL_ERROR;
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
SblDeviceCC2652::sendCmd(uint32_t ui32Cmd, const char *pcSendData/* = NULL*/,
    uint32_t ui32SendLen/* = 0*/)
{
	DEBUG_PRINT("\n");
    unsigned char pktLen = ui32SendLen + 3; // +3 => <1B Length>, <1B checksum>, <1B cmd>
    std::vector<char> pvPkt((pktLen));
    unsigned char pktSum = generateCheckSum(ui32Cmd, pcSendData, ui32SendLen);

    //
    // Build packet
    //
    pvPkt.at(0) = pktLen;
    pvPkt.at(1) = pktSum;
    pvPkt.at(2) = (unsigned char)ui32Cmd;
    if (ui32SendLen)
    {
        memcpy(&pvPkt[3], pcSendData, ui32SendLen);
    }

    //
    // Send packet
    //
    if (m_pCom->writeBytes(&pvPkt[0], pvPkt.size()) < 1)
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

