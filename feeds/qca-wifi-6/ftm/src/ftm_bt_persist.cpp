/*==========================================================================

                     BT persist NV items access source file

Description
  Read/Write APIs for retreiving NV items from persist memory.

# Copyright (c) 2011-12 by Qualcomm Technologies, Inc.  All Rights Reserved.
# Qualcomm Technologies Proprietary and Confidential.

===========================================================================*/

/*===========================================================================

                         Edit History


when       who     what, where, why
--------   ---     ----------------------------------------------------------
05/25/12   jav    Added FTM log that will display bt address while testing.
09/27/11   rrr     Moved persist related API for c/c++ compatibility, needed
                   for random BD address to be persistent across target
                   reboots.
==========================================================================*/

#include "ftm_bt_persist.h"
#include <semaphore.h>

#ifdef BT_NV_SUPPORT
#include "bt_nv.h"

/* Semaphore shared by the Event handler and main thread */
extern sem_t semaphore_cmd_complete;
/*Flag to manage the verbose output */
extern int verbose;

/*===========================================================================
FUNCTION   ftm_bt_send_nv_read_cmd

DESCRIPTION
 Helper Routine to process the nv read command

DEPENDENCIES
  NIL

RETURN VALUE
  RETURN VALUE
  FALSE = failure, else TRUE

SIDE EFFECTS
  None

===========================================================================*/
boolean ftm_bt_send_nv_read_cmd
(
  uint8 * cmd_buf,   /* pointer to Cmd */
  uint16 cmd_len     /* Cmd length */
)
{
  nv_persist_item_type my_nv_item;
  nv_persist_stat_enum_type cmd_result;
  boolean result = TRUE;

  if(cmd_len >1)
  {
    switch(*(cmd_buf+1))
    {
      case NV_BD_ADDR_I:
        cmd_result = (nv_persist_stat_enum_type)bt_nv_cmd(NV_READ_F,  NV_BD_ADDR_I, &my_nv_item);
        if (NV_SUCCESS != cmd_result)
        {
          if (verbose > 0)
          {
            fprintf (stderr, "nv_cmd_remote failed to get BD_ADDR from NV, code %d\n", cmd_result);
          }
          /* Send fail response */
          result = FALSE;
        }
        else
        {
          /* copy bytes */
          event_buf_nv_read_response[0] = FTM_BT_CMD_NV_READ;
          event_buf_nv_read_response[1] = NV_BD_ADDR_I;
          event_buf_nv_read_response[7] = my_nv_item.bd_addr[5];
          event_buf_nv_read_response[6] = my_nv_item.bd_addr[4];
          event_buf_nv_read_response[5] = my_nv_item.bd_addr[3];
          event_buf_nv_read_response[4] = my_nv_item.bd_addr[2];
          event_buf_nv_read_response[3] = my_nv_item.bd_addr[1];
          event_buf_nv_read_response[2] = my_nv_item.bd_addr[0];
          /* send BD_ADDR in the response */
          fprintf (stderr, "nv_cmd_remote got NV_BD_ADDR_I from NV: %x:%x:%x:%x:%x:%x\n",
                           (unsigned int) my_nv_item.bd_addr[5], (unsigned int) my_nv_item.bd_addr[4],
                           (unsigned int) my_nv_item.bd_addr[3], (unsigned int) my_nv_item.bd_addr[2],
                           (unsigned int) my_nv_item.bd_addr[1], (unsigned int) my_nv_item.bd_addr[0]);

          ftm_log_send_msg((const uint8 *)event_buf_nv_read_response,nv_read_response_size);
          result = TRUE;
        }
        break;

      case NV_BT_SOC_REFCLOCK_TYPE_I:
        cmd_result = (nv_persist_stat_enum_type)bt_nv_cmd(NV_READ_F,  NV_BT_SOC_REFCLOCK_TYPE_I, &my_nv_item);
        if (NV_SUCCESS != cmd_result)
        {
          if (verbose > 0)
          {
            fprintf (stderr, "nv_cmd_remote failed to get BD_ADDR from NV, code %d\n", cmd_result);
          }
          /* Send fail response */
          result = FALSE;
        }
        else
        {
          event_buf_nv_read_response[0] = FTM_BT_CMD_NV_READ;
          event_buf_nv_read_response[1] = NV_BT_SOC_REFCLOCK_TYPE_I;
          event_buf_nv_read_response[2] = (uint8) my_nv_item.bt_soc_refclock_type ;
          event_buf_nv_read_response[7] = 0x0;
          event_buf_nv_read_response[6] = 0x0;
          event_buf_nv_read_response[5] = 0x0;
          event_buf_nv_read_response[4] = 0x0;
          event_buf_nv_read_response[3] = 0x0;
          fprintf (stderr, "nv_cmd_remote got NV_BT_SOC_REFCLOCK_TYPE_I from NV: 0x%x\n",
                           (unsigned int) my_nv_item.bt_soc_refclock_type);
          ftm_log_send_msg((const uint8 *)event_buf_nv_read_response,nv_read_response_size);
          result = TRUE;
        }
        break;

      case NV_BT_SOC_CLK_SHARING_TYPE_I:
        cmd_result = (nv_persist_stat_enum_type)bt_nv_cmd(NV_READ_F,  NV_BT_SOC_CLK_SHARING_TYPE_I, &my_nv_item);
        if (NV_SUCCESS != cmd_result)
        {
          if (verbose > 0)
          {
            fprintf (stderr, "nv_cmd_remote failed to get CLK_SHARING from NV, code %d\n", cmd_result);
          }
          /* Send fail response */
          result = FALSE;
        }
        else
        {
          event_buf_nv_read_response[0] = FTM_BT_CMD_NV_READ;
          event_buf_nv_read_response[1] = NV_BT_SOC_CLK_SHARING_TYPE_I;
          event_buf_nv_read_response[2] = (uint8) my_nv_item.bt_soc_clk_sharing_type ;
          event_buf_nv_read_response[7] = 0x0;
          event_buf_nv_read_response[6] = 0x0;
          event_buf_nv_read_response[5] = 0x0;
          event_buf_nv_read_response[4] = 0x0;
          event_buf_nv_read_response[3] = 0x0;
          fprintf (stderr, "nv_cmd_remote got NV_BT_SOC_CLK_SHARING_TYPE_I from NV: 0x%x\n",
                        (unsigned int) my_nv_item.bt_soc_refclock_type);
          ftm_log_send_msg((const uint8 *)event_buf_nv_read_response,nv_read_response_size);
          result = TRUE;
        }
        break;
    }
    if(result == FALSE)
      ftm_log_send_msg(event_buf_nv_read_response_fail,nv_read_response_size_fail);

    sem_post(&semaphore_cmd_complete);
    return result;
  }
  return TRUE;
}
/*===========================================================================
FUNCTION   ftm_bt_send_nv_write_cmd

DESCRIPTION
 Helper Routine to process the nv write command

DEPENDENCIES
  NIL

RETURN VALUE
  RETURN VALUE
  FALSE = failure, else TRUE

SIDE EFFECTS
  None

===========================================================================*/
boolean ftm_bt_send_nv_write_cmd
(
  uint8 * cmd_buf,   /* pointer to Cmd */
  uint16 cmd_len     /* Cmd length */
)
{
  nv_persist_item_type my_nv_item;
  nv_persist_stat_enum_type cmd_result;
  boolean result = TRUE;
  if(cmd_len >1)
  {
    switch(*(cmd_buf+1))
    {
      case NV_BD_ADDR_I:
        memcpy(&my_nv_item.bd_addr, (cmd_buf+2), NV_BD_ADDR_SIZE);
        cmd_result = (nv_persist_stat_enum_type)bt_nv_cmd(NV_WRITE_F,  NV_BD_ADDR_I, &my_nv_item);
        if (NV_SUCCESS != cmd_result)
        {
          if (verbose > 0)
          {
            fprintf (stderr, "nv_cmd_remote failed to get BD_ADDR from NV, code %d\n", cmd_result);
          }
          /* Send fail response */
          result = FALSE;
        }
        else
        {
          result = TRUE;
        }
        break;

      case NV_BT_SOC_REFCLOCK_TYPE_I:
        switch (*(cmd_buf+2))
        {
          case NV_PS_BT_SOC_REFCLOCK_32MHZ:
          case NV_PS_BT_SOC_REFCLOCK_19P2MHZ:
            my_nv_item.bt_soc_refclock_type = (nv_ps_bt_soc_refclock_enum_type)(*(cmd_buf+2)) ;
            break;
          default:
          fprintf (stderr, "Invalid Ref Clock option\n");
          result = FALSE;
        }
        if (result != FALSE)
        {
          cmd_result= (nv_persist_stat_enum_type)bt_nv_cmd(NV_WRITE_F,  NV_BT_SOC_REFCLOCK_TYPE_I, &my_nv_item);
          if (NV_SUCCESS != cmd_result)
          {
            fprintf (stderr, "nv_cmd_remote failed to write SOC_REFCLOCK_TYPE to NV, code %d\n", cmd_result);
            result = FALSE;
          }
          else
          {
            result = TRUE;
          }
          break;
        }
      case NV_BT_SOC_CLK_SHARING_TYPE_I:
        switch (*(cmd_buf+2))
        {
          case NV_PS_BT_SOC_CLOCK_SHARING_ENABLED:
          case NV_PS_BT_SOC_CLOCK_SHARING_DISABLED:
            my_nv_item.bt_soc_clk_sharing_type = (nv_ps_bt_soc_clock_sharing_enum_type)(*(cmd_buf+2)) ;
            break;
          default:
          fprintf (stderr, "Invalid Clock Sharing option\n");
          result = FALSE;
        }
        if (result != FALSE)
        {
          cmd_result= (nv_persist_stat_enum_type)bt_nv_cmd(NV_WRITE_F, NV_BT_SOC_CLK_SHARING_TYPE_I, &my_nv_item);
          if (NV_SUCCESS != cmd_result)
          {
            fprintf (stderr, "nv_cmd_remote failed to write SOC_CLK_SHARING_TYPE to NV, code %d\n", cmd_result);
            result = FALSE;
          }
          else
          {
            result = TRUE;
          }
          break;
        }
    }
    if(result == FALSE)
    {
      ftm_log_send_msg(event_buf_bt_nv_write_fail,nv_write_response_size);
      sem_post(&semaphore_cmd_complete);
    }
    else
    {
      ftm_log_send_msg((const uint8 *)event_buf_bt_nv_write_pass,nv_write_response_size);
      sem_post(&semaphore_cmd_complete);
    }
    return result;
  }
  return TRUE;
}
#endif /* End of BT_NV_SUPPORT */
