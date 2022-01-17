#ifndef DIAG_LSMI_H
#define DIAG_LSMI_H

/*===========================================================================

                   Diag Mapping Layer DLL , internal declarations

DESCRIPTION
  Internal declarations for Diag Service Mapping Layer.


Copyright (c) 2007-2015 Qualcomm Technologies, Inc.  All Rights Reserved.
Qualcomm Technologies Proprietary and Confidential
===========================================================================*/

/*===========================================================================

                        EDIT HISTORY FOR MODULE

This section contains comments describing changes made to the module.
Notice that changes are listed in reverse chronological order.

$Header:

when       who     what, where, why
--------   ---     ----------------------------------------------------------
10/01/08   sj      Added featurization for WM specific code & CBSP2.0
02/04/08   mad     Created File
===========================================================================*/

#define DIAG_INVALID_HANDLE -1
#define DIAG_MDLOG_DIR		"/tmp/diag_logs/"
#define DIAG_MDLOG_PID_FILE	"/tmp/diag_logs/diag_mdlog_pid"
#define DIAG_MDLOG_PID_FILE_SZ  100
#define NUM_PROC 10
#define HDLC_DISABLE	1
extern int diag_fd;
extern int fd_md[NUM_PROC];
extern int gdwClientID;
void log_to_device(unsigned char *ptr, int logging_mode, int size, int type);
void send_mask_modem(unsigned char mask_buf[], int count_mask_bytes);
int diag_has_remote_device(uint16 *remote_mask);
int diag_register_socket_cb(int (*callback_ptr)(void *data_ptr, int socket_id), void *data_ptr);
int diag_set_socket_fd(int socket_id, int socket_fd);
int diag_send_socket_data(int id, unsigned char buf[], int num_bytes);
int diag_get_max_channels(void);
int diag_read_mask_file_list(char *mask_list_file);

/* === Functions dealing with diag wakelocks === */

/* Returns 1 if a wakelock is initialized for this process,
   0 otherwise. */
int diag_is_wakelock_init(void);

/* Opens the wakelock files and initializes the wakelock for
   the current process. It doesn't hold any wakelock. To hold
   a wakelock, call diag_wakelock_acquire. */
void diag_wakelock_init(char *wakelock_name);

/* Closes the wakelock files. It doesn't release the wakelock
   for the current process if held. */
void diag_wakelock_destroy(void);

/* Acquires a wakelock for the current process under the name
   given by diag_wakelock_init. */
void diag_wakelock_acquire(void);

/* Releases the wakelock held by the current process. */
void diag_wakelock_release(void);

/* To convert an integer/hexadecimal string to an integer */
int to_integer(char *str);

/* Request  the kernel diag to turn on/off the hdlc encoding of the data. */
int diag_hdlc_toggle(uint8 hdlc_support);

/* === Functions dealing with qshrink4 === */

/* Creates threads to read the qshrink4 database threads. */
int create_diag_qshrink4_db_parser_thread(unsigned int peripheral_mask);

/* Parses the data for qshrink4 command response */
int parse_data_for_qsr4_db_file_op_rsp(uint8 *ptr, int count_received_bytes);

/* Add qshrink4 guid information to qmdl2 header */
int add_guid_to_qshrink4_header(unsigned char * guid);

/* Notify parser thread when a PD comes up */
void diag_notify_parser_thread(int type, int peripheral_mask);

void diag_set_peripheral_mask(unsigned int peripheral_mask);
void diag_get_peripheral_name_from_mask(char *peripheral_name,
					unsigned int len,
					unsigned int peripheral_mask);
void diag_kill_qshrink4_threads(void);

extern boolean gbRemote;
#define DIAG_LSM_PKT_EVENT_PREFIX "DIAG_SYNC_EVENT_PKT_"
#define DIAG_LSM_MASK_EVENT_PREFIX "DIAG_SYNC_EVENT_MASK_"
#endif /* DIAG_LSMI_H */

