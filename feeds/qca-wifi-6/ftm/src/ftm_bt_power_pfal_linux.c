/*==========================================================================

                     FTM Platform specfic BT power File

Description
  Platform specific routines to toggle/read the BT power state

# Copyright (c) 2010-2011 by Qualcomm Technologies, Inc.  All Rights Reserved.
# Qualcomm Technologies Proprietary and Confidential.

===========================================================================*/

/*===========================================================================

                         Edit History


when       who     what, where, why
--------   ---     ----------------------------------------------------------
06/18/10   rakeshk  Created a source file to implement platform specific
                    routines for BT power.
07/07/10   rakeshk  Added routine to find the sysfs entry for bluetooth in
                    runtime
07/07/10   rakeshk  Added call to init the rfkill state path in case of first
                    read
===========================================================================*/


#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include "ftm_bt_power_pfal.h"
#include <string.h>

/* Bluetooth Rfkill Entry for Android */
static char *rfkill_state_path = NULL;
/*===========================================================================
FUNCTION   init_rfkill_path

DESCRIPTION
  Opens the sysfs entry for different types of rfkill and finds the one
  which matches Bluetooth by iterating through the rfkill entries
  and checking for bluetooth

DEPENDENCIES
  NIL

RETURN VALUE
  RETURN VALUE
  TRUE if SUCCESS, else FALSE

SIDE EFFECTS
  None

===========================================================================*/
boolean init_rfkill_path()
{
  int fd;
  int readsize;
  int rfkillid;
  char rfkill_path[64];
  char buf[16];

  for (rfkillid = 0; ; rfkillid++)
  {
    /* Open the different rfkill type entries and check if type macthes bluetooth */
    snprintf(rfkill_path, sizeof(rfkill_path), "/sys/class/rfkill/rfkill%d/type", rfkillid);
    fd = open(rfkill_path, O_RDONLY);
    if (fd < 0)
    {
      printf("open(%s) failed: \n", rfkill_path);
      return FALSE;
    }
    readsize = read(fd, &buf, sizeof(buf));
    close(fd);

    if (memcmp(buf, "bluetooth", 9) == 0)
    {
      break;
    }
  }

  asprintf(&rfkill_state_path, "/sys/class/rfkill/rfkill%d/state", rfkillid);
  return TRUE;
}

/*===========================================================================
FUNCTION   ftm_bt_power_pfal_set

DESCRIPTION
  Platform dependent interface API which sets the BT power state
  and returns the status of the toggle operation.

PLATFORM SPECIFIC DESCRIPTION
  Opens the rfkill entry for Bleutooth and initiates a write of the value
  passed as argument.

DEPENDENCIES
  NIL

RETURN VALUE
  RETURN VALUE
  STATUS_SUCCESS if SUCCESS, else other reasons

SIDE EFFECTS
  None

===========================================================================*/
request_status ftm_bt_power_pfal_set(bt_power_state state)
{
  int sz;
  int fd = -1;
  request_status ret = STATUS_FAIL;
  const char buffer = state;
  if(rfkill_state_path == NULL)
  {
    if(init_rfkill_path() != TRUE)
      goto out;
  }

  fd = open(rfkill_state_path, O_WRONLY);
  if (fd < 0)
  {
    ret = STATUS_NO_RESOURCES;
    goto out;
  }
  sz = write(fd, &buffer, 1);
  if (sz < 0)
  {
    goto out;
  }
  ret = STATUS_SUCCESS;

out:
  if (fd >= 0)
    close(fd);
  return ret;
}

/*===========================================================================
FUNCTION   ftm_bt_power_pfal_check

DESCRIPTION

  Platform dependent interface API which intiates a BT power read/check
  and returns the current state of the BT HW.

PLATFORM SPECIFIC DESCRIPTION
  Opens the rfkill entry for Bleutooth and initiates a read on the rfkill
  descriptor.

DEPENDENCIES
  NIL

RETURN VALUE
  RETURN VALUE
  Current BT power state

SIDE EFFECTS
  None

===========================================================================*/
bt_power_state ftm_bt_power_pfal_check()
{
  int sz;
  bt_power_state state= BT_OFF;
  int fd = -1;
  char buffer = '0';

  if(rfkill_state_path == NULL)
  {
    if(init_rfkill_path() != TRUE)
      goto out;
  }
  fd = open(rfkill_state_path, O_RDONLY);
  if (fd < 0)
  {
    goto out;
  }
  sz = read(fd, &buffer, 1);
  if (sz < 0)
  {
    goto out;
  }

out:
  if (fd >= 0)
    close(fd);
  state = (bt_power_state)buffer;
  return state;
}

