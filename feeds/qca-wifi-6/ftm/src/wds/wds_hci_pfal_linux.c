/*
 * Copyright (c) 2016 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * Copyright (c) 2012 by Qualcomm Atheros, Inc..
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <cutils/sockets.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <sys/select.h>
#include <termios.h>
#include <pthread.h>
#include <stdio.h>
#include "wds_hci_pfal.h"
#include <math.h>
#include <dlfcn.h>
#include <unistd.h>
#include <string.h>
#include "hidl_client.h"

#ifdef ANDROID
#include "bt_vendor_lib.h"
#else
#ifdef BT_SOC_TYPE_ROME
#include "bt_vendor_lib.h"
#endif
#endif

#define SOCKET_NAME "wdssock"

typedef unsigned char uint8;
extern int process_packet_type(wdsdaemon *wds, unsigned char pkt_id,
        int *dst_fd, int *len, int dir);

#ifdef ANDROID
extern int soc_type;
#endif

static int find_max(int *arr, int len)
{
    int max = arr[0];
    int i;

    for (i = 1; i < len; i++) {
        if (arr[i] > max)
            max = arr[i];
    }

    return max;
}

unsigned short get_pkt_data_len(unsigned char type,
                            unsigned char *buf)
{
    unsigned short len = 0;
    switch (type) {
    case BT_EVT_PKT_ID:
        /* Event packet: 1 byte length */
        len = buf[BT_EVT_PKT_HDR_LEN_UART - 1];
        break;
    case BT_ACL_DATA_PKT_ID:
        /* ACL packet: 2 byte length */
        len =
          (((unsigned short) buf[BT_ACL_PKT_HDR_LEN_UART - 1] << 8) &
        0xFF00) | (((unsigned short) buf[BT_ACL_PKT_HDR_LEN_UART - 2])
        & 0x00FF);
        break;
    case BT_CMD_PKT_ID:
        len = buf[BT_EVT_PKT_HDR_LEN_UART];
        break;
    case FM_CMD_PKT_ID:
        /* FM Cmd packet param len: 1 byte length */
        len = buf[FM_CMD_PKT_HDR_LEN];
        break;
    case FM_EVT_PKT_ID:
        /* FM Evt packet param len: 1 byte length */
        len = buf[FM_EVT_PKT_HDR_LEN];
        break;
    }
    return len;
}

static int process_soc_data_to_pc(wdsdaemon *wds, unsigned char *buf_in,
        int src_fd)
{
    int retval = STATUS_SUCCESS;
    ssize_t n_bytes = 0, n_total = 0;
    int len = 1, dst_fd = 0, i;
    int state = RX_PKT_IND, offset = 0;
    unsigned char pkt_ind_to_read = 1;


    /* In case of Pronto, we have different channels for CMD and ACL,
     * so we don't get packet indicator from SoC.
     * Below condition will skip reading packet indicator byte in
     * case of Pronto.
     */
    if (wds->mode != MODE_BT_UART && wds->mode != MODE_ANT_UART &&
            wds->mode != MODE_FM_UART) {
        pkt_ind_to_read = 0;
        offset++;
    }

    do {
        while (len) {
            if (pkt_ind_to_read == 0)
                goto dont_read_pkt_ind;

            if ((n_bytes = read(src_fd,
                    (unsigned char *)(&buf_in[offset + n_total]),
                            len)) > 0) {
                n_total += n_bytes;
                len -= n_bytes;
                if (len)
                    continue;
dont_read_pkt_ind:
                switch(state) {
                case RX_PKT_IND:
                    pkt_ind_to_read = 1;
                    state = process_packet_type(wds, buf_in[0], &dst_fd, &len,
                            SOC_TO_PC);
                    break;
                case RX_BT_HDR:
                    len = get_pkt_data_len(buf_in[0], buf_in);
                    state = RX_BT_DATA;
                    break;
                case RX_BT_DATA:
                    len = 0;
                    break;
                case RX_ANT_HDR:
                    pkt_ind_to_read = 1;
                    len = buf_in[n_total];
                    state = RX_ANT_DATA;
                    break;
                case RX_ANT_DATA:
                    if (buf_in[2] ==
                        ANT_DATA_TYPE_BROADCAST ||
                    buf_in[2] ==
                        ANT_DATA_TYPE_ACKNOWLEDGED ||
                    buf_in[2] == ANT_DATA_TYPE_BURST ||
                    buf_in[2] == ANT_DATA_TYPE_ADV_BURST)
                        buf_in[0] = ANT_DATA_PKT_ID;
                    else
                        buf_in[0] = ANT_EVT_PKT_ID;

                    retval = STATUS_SUCCESS;
                    break;
                case RX_FM_HDR:
                    len = get_pkt_data_len(buf_in[0], buf_in);
                    state = RX_FM_DATA;
                    break;
                case RX_FM_DATA:
                    len = 0;
                    break;
                default:
                    retval = STATUS_ERROR;
                    break;
                }
            } else {
                ERROR("%s Failed To read from SoC fd = %d\n",__func__, src_fd);
                break;
            }
        }
        if (retval)
            break;
        n_total += offset;
        len = 0;

        DEBUG("evt:\t");
        for (i = 0; i < n_total; i++)
            DEBUG("0x%x\t", buf_in[i]);
        DEBUG("\n");
        while (n_total) {
            if((n_bytes = write(dst_fd, buf_in + len, n_total)) >= 0) {
                len += n_bytes;
                n_total -= n_bytes;
            }
            else {
                if (wds->is_server_enabled) {
                    retval = STATUS_CLIENT_ERROR;
                    ERROR("%s: unable to write to client socket, fd = %d err = %s\n", __func__, dst_fd, strerror(errno));
                }
                else {
                    retval = STATUS_ERROR;
                    ERROR("%s: unable to write to pc_if fd = %d err = %s\n", __func__, dst_fd, strerror(errno));
                }
                break;
            }
        }
    } while (0);

    return retval;
}

static void *process_soc_data(void *arg)
{
    int retval = STATUS_ERROR;
    fd_set readfds, saved_readfds;
    wdsdaemon *wds = (wdsdaemon *) arg;
    int max, src_fd = 0, dst_fd = 0;
    size_t sz = 0;
    unsigned char *buf_in = NULL;
    int arr[5], num = 0;

    FD_ZERO(&readfds);
    FD_ZERO(&saved_readfds);

    if (wds->mode == MODE_BT_UART || wds->mode == MODE_ANT_UART ||
            wds->mode == MODE_FM_UART) {
        FD_SET(wds->soc_if.uart.uart_fd, &saved_readfds);
        max = wds->soc_if.uart.uart_fd;
        sz = UART_BUF_SIZE * sizeof(unsigned char);
    } else {
        if (wds->mode == MODE_BT_SMD || wds->mode == MODE_ALL_SMD) {
            FD_SET((arr[num] = wds->soc_if.smd.bt_acl_fd), &saved_readfds);num++;
            FD_SET((arr[num] = wds->soc_if.smd.bt_cmd_fd), &saved_readfds);num++;
        }
        if (wds->mode == MODE_ANT_SMD || wds->mode == MODE_ALL_SMD) {
            FD_SET((arr[num] = wds->soc_if.smd.ant_cmd_fd), &saved_readfds);num++;
            FD_SET((arr[num] = wds->soc_if.smd.ant_data_fd), &saved_readfds);num++;
        }

        if (wds->mode == MODE_FM_SMD || wds->mode == MODE_ALL_SMD)
            FD_SET((arr[num] = wds->soc_if.smd.fm_cmd_fd), &saved_readfds);num++;

        max = find_max(arr, num);

        sz = SMD_BUF_SIZE * sizeof(unsigned char);
    }

    buf_in = (unsigned char *) calloc(sz, 1);
    if (!buf_in) {
        ERROR("Insufficient Memory");
        retval = STATUS_NO_MEMORY;
        goto failed;
    }

    do {
        readfds = saved_readfds;
        retval = select(max + 1, &readfds, NULL, NULL, NULL);
        if (retval == -1) {
            ERROR("select failed, Error: %s (%d)\n", strerror(errno),
                                    errno);
            break;
        }

        switch (wds->mode) {
        case MODE_BT_UART:
        case MODE_FM_UART:
        case MODE_ANT_UART:
            src_fd = wds->soc_if.uart.uart_fd;
            if (FD_ISSET(src_fd,&readfds))
                retval = process_soc_data_to_pc(wds, buf_in, src_fd);
            break;
        case MODE_ALL_SMD:
        case MODE_BT_SMD:
            src_fd = wds->soc_if.smd.bt_cmd_fd;
            if (FD_ISSET(src_fd ,&readfds)) {
                buf_in[0] = BT_EVT_PKT_ID;
                retval = process_soc_data_to_pc(wds, buf_in,src_fd);
            }
            src_fd = wds->soc_if.smd.bt_acl_fd;
            if (FD_ISSET(src_fd ,&readfds)) {
                buf_in[0] = BT_ACL_DATA_PKT_ID;
                retval = process_soc_data_to_pc(wds, buf_in, src_fd);
            }
            if (wds->mode == MODE_BT_SMD)
                break;
        case MODE_FM_SMD:
            src_fd = wds->soc_if.smd.fm_cmd_fd;
            if (FD_ISSET(src_fd ,&readfds)) {
                buf_in[0] = FM_EVT_PKT_ID;
                retval = process_soc_data_to_pc(wds, buf_in, src_fd);
            }
            if (wds->mode == MODE_FM_SMD)
                break;
            break;
        case MODE_ANT_SMD:
            src_fd = wds->soc_if.smd.ant_cmd_fd;
            if (FD_ISSET(src_fd, &readfds)) {
                buf_in[0] = ANT_EVT_PKT_ID;
                retval = process_soc_data_to_pc(wds, buf_in, src_fd);
            }
            src_fd = wds->soc_if.smd.ant_data_fd;
            if (FD_ISSET(src_fd ,&readfds)) {
                buf_in[0] = ANT_DATA_PKT_ID;
                retval = process_soc_data_to_pc(wds, buf_in, src_fd);
            }
            break;
        }

        if (STATUS_SUCCESS != retval) {
            if (retval == STATUS_CLIENT_ERROR) {
                ERROR("Write to client failed\n");
                continue;
            }
            ERROR("Failed to process SOC data\n");
            break;
        }
    } while(1);

failed:
    ERROR("\nReader thread exited\n");
    if (buf_in) {
        free(buf_in);
        buf_in = NULL;
    }
    return 0;
}

static int set_port_raw_mode(int fd)
{
    struct termios term;
    int ret = STATUS_SUCCESS;

    if (tcgetattr(fd, &term) < 0) {
        ERROR("Failed to get attributes");
        ERROR("Error: %s (%d)", strerror(errno), errno);
        return STATUS_ERROR;
    }
    cfmakeraw(&term);
    if (tcsetattr(fd, TCSANOW, &term) < 0) {
        ERROR("Failed to set attributes");
        ERROR("Error: %s (%d)", strerror(errno), errno);
        return STATUS_ERROR;
    }
    if (tcflush(fd, TCIFLUSH) < 0) {
        ERROR("Failed to flush port");
        ERROR("Error: %s (%d)", strerror(errno), errno);
        return STATUS_ERROR;
    }

    return ret;
}

/*===========================================================================
FUNCTION   port_init_libbt

DESCRIPTION
Initilize port and open the file through libbt-vendor

DEPENDENCIES
NIL

RETURN VALUE
RETURN fd handle

SIDE EFFECTS
None

===========================================================================*/
#ifndef BT_BLUEZ
static int port_init_libbt(uint8 option)
{
   int fd_array[CH_MAX];
   bt_vendor_callbacks_t cb;
   uint8_t init_bd_addr[6] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 };
   bt_vendor_interface_t * p_btf = NULL;
   bt_vendor_opcode_t opCmd1, opCmd2;
   int iState;

   void* vendor_handle = dlopen("libbt-vendor.so", RTLD_NOW);
   if(!vendor_handle){
      ERROR("Error open libbt-vendor \n");
      return -1;
   }
   p_btf = (bt_vendor_interface_t *)dlsym(vendor_handle,
                    "BLUETOOTH_VENDOR_LIB_INTERFACE");
   if(!p_btf){
      ERROR("Failed obtain the address of libbt-vendor \n");
      return -1;
   }
   if (p_btf->init(&cb, &init_bd_addr[0]) < 0){
      ERROR("bt vendor init failed \n");
      return -1;
   }

   switch (option) {
       case MODE_BT_UART:
           opCmd1 = BT_VND_OP_POWER_CTRL;
           opCmd2 = BT_VND_OP_USERIAL_OPEN;
           break;
       case MODE_FM_UART:
           opCmd1 = FM_VND_OP_POWER_CTRL;
           opCmd2 = BT_VND_OP_FM_USERIAL_OPEN;
           break;
       case MODE_ANT_UART:
           opCmd1 = BT_VND_OP_POWER_CTRL;
           opCmd2 = BT_VND_OP_USERIAL_OPEN;
           break;
       default:
           printf("Invalid option\n");
           return -1;
   }

   iState = BT_VND_PWR_ON;

   if (p_btf->op(opCmd1, &iState) < 0){
      ERROR("Power on failed \n");
      return -1;
   }

   if (p_btf->op(opCmd2, (void*)fd_array) < 0){
      ERROR("op(VND_OP_USERIAL_OPEN) failed \n");
      return -1;
   }

   return fd_array[0];
}
#endif

static int change_baud(int fd, speed_t baud)
{
    struct termios term;
    int ret = STATUS_SUCCESS; /* assume success */

    do {
        if (tcgetattr(fd, &term) < 0) {
            ERROR("Failed to get attributes");
            ret = STATUS_ERROR;
            break;
        }
        cfsetospeed(&term, baud);
        /* don't change speed until last write done */
        if (tcsetattr(fd, TCSADRAIN, &term) < 0) {
            ERROR("Failed to set attribute");
            ERROR("Error: %s (%d)", strerror(errno), errno);
            ret = STATUS_ERROR;
            break;
        }
    } while(0);
    return 0;
}

int init_soc_interface(wdsdaemon *wds)
{
    int ret = STATUS_ERROR;
    int fd = 0;
    struct termios term;

    if (!wds) {
        ret = STATUS_NULL_POINTER;
        ERROR("Invalid input argument\n");
        return ret;
    }

    switch (wds->mode) {
    case MODE_FM_UART:
        if(hidl_client_initialize(MODE_FM,&fd) == false)
            {
                ERROR("HIDL client initialization failed");
                exit(1);
            }
        wds->soc_if.uart.uart_fd = fd;
        ret = STATUS_SUCCESS;
        break;
    case MODE_BT_UART:
#ifdef BT_BLUEZ
        fd = open(wds->soc_if.uart.intf,
                    O_RDWR | O_NONBLOCK | O_NOCTTY);
        if (-1 == fd) {
            ERROR("Failed to open port: %s\n",
                        wds->soc_if.uart.intf);
            ERROR("Error: %s (%d)", strerror(errno), errno);
            break;
        }
        if (tcflush(fd, TCIOFLUSH) < 0) {
            ERROR("Failed to flush port: %s\n",
                        wds->soc_if.uart.intf);
            ERROR("Error: %s (%d)", strerror(errno), errno);
            close(fd);
            break;
        }
        if (tcgetattr(fd, &term) < 0) {
            ERROR("Failed to get attributes for port: %s\n",
                        wds->soc_if.uart.intf);
            ERROR("Error: %s (%d)", strerror(errno), errno);
            close(fd);
            break;
        }
        cfmakeraw(&term);
        /* enable flow control */
        term.c_cflag |= (CRTSCTS | CLOCAL);
        if (tcsetattr(fd, TCSANOW, &term) < 0) {
            ERROR("Failed to set attributes for port: %s\n",
                        wds->soc_if.uart.intf);
            ERROR("Error: %s (%d)", strerror(errno), errno);
            close(fd);
            break;
        }
        if (STATUS_SUCCESS != change_baud(fd, B3000000)) {      //TODO:set baud rate
            ERROR("Failed to change baud rate\n");
            close(fd);
            break;
        }
#else
        if(hidl_client_initialize(MODE_BT,&fd) == false)
        {
            ERROR("HIDL client initialization failed");
            exit(1);
        }
#endif
        /* everything okay */
        wds->soc_if.uart.uart_fd = fd;
        ret = STATUS_SUCCESS;
        break;
        case MODE_ANT_UART:
#ifndef BT_BLUEZ
        if(hidl_client_initialize(MODE_ANT,&fd) == true){
            wds->soc_if.uart.uart_fd = fd;
        }
        else {
            ERROR("HIDL client initialization failed, opening port with port_init_libbt\n");
            wds->soc_if.uart.uart_fd  = port_init_libbt(wds->mode);
        }
        ret = STATUS_SUCCESS;
#endif
        break;
        case MODE_ALL_SMD:
        case MODE_ANT_SMD:
            /* ANT commdnas */
            fd = open(wds->soc_if.smd.ant_cmd,
                    O_RDWR | O_NONBLOCK | O_NOCTTY);
            if (-1 == fd) {
                ERROR("Failed to open port: %s\n",
                        wds->soc_if.smd.ant_cmd);
                ERROR("Error: %s (%d)\n", strerror(errno),
                                    errno);
                break;
            }
            set_port_raw_mode(fd);
            wds->soc_if.smd.ant_cmd_fd = fd;
            /* ANT data */
            fd = open(wds->soc_if.smd.ant_data,
                    O_RDWR | O_NONBLOCK | O_NOCTTY);
            if (-1 == fd) {
                ERROR("Failed to open port: %s\n",
                        wds->soc_if.smd.ant_data);
                ERROR("Error: %s (%d)", strerror(errno),
                                    errno);
                break;
            }
            set_port_raw_mode(fd);
            wds->soc_if.smd.ant_data_fd = fd;
            if (wds->mode == MODE_ANT_SMD) {
                ret = STATUS_SUCCESS;
                break;
            }
        /* fallthrough intentional for MODE_ALL_SMD */
        case MODE_BT_SMD:
            /* BT commdnas */
            fd = open(wds->soc_if.smd.bt_cmd,
                    O_RDWR | O_NONBLOCK | O_NOCTTY);
            if (-1 == fd) {
                ERROR("Failed to open port: %s\n",
                        wds->soc_if.smd.bt_cmd_fd);
                ERROR("Error: %s (%d)\n",
                        strerror(errno), errno);
                break;
            }
            set_port_raw_mode(fd);
            wds->soc_if.smd.bt_cmd_fd = fd;
            /* BT ACL */
            fd = open(wds->soc_if.smd.bt_acl,
                    O_RDWR | O_NONBLOCK | O_NOCTTY);
            if (-1 == fd) {
                ERROR("Failed to open port: %s\n",
                        wds->soc_if.smd.bt_acl);
                ERROR("Error: %s (%d)\n",
                        strerror(errno), errno);
                break;
            }
            set_port_raw_mode(fd);
            wds->soc_if.smd.bt_acl_fd = fd;
            if (wds->mode == MODE_BT_SMD) {
                ret = STATUS_SUCCESS;
                break;
            }
        /* fallthrough intentional for MODE_ALL_SMD */
        case MODE_FM_SMD:
            /* FM commdnas */
            fd = open(wds->soc_if.smd.fm_cmd, O_RDWR | O_NONBLOCK | O_NOCTTY);
            if (-1 == fd) {
                ERROR("Failed to open port: %s\n",
                        wds->soc_if.smd.fm_cmd_fd);
                ERROR("Error: %s (%d)\n",
                        strerror(errno), errno);
                break;
            }
            set_port_raw_mode(fd);
            wds->soc_if.smd.fm_cmd_fd = fd;
            ret = STATUS_SUCCESS;
            break;
    }

    if (ret == STATUS_SUCCESS)
        if (pthread_create(&wds->soc_rthread, NULL, process_soc_data,
                                    wds) != 0) {
            ERROR("%s:Unable to create pthread err = %s\n", __func__,
                    strerror(errno));
            close(fd);
            ret = STATUS_ERROR;
        }

failed:
    return ret;
}

int init_pc_interface(wdsdaemon *wds)
{
    int fd = 0;
    int ret = STATUS_ERROR;
    struct termios term;

    if (!wds) {
        ret = STATUS_NULL_POINTER;
        ERROR("Invalid input argument");
        return ret;
    }

    do {
        fd = open(wds->pc_if.uart.intf, O_RDWR);
        if (-1 == fd) {
            ERROR("Unable to open port: %s", wds->pc_if.uart.intf);
            ERROR("Error: %s (%d)", strerror(errno), errno);
            ret = STATUS_ERROR;
            break;
        }
        /* set terminal properties */
        if (tcgetattr(fd, &term) < 0) {
            ERROR("Failed to get attributes of port: %s",
                                wds->pc_if.uart.intf);
            ERROR("Error: %s (%d)", strerror(errno), errno);
            close(fd);
            ret = STATUS_ERROR;
            break;
        }
        cfmakeraw(&term);
        term.c_lflag = term.c_lflag & ((tcflag_t)(~ECHO));
        /* TODO: Make baud rate command line argument */
        cfsetospeed(&term, B115200);
        cfsetispeed(&term, B115200);
        if (tcsetattr(fd, TCSANOW, &term) < 0) {
            ERROR("Failed to set attributes of port: %s",
                                wds->pc_if.uart.intf);
            ERROR("Error: %s (%d)", strerror(errno), errno);
            close(fd);
            ret = STATUS_ERROR;
            break;
        }
        tcflush(fd, TCIOFLUSH);

        /* everything okay, set success */
        wds->pc_if.uart.uart_fd = fd;
        ret = STATUS_SUCCESS;
    } while(0);

    return ret;
}

int establish_server_socket(wdsdaemon *wds)
{
    int fd = -1;
    struct sockaddr_un client_address;
    socklen_t clen;
    int sock_id, ret = STATUS_ERROR;
    DEBUG("%s(%s) Entry  \n", __func__, SOCKET_NAME);

    if (!wds) {
        ret = STATUS_NULL_POINTER;
        ERROR("Invalid input argument\n");
        return ret;
    }

    sock_id = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (sock_id < 0) {
        ERROR("%s: server Socket creation failure\n", __func__);
        return ret;
    }

    DEBUG("convert name to android abstract name:%s %d\n", SOCKET_NAME, sock_id);
    if (socket_local_server_bind(sock_id,
        SOCKET_NAME, ANDROID_SOCKET_NAMESPACE_ABSTRACT) >= 0) {
        if (listen(sock_id, 5) == 0) {
            DEBUG("listen to local socket:%s, fd:%d\n", SOCKET_NAME, sock_id);
        } else {
            ERROR("listen to local socket:failed\n");
            close(sock_id);
            return ret;
        }
    } else {
        close(sock_id);
        ERROR("%s: server bind failed for socket : %s\n", __func__, SOCKET_NAME);
        return ret;
    }

    clen = sizeof(client_address);
    DEBUG("%s: before accept_server_socket\n", SOCKET_NAME);
    fd = accept(sock_id, (struct sockaddr *)&client_address, &clen);
    if (fd > 0) {
        DEBUG("%s accepted fd:%d for server fd:%d\n", SOCKET_NAME, fd, sock_id);
        close(sock_id);
        wds->server_socket_fd = fd;
        return STATUS_SUCCESS;
    } else {
        ERROR("Accept failed fd:%d sock d:%d error %s\n", fd, sock_id, strerror(errno));
        close(sock_id);
        return ret;
    }
}
