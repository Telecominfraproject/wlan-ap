#ifndef __SBLLIB_H__
#define __SBLLIB_H__
/******************************************************************************
*  Filename:       sbllib.h
*  Revised:        $Date$
*  Revision:       $Revision$
*
*  Description:    Serial Bootloader Library header file.
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
#include <string>
#include <stdint.h>

#include "debug.h"
#define SBL_MAX_DEVICES             20
#define SBL_DEFAULT_RETRY_COUNT     1
#define SBL_DEFAULT_READ_TIMEOUT    100 // in ms
#define SBL_DEFAULT_WRITE_TIMEOUT   200 // in ms

typedef enum {
    SBL_SUCCESS = 0,
    SBL_ERROR,
    SBL_ARGUMENT_ERROR,
    SBL_TIMEOUT_ERROR,
    SBL_PORT_ERROR,
    SBL_ENUM_ERROR,
    SBL_UNSUPPORTED_FUNCTION,
} tSblStatus;

#include "serialib.h" 
#include "sbl_device.h"
#include "sbl_device_cc2538.h"
#include "sbl_device_cc2650.h"
#include "sbl_device_cc2652.h"
#include "sbl_eb_info.h"



#endif // __SBLLIB_H__
