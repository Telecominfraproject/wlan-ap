/*
 * Copyright (c) 2016 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * Copyright (c) 2012 by Qualcomm Atheros, Inc..
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

/*
* Description:
* Added wdsdaemon to enable testing of Host Controller Interface (HCI)
* communication with stack layers bypassed.
*  1. Acts as a communication bridge between PC to DUT over UART (/dev/ttyHSL0)
*     and also UART transport between DUT and BTSOC (/dev/ttyHS0).
*  2. Used to test exchange of BT-FM HCI commands, events and ACL data packets
*      between host and controller.
**/

#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <cutils/sockets.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdlib.h>
#include <sys/time.h>
#include <getopt.h>
#include <strings.h>
#include <termios.h>
#include <math.h>
#include <string.h>
#include <signal.h>
#include "wds_hci_pfal.h"
#include "hidl_client.h"

#ifdef ANDROID
#include <cutils/properties.h>
#endif

/*===========================================================================
FUNCTION   get_pkt_type

DESCRIPTION
  Routine to get the packet type from the data bytes received

DEPENDENCIES
  NIL

RETURN VALUE
  Packet type for the data bytes received

SIDE EFFECTS
  None

===========================================================================*/
static int get_packet_type(unsigned char id)
{
    int type;

    switch (id) {
    case BT_CMD_PKT_ID:
        type = PACKET_TYPE_BT_CMD;
        break;
    case FM_CMD_PKT_ID:
        type = PACKET_TYPE_FM_CMD;
        break;
    case BT_ACL_DATA_PKT_ID:
        type = PACKET_TYPE_BT_ACL;
        break;
    case ANT_CMD_PKT_ID:
        type = PACKET_TYPE_ANT_CMD;
        break;
    case ANT_DATA_PKT_ID:
        type = PACKET_TYPE_ANT_DATA;
        break;
    default:
        type = PACKET_TYPE_INVALID;
    }

    return type;
}

#ifdef ANDROID

int soc_type;

/** Get Bluetooth SoC type from system setting */
static int get_bt_soc_type()
{
    int ret = 0;
    char bt_soc_type[PROPERTY_VALUE_MAX];

    DEBUG("bt-hci: get_bt_soc_type\n");

    ret = property_get("qcom.bluetooth.soc", bt_soc_type, NULL);
    if (ret != 0) {
        DEBUG("qcom.bluetooth.soc set to %s\n", bt_soc_type);
        if (!strncasecmp(bt_soc_type, "rome", sizeof("rome"))) {
            return BT_SOC_ROME;
        }
        else if (!strncasecmp(bt_soc_type, "cherokee", sizeof("cherokee"))) {
            return BT_SOC_CHEROKEE;
        }
        else if (!strncasecmp(bt_soc_type, "ath3k", sizeof("ath3k"))) {
            return BT_SOC_AR3K;
        }
        else if (!strncasecmp(bt_soc_type, "napier", sizeof("napier"))) {
            return BT_SOC_NAPIER;
        }
        else {
            DEBUG("qcom.bluetooth.soc not set, so using default.\n");
            return BT_SOC_DEFAULT;
        }
    }
    else {
        DEBUG("%s: Failed to get soc type\n", __FUNCTION__);
        ret = BT_SOC_DEFAULT;
    }

    return ret;
}
#endif

static int parse_options(wdsdaemon *wds, int argc, char *argv[])
{
    int ret = STATUS_SUCCESS;
    int opt;

    if (argc > 2) {
        ERROR("Invalid number of arguments\n");
        ret = STATUS_INVALID_LENGTH;
        ERROR("Usage %s [-abfunht]", argv[0]);
        return ret;
    }

    if (argc == 1) {
        wds->mode = MODE_ALL_SMD;
        return ret;
    }

    while ((opt = getopt(argc, argv, "abfunhstm")) != -1) {
        switch (opt) {
        case 'a':
            DEBUG("Opening ANT SMD channels\n");
            wds->mode = MODE_ANT_SMD;
            break;
        case 'b':
            DEBUG("Opening BT SMD channels\n");
            wds->mode = MODE_BT_SMD;
            break;
        case 'f':
            DEBUG("Opening FM SMD channels\n");
            wds->mode = MODE_FM_SMD;
            break;
        case 't':
            ERROR("Setting mask for pc initialization\n");
            wds->pcinit_mask = true;
            break;
        case 's':
            ERROR("Opening WDS server socket\n");
            wds->is_server_enabled = true;
            wds->pcinit_mask = true;
            break;
#ifdef ANDROID
      if (soc_type == BT_SOC_ROME || soc_type == BT_SOC_CHEROKEE) {
          case 'u':
            DEBUG("Opening UART BT Channel\n");
            wds->mode = MODE_BT_UART;
            break;
      }
      if (soc_type == BT_SOC_CHEROKEE) {
          case 'm':
              DEBUG("Opening UART FM Channel\n");
              wds->mode = MODE_FM_UART;
              break;
      }
#else
#ifdef BT_SOC_TYPE_ROME
      case 'u':
        DEBUG("Opening UART BT Channel\n");
        wds->mode = MODE_BT_UART;
        break;
#endif
#endif
#ifdef CONFIG_ANT
      case 'n':
            ERROR("Opening ANT UART channels\n");
            wds->mode = MODE_ANT_UART;
            break;
#endif
      case 'h':
        DEBUG("By Default, it will open all SMD channels\n");
        DEBUG("Use -a for opening only ANT Channels\n");
        DEBUG("Use -b for opening only BT Channels\n");
        DEBUG("Use -f for opening only FM Channels\n");

#ifdef ANDROID
      if (soc_type == BT_SOC_ROME || soc_type == BT_SOC_CHEROKEE) {
          DEBUG("Use -u for opening only UART Channel for BT (ROME)\n");
      }
      if (soc_type == BT_SOC_CHEROKEE) {
          DEBUG("Use -m for opening only UART Channel for FM\n");
      }
#else
#ifdef BT_SOC_TYPE_ROME
      DEBUG("Use -u for opening only UART Channel for BT (ROME)\n");
#endif
#endif

#ifdef CONFIG_ANT
            DEBUG("Use -n for opening ANT UART channels only\n");
#endif
            DEBUG("Use -t for masking pc initialization\n");
            DEBUG("Use -s for setting communication via server socket\n");
            DEBUG("Use -h to print help\n");
            ret = STATUS_ERROR;
            break;
        default:
            DEBUG("Usage %s [-abfunhmst]\n", argv[0]);
            ret = STATUS_ERROR;
            break;
        }
    }

    return ret;
}

static void wdsdaemon_init(wdsdaemon *wds)
{
    /* PC-DUT interface */
#ifdef BT_BLUEZ
    wds->pc_if.uart.intf = (unsigned char *)BT_HSLITE_UART_DEVICE;
#else
    wds->pc_if.uart.intf = (unsigned char *)BT_HS_NMEA_DEVICE;
#endif

    /* DUT-BTSOC interface */
    switch (wds->mode) {
    case MODE_ALL_SMD:
        wds->soc_if.smd.fm_cmd = (unsigned char *)APPS_RIVA_FM_CMD_CH;
        wds->soc_if.smd.bt_acl = (unsigned char*)APPS_RIVA_BT_ACL_CH;
        wds->soc_if.smd.bt_cmd = (unsigned char *)APPS_RIVA_BT_CMD_CH;
        wds->soc_if.smd.ant_cmd = (unsigned char *)APPS_RIVA_ANT_CMD;
        wds->soc_if.smd.ant_data = (unsigned char *)APPS_RIVA_ANT_DATA;
        break;
    case MODE_ANT_SMD:
        wds->soc_if.smd.ant_cmd = (unsigned char *)APPS_RIVA_ANT_CMD;
        wds->soc_if.smd.ant_data = (unsigned char *)APPS_RIVA_ANT_DATA;
        break;
    case MODE_BT_SMD:
        wds->soc_if.smd.bt_acl = (unsigned char *)APPS_RIVA_BT_ACL_CH;
        wds->soc_if.smd.bt_cmd = (unsigned char *)APPS_RIVA_BT_CMD_CH;
        break;
    case MODE_FM_SMD:
        wds->soc_if.smd.fm_cmd = (unsigned char *)APPS_RIVA_FM_CMD_CH;
        break;
    case MODE_BT_UART:
    case MODE_ANT_UART:
        wds->soc_if.uart.intf = (unsigned char *)BT_HS_UART_DEVICE;
        break;
    }
}

int process_packet_type(wdsdaemon *wds, unsigned char pkt_id,
                            int *dst_fd, int *len, int dir)
{
    int state;

    switch(pkt_id) {
        case BT_CMD_PKT_ID:
            *len = BT_EVT_PKT_HDR_LEN_UART;
        case BT_EVT_PKT_ID:
        case BT_ACL_DATA_PKT_ID:
            state = RX_BT_HDR;
            if (wds->mode == MODE_BT_UART)
                *dst_fd = wds->soc_if.uart.uart_fd;
            else
                if (pkt_id == BT_CMD_PKT_ID)
                    *dst_fd = wds->soc_if.smd.bt_cmd_fd;
                else
                    *dst_fd = wds->soc_if.smd.bt_acl_fd;
            if (pkt_id == BT_ACL_DATA_PKT_ID)
                *len = BT_ACL_PKT_HDR_LEN;
            else if (pkt_id == BT_EVT_PKT_ID)
                *len = BT_EVT_PKT_HDR_LEN;
            break;
        case FM_CMD_PKT_ID:
            if (wds-> mode == MODE_FM_UART)
                *dst_fd = wds->soc_if.uart.uart_fd;
            else
                *dst_fd = wds->soc_if.smd.fm_cmd_fd;
        case FM_EVT_PKT_ID:
            state = RX_FM_HDR;
            if (pkt_id == FM_CMD_PKT_ID)
                *len = FM_CMD_PKT_HDR_LEN;
            else if (pkt_id == FM_EVT_PKT_ID)
                *len = FM_EVT_PKT_HDR_LEN;
            break;
        case ANT_CMD_PKT_ID:
        case ANT_DATA_PKT_ID:
            state = RX_ANT_HDR;
            if (wds->mode == MODE_ANT_UART)
                *dst_fd = wds->soc_if.uart.uart_fd;
            else
                if (pkt_id == ANT_CMD_PKT_ID)
                    *dst_fd = wds->soc_if.smd.ant_cmd_fd;
                else
                    *dst_fd = wds->soc_if.smd.ant_data_fd;
            break;
        default:
            state = RX_ERROR;
            break;
        }

    if (dir == SOC_TO_PC) {
        if (wds->is_server_enabled)
            *dst_fd = wds->server_socket_fd;
        else
            *dst_fd = wds->pc_if.uart.uart_fd;
    }

    return state;
}

static int process_pc_data_to_soc(wdsdaemon *wds, unsigned char *buf, int src_fd)
{
    int retval = STATUS_SUCCESS;
    int len = 1, n_bytes = 0, n_total = 0;
    int pkt_id = 0, dst_fd = 0;
    int state = RX_PKT_IND, i;

    do {
        if ((n_bytes = read(src_fd, (unsigned char *)&buf[n_total], len)) > 0) {
            n_total += n_bytes;
            len -= n_bytes;
            if (len)
                continue;

            switch(state) {
                case RX_PKT_IND:
                    pkt_id = buf[0];
                    state = process_packet_type(wds, pkt_id, &dst_fd, &len,
                            PC_TO_SOC);
                    break;
                case RX_BT_HDR:
                    len = get_pkt_data_len(pkt_id, buf);
                    state = RX_BT_DATA;
                    break;
                case RX_ANT_HDR:
                    len = buf[0];
                    state = RX_ANT_DATA;
                    break;
                case RX_FM_HDR:
                    len = get_pkt_data_len(pkt_id, buf);
                    state = RX_FM_DATA;
                    break;
                case RX_BT_DATA:
                case RX_ANT_DATA:
                case RX_FM_DATA:
                    len  = 0;
                    break;
                default:
                    retval = STATUS_ERROR;
                    break;
            }
        } else {
            ERROR("%s: error while reading from fd = %d err = %s\n",
                    __func__, src_fd, strerror(errno));
            if (n_bytes < 0)
                ERROR("%s:read returns err: %d\n", __func__,n_bytes);
            if (n_bytes == 0)
                ERROR("%s: This indicates the close of other end\n", __func__);
            retval = STATUS_ERROR;
            break;
        }
    } while (len);

    if(retval)
        goto fail;

    /* In case of Pronto, for BT, we have different channels for CMD and ACL,
     * so we don't send packet indicator to SoC.
     * Below condition will skip the packet indicator byte to Soc in\
     * case of Pronto.
     */
    if (wds->mode != MODE_BT_UART && wds->mode != MODE_ANT_UART &&
            wds->mode != MODE_FM_UART) {
        n_total -= 1;
        len = 1;
    }
    while(n_total) {
        if((n_bytes = write(dst_fd, buf + len, n_total)) > 0) {
            len += n_bytes;
            n_total -= n_bytes;
        } else
            ERROR("%s :Error while writeto fd = %d err = %s\n",
        __func__, dst_fd, strerror(errno));
            break;
    }

    DEBUG("cmd:\t");
    for (i = 0; i < len; i++)
        DEBUG("0x%x\t", buf[i]);
    DEBUG("\n");

    if (n_total)
        retval = STATUS_ERROR;

fail:
    return retval;
}

static void thread_exit_handler(int signo){
    DEBUG("%s: %d",__func__,signo);
}

int server_create(wdsdaemon *wds,int *src_fd) {
    int retval = establish_server_socket(wds);
    if (STATUS_SUCCESS == retval)
        *src_fd = wds->server_socket_fd;
    else
        ERROR("Failed to init server socket\n");

    return retval;
}

int main(int argc, char *argv[])
{
    int retval = STATUS_ERROR, src_fd = 0;
    fd_set readfds;
    wdsdaemon wds;
    unsigned char *buf = NULL;
    size_t size = UART_BUF_SIZE;
    struct sigaction action;
    sigset_t sigmask, emptymask;

    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGINT);
    sigaddset(&sigmask, SIGPIPE);
    if (sigprocmask(SIG_BLOCK, &sigmask, NULL) == -1) {
        ERROR("failed to sigprocmask");
    }
    memset(&action, 0, sizeof(struct sigaction));
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    action.sa_handler = thread_exit_handler;

    sigemptyset(&emptymask);

    if (sigaction(SIGINT, &action, NULL) < 0) {
        ERROR("%s:sigaction failed\n", __func__);
    }

    memset(&wds, 0, sizeof(wdsdaemon));

#ifdef ANDROID
    soc_type = get_bt_soc_type();
#endif

    /* parse options */
    retval = parse_options(&wds, argc, argv);
    if (STATUS_SUCCESS != retval) {
        goto fail;
    }

    wdsdaemon_init(&wds);

    if(!(wds.pcinit_mask))
    {
        retval = init_pc_interface(&wds);
        if (STATUS_SUCCESS != retval) {
            ERROR("Failed to init DUT-PC interface\n");
            goto fail;
        }
        src_fd = wds.pc_if.uart.uart_fd;
    }

    retval = init_soc_interface(&wds);
    if (STATUS_SUCCESS != retval) {
        ERROR("Failed to init DUT-BTSOC interface\n");
        goto fail;
    }
#ifdef BT_BLUEZ
    fflush(stdout);
    fflush(stderr);
#endif

    buf = (unsigned char *)calloc(size, 1);
    if (!buf) {
        ERROR("%s:Unable to allocate memory\n", __func__);
        goto fail;
    }

    if( wds.is_server_enabled && ( server_create(&wds, &src_fd)!= STATUS_SUCCESS ))
        goto fail;

    do {
        FD_ZERO(&readfds);
        FD_SET(src_fd, &readfds);

        DEBUG("Waiting for data:\n");
        if ((retval = select(src_fd + 1, &readfds, NULL, NULL, NULL)) == -1) {
            ERROR("%s:select failed\n", __func__);
            if (wds.is_server_enabled)
            {
                ERROR("%s:closing the server socket and reopening\n", __func__);
                close(src_fd);
                if(server_create(&wds, &src_fd)== STATUS_SUCCESS)
                    continue;
            }
            break;
        }

        if (FD_ISSET(src_fd, &readfds)) {
            retval = process_pc_data_to_soc(&wds, buf, src_fd);
        } else
            ERROR("%s:src_fd port not set\n",__func__);
        if (retval != STATUS_SUCCESS) {
            ERROR("%s: Error while processing Data to SoC err = %d\n", __func__, retval);
            if (wds.is_server_enabled)
            {
                ERROR("%s:closing the server socket and reopening\n", __func__);
                close(src_fd);
                if(server_create(&wds, &src_fd)== STATUS_SUCCESS)
                    continue;
            }
            break;
        }
    }while(1);

fail:
    if (buf)
        free(buf);
    shutdown(src_fd, SHUT_RDWR);
    switch (wds.mode) {
        case MODE_BT_UART:
        case MODE_FM_UART:
        case MODE_ANT_UART:
            shutdown(wds.soc_if.uart.uart_fd, SHUT_RDWR);
            break;
        case MODE_ALL_SMD:
        case MODE_BT_SMD:
            shutdown(wds.soc_if.smd.bt_cmd_fd, SHUT_RDWR);
            shutdown(wds.soc_if.smd.bt_acl_fd, SHUT_RDWR);
            if(wds.mode == MODE_BT_SMD)
                break;
        case MODE_FM_SMD:
            shutdown(wds.soc_if.smd.fm_cmd_fd, SHUT_RDWR);
            if (wds.mode == MODE_FM_SMD)
                break;
        case MODE_ANT_SMD:
            shutdown(wds.soc_if.smd.ant_cmd_fd, SHUT_RDWR);
            shutdown(wds.soc_if.smd.ant_data_fd, SHUT_RDWR);
            break;
    }
    pthread_join(wds.soc_rthread, NULL);

    hidl_client_close();
    return retval;
}
