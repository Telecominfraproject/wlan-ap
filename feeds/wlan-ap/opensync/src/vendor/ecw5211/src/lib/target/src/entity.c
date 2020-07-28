/*
Copyright (c) 2019, Plume Design Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
   3. Neither the name of the Plume Design Inc. nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL Plume Design Inc. BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <target.h>
#include <os_types.h>
#include <os_nif.h>
#include "log.h"

/* devinfo is /dev/mtd9 for ECW5211 */
static char devInfoFileName[] = "/dev/mtd9";
#define DEV_INFO_RECORD_SZ 40
static char devInfoModelNumber[DEV_INFO_RECORD_SZ];
static char devInfoSerialNumber[DEV_INFO_RECORD_SZ];
static bool devInfoModelNumber_saved = false;
static bool devInfoSerialNumber_saved = false;

char *get_devinfo_record( char * tag, char * payload, size_t payloadsz )
{
    FILE *devInfoFn = NULL;
    char buffer[80];
    char *tagPtr, *payloadPtr;
    int bytesRead = 0;
    bool record_found = false;

    if (tag == NULL)   return NULL;

    devInfoFn = fopen(devInfoFileName, "r");
    if (devInfoFn == NULL)   {
        LOGE("File open failed %s %s", devInfoFileName, tag );
        return NULL;
    }
    memset(buffer, 0, 80);
    payload[0] = 0;

    while ( bytesRead < 0x300 && !feof(devInfoFn)) {
       fgets( buffer, 80, devInfoFn);
       tagPtr = strstr(buffer, tag);
       if (tagPtr != NULL)   {
           strtok(tagPtr,"=");
           payloadPtr = strtok(NULL, " \n\r");
           LOGN ("devInfo %s %s", tag, payloadPtr);
           strncpy(payload, payloadPtr, payloadsz);
           record_found = true;
           break;
       } else {
           bytesRead += strlen(buffer);
       }
    }

    fclose(devInfoFn);
    if (record_found) {
        return payload;
    } else {
        return NULL;
    }
}

bool target_model_get(void *buff, size_t buffsz)
{
    if (!devInfoModelNumber_saved)  {
        if ( NULL == get_devinfo_record( "modelNumber=", devInfoModelNumber, DEV_INFO_RECORD_SZ))
	   snprintf(devInfoModelNumber, DEV_INFO_RECORD_SZ, "%s", "ECW5211");
        devInfoModelNumber_saved = true; 
    }
    strncpy(buff, devInfoModelNumber, buffsz);
    return true;
}

bool target_serial_get(void *buff, size_t buffsz)
{
    os_macaddr_t mac;
    char mac_buff[TARGET_BUFF_SZ];
    int n;

    if (!devInfoSerialNumber_saved)  {
        if ( NULL == get_devinfo_record( "serial_number=", devInfoSerialNumber, DEV_INFO_RECORD_SZ))
        {
            if (true == os_nif_macaddr("eth0", &mac))
            {
                memset(mac_buff, 0, sizeof(mac_buff));
                n = snprintf(mac_buff, sizeof(mac_buff), PRI(os_macaddr_plain_t), FMT(os_macaddr_t, mac));
                if (n == OS_MACSTR_PLAIN_SZ) {
                    LOG(ERR, "buffer not large enough");
                    return false;
                }
                strncpy(devInfoSerialNumber, mac_buff, buffsz);
            }
        }
        else
        {
            snprintf(devInfoSerialNumber, DEV_INFO_RECORD_SZ, "%s", "ECW5211-TIP-01");
        }

        devInfoSerialNumber_saved = true;
    }
    strncpy(buff, devInfoSerialNumber, buffsz);
    return true;
}

bool target_sw_version_get(void *buff, size_t buffsz)
{
    snprintf(buff, buffsz, "%s", "0.1.0");

    return true;
}

bool target_platform_version_get(void *buff, size_t buffsz)
{
    snprintf(buff, buffsz, "%s", "OPENWRT_ECW5211");

    return true;
}

