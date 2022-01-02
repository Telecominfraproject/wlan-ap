#ifndef __SBL_EB_INFO_H__
#define __SBL_EB_INFO_H__
/******************************************************************************
*  Filename:       sbl_eb_info.h
*  Revised:        $Date: 2013-07-09 15:06:47 +0200 (Tue, 09 Jul 2013) $
*  Revision:       $Revision: 26800 $
*
*  Description:    Serial Bootloader EB info class header file.
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
#include <string.h>

class SblEbInfo 
{
public:
    enum 
    {
        DEVICE_NAME_SIZE = 15,
        EB_PORT_SIZE = 16,
        DESCRIPTION_SIZE = 256
    };

    char ebPort[EB_PORT_SIZE];              // E.g. COM42
    char ebDescription[DESCRIPTION_SIZE];
    char devName[DEVICE_NAME_SIZE];         // E.g. CC2538 for CC2650

    SblEbInfo& operator = (const SblEbInfo& other) {
        strncpy(ebPort, other.ebPort, EB_PORT_SIZE);
        strncpy(ebDescription, other.ebDescription, DESCRIPTION_SIZE);
        strncpy(devName, other.devName, DEVICE_NAME_SIZE);
        return *this;
    }

};

#endif // __SBL_EB_INFO_H__ 