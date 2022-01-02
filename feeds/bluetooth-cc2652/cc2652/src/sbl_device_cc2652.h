#ifndef __SBL_DEVICE_CC2652_H__
#define __SBL_DEVICE_CC2652_H__
/******************************************************************************
*  Filename:       sbl_device_cc2652.h
*  Revised:        $Date$
*  Revision:       $Revision$
*
*  Description:    Serial Bootloader for CC13x2/CC26x2 header file.
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
#include "sbl_device_cc2650.h"

#define SBL_CC2652_PAGE_ERASE_SIZE          8192
#define SBL_CC2652_FLASH_SIZE_CFG           0x4003002C
#define SBL_CC2652_RAM_SIZE_CFG             0x40082250
#define SBL_CC2652_RAM_SIZE_CFG             0x40082250
#define SBL_CC2652_FLASH_START_ADDRESS      SBL_CC2650_FLASH_START_ADDRESS
#define SBL_CC2652_BL_CONFIG_PAGE_OFFSET    0x1FDB

#define CHIP_SRAM_SIZE_INFO                 0x40082250
#define CHIP_SRAM_SIZE_INFO_M               0x00000003
#define CHIP_SRAM_SIZE_INFO_S               0

class SblDeviceCC2652 : public SblDeviceCC2650 
{
public:
    SblDeviceCC2652();  // Constructor
    ~SblDeviceCC2652(); // Destructor

    enum {
        CMD_DOWNLOAD_CRC = 0x2F,
    };

protected:
    uint32_t readFlashSize(uint32_t *pui32FlashSize);
    uint32_t readRamSize(uint32_t *pui32RamSize);
    uint32_t calculateRamSize(uint32_t ramSizeInfo);
    uint32_t getBootloaderEnableAddress();

private:
    std::string getCmdString(uint32_t ui32Cmd);
    uint32_t sendCmd(uint32_t ui32Cmd, const char *pcSendData = NULL, uint32_t ui32SendLen = 0);
    uint32_t cmdDownloadCrc(uint32_t ui32Address, uint32_t ui32Size, uint32_t uiCrc);

};

#endif // __SBL_DEVICE_CC2652_H__
