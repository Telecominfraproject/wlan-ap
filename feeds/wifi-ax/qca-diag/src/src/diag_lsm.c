/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*
Copyright (c) 2007-2016 Qualcomm Technologies, Inc.
All Rights Reserved.
Confidential and Proprietary - Qualcomm Technologies, Inc.

              Diag Legacy Service Mapping DLL

GENERAL DESCRIPTION

Implementation of entry point and Initialization functions for Diag_LSM.dll.


EXTERNALIZED FUNCTIONS
DllMain
Diag_LSM_Init
Diag_LSM_DeInit

INITIALIZATION AND SEQUENCING REQUIREMENTS


*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*/

/*===========================================================================

                        EDIT HISTORY FOR MODULE

$Header:

when       who    what, where, why
--------   ---    ----------------------------------------------------------
10/01/08   SJ     Ported WM7 code onto Linux for CBSP2.0
04/23/08   JV     Added calls to functions that update masks during bring-up.
                  LSM registers for a mask change during Init.Now calling
				  diagpkt_bindpkt with the UnBind ID during de-init.
04/14/08   JV     Created instance of IDiagPktRsp
03/19/08   JV     Added packet service to init and de-init functions.
02/05/08   mad    Moved declarations for Diag_LSM_Init() and Diag_LSM_DeInit()
                  to Diag_LSM.h. This will enable clients to include
                  Diag_LSM.h and call the functions directly.
01/15/08   mad    Removed explicit linking to a1Host.lib, now using
                  LoadLibrary() and GetProcAddress() to call functions
                  exported by a1Host.dll. This was done to avoid a1Host.dll
                  being loaded implicitly upon loading of Diag_LSM.dll.
11/29/07   mad    Created

===========================================================================*/

#include <stdlib.h>
#include "comdef.h"
#include "stdio.h"
#include "diag_lsmi.h"
#include "./../include/diag_lsm.h"
#include "diagsvc_malloc.h"
#include "diag_lsm_event_i.h"
#include "diag_lsm_log_i.h"
#include "diag_lsm_msg_i.h"
#include "diag.h" /* For definition of diag_cmd_rsp */
#include "diag_lsm_pkt_i.h"
#include "diag_lsm_dci_i.h"
#include "diag_lsm_dci.h"
#include "diag_shared_i.h" /* For different constants */
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include "errno.h"
#include <pthread.h>
#include <stdint.h>
#include <eventi.h>
#include <signal.h>
#include<sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/select.h>
#include <ctype.h>
#include <limits.h>

#define std_strlprintf     snprintf
/* strlcpy is from OpenBSD and not supported by Linux Embedded.
 * GNU has an equivalent g_strlcpy implementation into glib.
 * Featurized with compile time USE_GLIB flag for Linux Embedded builds.
 */
#ifdef USE_GLIB
#define strlcpy g_strlcpy
#endif

#define MAX_CHANNELS 3

#define DCI_HEADER_LENGTH	sizeof(int)
#define DCI_LEN_FIELD_LENGTH	sizeof(int)
#define DCI_EVENT_OFFSET	sizeof(uint16)
#define DCI_DEL_FLAG_LEN	sizeof(uint8)
#define DCI_EXT_HDR_LENGTH	8

#define CALLBACK_TYPE_LEN	4
#define CALLBACK_PROC_TYPE_LEN	4

#define MAX_GUID_ENTRIES 128
	static unsigned int diag_peripheral_mask = 0;
	int gdwClientID = 0;
	int diag_fd = -1;		/* File descriptor for DIAG device */
	/* File descriptor for Memory device */
	int fd_md[NUM_PROC] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
	int fd_uart = -1;	/* File descriptor for UART device */
	int fd_socket[MAX_CHANNELS] = {-1, -1, -1};	/* File descriptor for socket */
	int fd_dev = -1;	/* Generic file descriptor */
	int num_bytes_read;
	unsigned long count_written_bytes[NUM_PROC];
	/* This is for non MEMORY logging*/
	unsigned long count_written_bytes_1 = 0;
	char file_name_curr[NUM_PROC][FILE_NAME_LEN];
	char file_name_del[FILE_NAME_LEN] = "/sdcard/diag_logs/diag_log_";
	char mask_file[FILE_NAME_LEN] = "/sdcard/diag_logs/Diag.cfg";
	char mask_file2[FILE_NAME_LEN] = "/sdcard/diag_logs/Diag.cfg2";
	char mask_file_mdm[FILE_NAME_LEN] = "/sdcard/diag_logs/Diag.cfg";
	char mask_file_mdm2[FILE_NAME_LEN] = "/sdcard/diag_logs/Diag.cfg2";
	char output_dir[NUM_PROC][FILE_NAME_LEN] = {
				"/sdcard/diag_logs/"
	};
	char qsr4_xml_file_name[FILE_NAME_LEN] = "/sdcard/diag_logs/";
	/* This array is used for proc names */
	char proc_name[NUM_PROC][6] = {"", "/mdm", "/mdm2", "/mdm3", "/mdm4", "/qsc", "",
								"", "", ""};
	int logging_mode = USB_MODE;
	int uart_logging_proc = MSM;
	char* file_list[NUM_PROC] = {NULL, NULL, NULL, NULL, NULL,
					NULL, NULL, NULL, NULL, NULL};
	int file_list_size[NUM_PROC] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	int file_list_index[NUM_PROC] = {-1, -1, -1, -1, -1, -1, -1,
								-1, -1, -1};
	char dir_name[FILE_NAME_LEN];
	char peripheral_name[FILE_NAME_LEN];

static struct diag_callback_tbl_t cb_clients[NUM_PROC];
static int socket_inited = 0;
static int (*socket_cb_ptr)(void *socket_cb_data_ptr, int socket_id);
static void *socket_cb_data_ptr;
static int socket_token[MAX_CHANNELS];
static unsigned int file_count = 0;
static unsigned int circular_logging_inited = 0;
static int lsm_init_count = 0;
static void init_circular_logging(void);
static int create_oldest_file_list(char *oldest_dir, int type);
unsigned int device_num = 0;

/* Globals related to diag wakelocks */
#define WAKELOCK_FILE		"/sys/power/wake_lock"
#define WAKEUNLOCK_FILE		"/sys/power/wake_unlock"
static int wl_inited;
static int fd_wl;
static int fd_wul;
static char *wl_name;
uint8 hdlc_disabled;
uint8 write_qshrink_header;


/* This event will be set (by DCM) when a request arrives for this process. */
//HANDLE ghReq_Sync_Event = NULL;

/* This event will be set (by DCM) when a mask change occurs. */
//HANDLE ghMask_Sync_Event = NULL;

#define NUM_SYNC_EVENTS_DIAG_LSM 2
#define SYNC_EVENT_DIAG_LSM_PKT_IDX 0
#define SYNC_EVENT_DIAG_LSM_MASK_IDX 1
#define READ_BUF_SIZE 100000
#define DISK_BUF_SIZE 1024*140
#define DISK_FLUSH_THRESHOLD  1024*128

#define FILE_LIST_NAME_SIZE 100
#define MAX_FILES_IN_FILE_LIST 100

static void *CreateWaitThread(void* param);
unsigned char read_buffer[READ_BUF_SIZE];

pthread_t read_thread_hdl;	/* Diag Read thread handle */
pthread_t disk_write_hdl;	/* Diag disk write thread handle */

unsigned long max_file_size = 100000000;
unsigned long min_file_size = 80000000; /* 80 percent of max size */
unsigned int log_to_memory ;
int cleanup_mask;	/*Control sending of empty mask to modem */
int diag_disable_console = 0; /*Variable to control console message */
unsigned int max_file_num = 0x7FFFFFFF;

#define RENAME_CMD_LEN ((2*FILE_NAME_LEN) + 10)

int rename_file_names = 0;	/* Rename file name on close to current time */
int rename_dir_name = 0;	/* Rename directory name to current time when ODL is halted */

/* Static array for workaround */
static unsigned char static_buffer[6][DISK_BUF_SIZE];

/* Externalized functions */
static pthread_once_t mask_sync_is_inited = PTHREAD_ONCE_INIT;
static pthread_mutex_t mask_sync_mutex;
static int diag_sync_mask = 0;
static pthread_mutex_t lsm_init_count_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t hdlc_toggle_mutex = PTHREAD_MUTEX_INITIALIZER;

#define MASK_SYNC_COMPLETE (MSG_MASKS_TYPE | LOG_MASKS_TYPE | EVENT_MASKS_TYPE)
#define MAX_MASK_SYNC_COUNT 100

extern qsr4_db_file_parser_state parser_state;
char pid_file[DIAG_MDLOG_PID_FILE_SZ];

void mask_sync_initialize(void)
{
	pthread_mutex_init(&mask_sync_mutex, NULL);
}

int get_sync_mask(void)
{
	int sync_mask;

	pthread_once(&mask_sync_is_inited, mask_sync_initialize);
	pthread_mutex_lock(&mask_sync_mutex);
	sync_mask = diag_sync_mask;
	pthread_mutex_unlock(&mask_sync_mutex);

	return sync_mask;
}

void update_sync_mask(int mask)
{
	pthread_once(&mask_sync_is_inited, mask_sync_initialize);
	pthread_mutex_lock(&mask_sync_mutex);
	diag_sync_mask |= mask;
	pthread_mutex_unlock(&mask_sync_mutex);
}

int do_mask_sync(void)
{
	int sync_mask = get_sync_mask();
	int count = 0;
	int success = 1;

	while (sync_mask != MASK_SYNC_COMPLETE)
	{
		usleep(1000);
		sync_mask = get_sync_mask();
		count++;
		if (count > MAX_MASK_SYNC_COUNT) {
			DIAG_LOGE("diag: In %s, mask sync error, count: %d\n",
				__func__, count);
			success = 0;
			break;
		}
	}

	return success;
}

void diag_wakelock_init(char *wakelock_name)
{
	/* The max permissible length of the wakelock string. The value
	   25 was chosen randomly */
	int wl_name_len = 25;

	if (!wakelock_name) {
		DIAG_LOGE("diag: In %s, invalid wakelock name\n", __func__);
		goto fail_init;
	}

	/* Wake lock is already initialized */
	if (wl_inited && wl_name) {
		/* Check if the wakelock name is the same as the wakelock held.
		   If not, just print a warning and continue to use the old
		   wakelock name */
		if (strncmp(wakelock_name, wl_name, wl_name_len))
			DIAG_LOGE("diag: %s, already holding another wakelock in this process\n", __func__);
		return;
	}

	wl_name = (char *)malloc(wl_name_len);
	if (!wl_name) {
		DIAG_LOGE("diag: In %s, cannot allocate memory for wl_name\n", __func__);
		goto fail_init;
	}
	strlcpy(wl_name, wakelock_name, wl_name_len);

	fd_wl = open(WAKELOCK_FILE, O_WRONLY|O_APPEND);
	if (fd_wl < 0) {
		DIAG_LOGE("diag: could not open wakelock file, errno: %d\n", errno);
		goto fail_init;
	} else {
		fd_wul = open(WAKEUNLOCK_FILE, O_WRONLY|O_APPEND);
		if (fd_wul < 0) {
			DIAG_LOGE("diag: could not open wake-unlock file, errno: %d\n", errno);
			close(fd_wl);
			goto fail_init;
		}
	}

	wl_inited = TRUE;
	return;

fail_init:
	wl_inited = FALSE;
}

void diag_wakelock_destroy()
{
	if (!wl_inited) {
		DIAG_LOGE("diag: %s, wakelock files are not initialized\n", __func__);
		return;
	}

	close(fd_wl);
	close(fd_wul);
}

int diag_is_wakelock_init()
{
	return wl_inited;
}

void diag_wakelock_acquire()
{
	int status = -1;
	if (!wl_inited) {
		DIAG_LOGE("diag: %s, wakelock files are not initialized\n", __func__);
		return;
	}

	if (!wl_name) {
		DIAG_LOGE("diag: In %s, invalid wakelock name\n", __func__);
		return;
	}

	status = write(fd_wl, wl_name, strlen(wl_name));
	if(status != (int)strlen(wl_name))
		DIAG_LOGE("diag: error writing to wakelock file");
}

void diag_wakelock_release()
{
	int status = -1;
	if (!wl_inited) {
		DIAG_LOGE("diag: %s, wakelock files are not initialized\n", __func__);
		return;
	}

	if (!wl_name) {
		DIAG_LOGE("diag: In %s, invalid wakelock name\n", __func__);
		return;
	}

	status = write(fd_wul, wl_name, strlen(wl_name));
	if(status != (int)strlen(wl_name))
		DIAG_LOGE("diag: error writing to wake unlock file");
}


/*========= variables for optimisation =======*/

static int curr_write;
static int curr_read;
static int  write_in_progress;
pthread_mutex_t stop_mutex;
pthread_cond_t stop_cond;

static char cmd_disable_log_mask[] = { 0x73, 0, 0, 0, 0, 0, 0, 0};
static char cmd_disable_msg_mask[] = { 0x7D, 0x05, 0, 0, 0, 0, 0, 0};
static char cmd_disable_event_mask[] = { 0x60, 0};

typedef PACK(struct) {
	uint32 header_length;
	uint8 version;
	uint8 hdlc_data_type;
	uint32 guid_list_entry_count;
	uint32 guid[MAX_GUID_ENTRIES][GUID_LEN];
}qshrink4_header;

static qshrink4_header qshrink4_data;

struct buffer_pool {
	int free;
	int data_ready;
	unsigned int bytes_in_buff[NUM_PROC];
	unsigned char *buffer_ptr[NUM_PROC];
	pthread_mutex_t write_mutex;
	pthread_cond_t write_cond;
	pthread_mutex_t read_mutex;
	pthread_cond_t read_cond;

};

enum status {
	NOT_READY,
	READY,
};

unsigned char *pool0_buffers[NUM_PROC];
unsigned char *pool1_buffers[NUM_PROC];
enum status buffer_init[NUM_PROC];
int token_list[NUM_PROC] = {0, -1, -2, -3, -4, -5, -6, -7, -8, -9};

struct buffer_pool pools[] = {
	[0] = {
		.free		=	1,
		.data_ready	=	0,
	},
	[1] = {
		.free		=	1,
		.data_ready	=	0,
	},

};

int diag_get_max_channels(void)
{
	return MAX_CHANNELS;
}

int to_integer(char *str)
{
	int hex_int = 0;
	int i = 0, is_hex = 0;
	char *p;
	if (str) {
		while(str[i] != '\0') {
			if(str[i] == 'x' || str[i] == 'X') {
				sscanf(str, "%i", &hex_int);
				is_hex = 1;
				break;
			}
			i++;
		}
		if(is_hex == 0)
			hex_int = strtol(str, &p, 10);
	}
	return hex_int;
}

/*==========================================================================
FUNCTION   diag_qshrink4_init

DESCRIPTION
 In case for HDLC encoding is disabled the qmdl2 file will
 contain the header information for the qshrink4 messages.

 The current header structure is as follows:


Field				Length (in bytes)			Description
HeaderLength				4			Number of bytes reserved for header.

Version				1			Version value to set

HDLC DataType				1			0 - indicates hdlc encoding removed
                                                                1-indicates hdlc encoding enabled

GuidListEntryCount			4			Number of Guids available to read

GUID[GuidListEntryCount]	16*( GuidListEntryCount)	An array of available GUIDs.This can expand
							upto the maximum number of GUIDlist entries reserved in diag.

Any data will be followed by header

SIDE EFFECTS
  None

===========================================================================*/

static void diag_qshrink4_init(void)
{
	qshrink4_data.header_length = 10;
	qshrink4_data.version = 1;
	qshrink4_data.hdlc_data_type = 0;
	qshrink4_data.guid_list_entry_count = 0;
}

static void diag_socket_init(void)
{
	if (!socket_inited) {
		int i;
		for (i = 0; i < MAX_CHANNELS; i++) {
			fd_socket[i] = -1;
			if (i > MSM)
				socket_token[i] = -i;
			else
				socket_token[i] = 0;
		}
		socket_cb_ptr = NULL;
		socket_cb_data_ptr = NULL;
		socket_inited = 1;
	} else {
		DIAG_LOGE("diag: In %s, Socket info already initialized\n", __func__);
	}
}

/*==========================================================================
FUNCTION   diag_has_remote_device

DESCRIPTION
  This function queries the kernel to determine if the target device has a
  remote device or not.
  remote_mask - 0, if the target does not have a remote device; otherwise
		a bit mask representing remote channels supported by the target

RETURN VALUE
  Returns 1 on success.  Anything else is a failure.

SIDE EFFECTS
  None

===========================================================================*/
int diag_has_remote_device(uint16 *remote_mask)
{
	return(ioctl(diag_fd, DIAG_IOCTL_REMOTE_DEV, remote_mask, 0, NULL, 0, NULL, NULL));
}

/*==========================================================================
FUNCTION   diag_register_socket_cb

DESCRIPTION
  This function is used to register a callback function in the case that
  remote data is discovered that does not have a corresponding socket.
  The purpose of the callback function is so that app can create a socket
  to be used to read/write the remote data.

  callback_ptr - the function pointer to be called
  cb_data_ptr -	a pointer to data that will be passed when the callback
		function is called

  The callback function must have the following signature:
  int (*callback_ptr)(void *data_ptr, int socket_id)

  Where:
  data_ptr -	is the cb_data_ptr
  socket_id -	the id to be used when calling diag_send_socket_data()
		to send data for that socket

RETURN VALUE
  Returns 1 on success.  0 on failure.

SIDE EFFECTS
  None

===========================================================================*/
int diag_register_socket_cb(int (*callback_ptr)(void *data_ptr, int socket_id), void *cb_data_ptr)
{
	int success = 0;
	if (callback_ptr) {
		DIAG_LOGE("diag: In %s, registered socket callback function\n", __func__);
		socket_cb_ptr = callback_ptr;
		socket_cb_data_ptr = cb_data_ptr;
		success = 1;
	} else {
		DIAG_LOGE("diag: in %s, Unable to register socket callback function\n", __func__);
	}

	return success;
}

/*==========================================================================
FUNCTION   diag_set_socket_fd

DESCRIPTION
  This function is to be called when registering a socket file descriptor.
  This function should be called anytime the socket file descriptor is changed.
  If the socket is closed, this function should be called with a socket file
  descriptor that has a value of -1.

  socket_id -	the socket id is used internal to the diag library. If the socket
		is for MSM data its value must be 0, regardless of whether the
		target device has a remote device of not.  In the case of a target
		with a remote device, the socket id is assigned by the diag library
		and sent in the socket callback function, see diag_register_socket_cb().
  socket_fd -	the socket file descriptor.

RETURN VALUE
  Returns 1 on success.  0 on failure.

SIDE EFFECTS
  None

===========================================================================*/
int diag_set_socket_fd(int socket_id, int socket_fd)
{
	int success;
	if ((socket_id >= 0) && (socket_id < MAX_CHANNELS)) {
		fd_socket[socket_id] = socket_fd;
		/* DIAG_LOGE("diag: In %s, setting fd_socket[%d] to %d\n", __func__, socket_id, socket_fd); */
		success = 1;
	} else {
		DIAG_LOGE("diag: In %s, Setting of socket fd failed. Invalid socket id: %d\n",
			__func__, socket_id);
		success = 0;
	}

	return success;
}

/*==========================================================================
FUNCTION   diag_send_socket_data

DESCRIPTION
  This function is to be called when sending data read from the socket.

  socket_id -	the socket id is used internal to the diag library. If the
		socket is for MSM data (target with remote device or or not)
		its value must be 0.  In the case of a target with a remote
		device, the socket id is assigned by the diag library and sent
		in the socket callback function, see diag_register_socket_cb().
  buf -		the buffer containing the data
  num_bytes -	the number of bytes of data in the buffer that is to be sent

RETURN VALUE
  Returns 1 on success.  0 on failure.

SIDE EFFECTS
  None

===========================================================================*/
int diag_send_socket_data(int id, unsigned char buf[], int num_bytes)
{
	unsigned char send_buf[4100];
	unsigned char offset = 4;
	int i;
	int start = 0;
	int end = 0;
	int copy_bytes;
	int success;

	if ((id >= 0) && (id < MAX_CHANNELS)) {
		*(int *)send_buf = USER_SPACE_DATA_TYPE;
		if (socket_token[id] != 0 ) {
			*(int *)(send_buf + offset) = socket_token[id];
			offset += 4;
		}

		for (i = 0; i < num_bytes; i++) {
			if (hdlc_disabled) {
				if (buf[i] == CONTROL_CHAR && i == 0) {
					end = end + 1;
					continue;
				}
			}
			if (buf[i] == CONTROL_CHAR) {
				copy_bytes = end-start+1;
				memcpy(send_buf+offset, buf+start, copy_bytes);
				diag_send_data(send_buf, copy_bytes+offset);
				start = i+1;
				end = i+1;
				continue;
			}
			end = end+1;
		}
		success = 1;
	} else {
		DIAG_LOGE("diag: In %s, Error sending socket data. Invalid socket id: %d\n",
			__func__, id);
		success = 0;
	}

	return success;
}

/*==========================================================================
FUNCTION   get_remote_socket_fd

DESCRIPTION
  This function is used internal to the diag library and returns the socket
  file descriptor for the remote data. If there is no valid socket file
  descriptor for the remote data, the registered socket callback function
  (see diag_register_socket_cb) will be called to notify the app that a new
  socket must be created for the remote data.

  token -	the embedded token data that identifies the data as being
		remote data.  For instance, MDM_TOKEN.

RETURN VALUE
  Returns the socket file descriptor on success; -1 otherwise.

SIDE EFFECTS
  None

===========================================================================*/
static int get_remote_socket_fd(int token)
{
	int i;
	int found = 0;
	int fd = -1;
	int status;

	for (i = 1; i < MAX_CHANNELS; i++) {
		if (socket_token[i] == token) {
			fd = fd_socket[i];
			found = i;
			break;
		}
	}

	/* If there is no socket for this token */
	if (!found) {
		for (i = 1; i < MAX_CHANNELS; i++) {
			/* If we have found an unused socket entry */
			if (fd_socket[i] == -1) {
				/* If the socket entry has not begun initialization */
				if (socket_token[i] != token) {
					/* Notify the app that another socket needs to be opened */
					if (socket_cb_ptr) {
						status = (*(socket_cb_ptr))(socket_cb_data_ptr, i);
						socket_token[i] = token;
					} else {
						DIAG_LOGE("diag: In %s, Error, socket callback function has not been registered\n",
							__func__);
					}
				}
				break;
			}
		}
	}

	return fd;
}

/*=========================================================================
 * FUNCTION send_empty_mask
 * DESCRIPTION
 *    This function send empty mask to modem, thus modem stops generating
 *    logs and can enter low power mode.
 * RETURN VALUE
 *    NONE
 * SIDE EFFECTS
 *    NONE
 *========================================================================*/

void send_empty_mask(int type)
{
	int offset = 0;
	int length = 0;
	const uint8 size = 20;
	unsigned char mask_buf[size];
	unsigned char *ptr = mask_buf;

	*(int *)ptr = USER_SPACE_RAW_DATA_TYPE;
	offset += sizeof(int);
	if (type == MDM) {
		*(int *)(ptr + offset) = MDM_TOKEN;
		offset += sizeof(int);
	} else if (type == QSC) {
		*(int *)(ptr + offset) = QSC_TOKEN;
		offset += sizeof(int);
	}

	/* Send Disable Log Mask Command */
	length = sizeof(cmd_disable_log_mask) / sizeof(char);
	if (length + offset <= size) {
		memcpy(ptr + offset, cmd_disable_log_mask, length);
		diag_send_data(ptr, offset + length);
	}

	/* Send Disable Msg Mask Command */
	length = sizeof(cmd_disable_msg_mask) / sizeof(char);
	if (length + offset <= size) {
		memcpy(ptr + offset, cmd_disable_msg_mask, length);
		diag_send_data(ptr, offset + length);
	}

	/* Send Disable Event Mask Command */
	length = sizeof(cmd_disable_event_mask) / sizeof(char);
	if (length + offset <= size) {
		memcpy(ptr + offset, cmd_disable_event_mask, length);
		diag_send_data(ptr, offset + length);
	}
}


/*  signal handler to flush logs to disk */
volatile int flush_log;
volatile int in_read;
volatile int flush_in_progress;

/* Global variable to decide the type of PROC */
int proc_type = MSM;

/*==========================================================================
 * FUNCTION   dummy_handler
 *
 * DESCRIPTION
 *   A dummy signal handler for read thraed only.
 * RETURN VALUE
 *  None
 *
 *  SIDE EFFECTS
 *    None
 *
 *========================================================================*/
void dummy_handler(int signal)
{
	int sys_getpid, sys_gettid;
	(void)signal;

#ifdef FEATURE_LE_DIAG
	sys_getpid = SYS_getpid;
	sys_gettid = SYS_gettid;
#else
	sys_getpid = __NR_getpid;
	sys_gettid = __NR_gettid;
#endif
	 DIAG_LOGE("diag:%s: pid-tid %ld-%ld \n", __func__,
			 (long int)syscall(sys_getpid),
			 (long int)syscall(sys_gettid));
}

/*==========================================================================
FUNCTION   flush_buffer

DESCRIPTION
  This function flushes the pending data in the buffers.
  If application receives signal then it also stops the application.
RETURN VALUE
  None

SIDE EFFECTS
  None

===========================================================================*/
void flush_buffer(int signal)
{
	int status = 0;
	struct stat pid_stat;
	int success = 0;
	uint16 remote_mask = 0;
	int buf_mode = MODE_REALTIME;
	char temp_xml_buf[GUID_LIST_END_XML_TAG_SIZE];
	int index = 0;
	int ret = 0;
	DIAG_LOGE("diag: Signal received\n");

	/* Dismiss the signal if we are already processing a signal */
	if (signal) {
		if (flush_in_progress)
			return;
		flush_in_progress = 1;
	}

	status = diag_get_real_time_status(&buf_mode);
	if (status) {
		DIAG_LOGE("diag: In %s, error in querying for real time mode status: %d, errno: %d\n",
			  __func__, status, errno);
		buf_mode = MODE_REALTIME;
	}
	if (buf_mode == MODE_UNKNOWN)
		DIAG_LOGE("diag: One of the peripherals is in buffering mode.\n");

	/*
	 * If we are going to exit and we are logging in non-real-time mode
	 * then let the kernel know it should move from nont-real-time mode
	 * to real-time mode. Then wait for a few seconds to allow diag on the
	 * peripherals to change to real-time logging mode and flush their
	 * buffers over the smd. This data will then be read from the kernel
	 * and placed into the this libraries buffers for later writing. Wait
	 * for a few seconds before progressing with the exit to allow this
	 * to take place.
	 */
	if (signal && buf_mode == MODE_NONREALTIME) {
		int num_secs = 5;
		errno = 0;
		status = diag_vote_md_real_time(MODE_REALTIME);
		if (status == -1)
			DIAG_LOGE("diag: In %s unable to set mode to real time mode. errno = %d\n", __func__, errno);

		DIAG_LOGE("diag: In %s, Waiting for %d seconds for non-real-time data to arrive\n",
					__func__, num_secs);
		sleep(num_secs);
	}

	if (signal && cleanup_mask) {
		DIAG_LOGE("diag: Sending empty mask MSM\n");
		send_empty_mask(MSM);
		success = diag_has_remote_device(&remote_mask);
		if (success == 1) {
			if (remote_mask & MDM) {
				DIAG_LOGE("diag: Sending empty mask MDM\n");
				send_empty_mask(MDM);
			}
			if (remote_mask & QSC) {
				DIAG_LOGE("diag: Sending empty mask QSC\n");
				send_empty_mask(QSC);
			}
		} else {
			DIAG_LOGE("diag: error in getting remote processor mask, err: %d, errno: %d\n", success, errno);
		}
	}
	diag_kill_qshrink4_threads();
	flush_log = 1;

	if (in_read) {
		DIAG_LOGE("diag: sending signal to unblock read thread\n");
		pthread_kill(read_thread_hdl, SIGUSR2);
	}

	while(flush_log < 3) {
		sleep(1);
	}
	pthread_mutex_lock(&stop_mutex);

	/* Clean up */
	write_in_progress = 0;
	in_read = 0;
	curr_write = curr_read = 0;

	pools[0].free =  pools[1].free = 1;
	pools[0].data_ready = pools[1].data_ready = 0;

	/* Signal received destroy the mutexes and stop the application */
	if (signal) {
		DIAG_LOGE("diag: Sending cond to CreateWaitThread\n");
		pthread_cond_signal(&stop_cond);
		pthread_mutex_unlock(&stop_mutex);

		success = Diag_LSM_DeInit();
		if (!success)
			DIAG_LOGE("diag: DIAG_LSM_DeInit() Failed\n");

		pthread_mutex_destroy(&(pools[0].write_mutex));
		pthread_cond_destroy(&(pools[0].write_cond));
		pthread_mutex_destroy(&(pools[0].read_mutex));
		pthread_cond_destroy(&(pools[0].read_cond));
		pthread_mutex_destroy(&(pools[1].write_mutex));
		pthread_cond_destroy(&(pools[1].write_cond));
		pthread_mutex_destroy(&(pools[1].read_mutex));
		pthread_cond_destroy(&(pools[1].read_cond));
		if (!hdlc_disabled) {
			ret = std_strlprintf(temp_xml_buf, GUID_LIST_END_XML_TAG_SIZE, "%s", "</guidlist>");
			for (index = 0; index < NUM_PROC; index++) {
				if ((fd_qsr4_xml[index] >= 0) && (ret > 0)) {
					status = write(fd_qsr4_xml[index], temp_xml_buf, ret);
					if (status != ret)
						DIAG_LOGE("diag:In %s failed to write to xml file with err %d\n", __func__, errno);
					status = close(fd_qsr4_xml[index]);
					if (status != 0)
						DIAG_LOGE("diag:In %s failed to close xml file with err %d\n", __func__, errno);
					fd_qsr4_xml[index] = -1;
				}
			}
		}

		/* Release the wakelock if held */
		if (diag_is_wakelock_init()) {
			diag_wakelock_release();
			diag_wakelock_destroy();
		}

		/* Delete the PID file */
		if (stat(pid_file, &pid_stat) == 0)
			unlink(pid_file);
		else
			DIAG_LOGE("diag: Unable to delete the PID file: %s, err: %d\n", pid_file, errno);

		DIAG_LOGE("diag: Exiting program \n");
		_exit(0);
	}
	else
		pthread_mutex_unlock(&stop_mutex);

}


int valid_token(int token)
{
	int i;

	if (token >= 0)
		return 0;
	for (i = 0; i < NUM_PROC; i++)
		if(token == token_list[i])
			return -token;
	return 0;
}

/*===========================================================================
FUNCTION  fill_pkt_buffer

DESCRIPTION
  This function creates a single byte stream of logs by removing header
  information.
DEPENDENCIES
  valid data type to be passed in

RETURN VALUE
  TRUE - if pkt is filled correctly
  FALSE - pkt is not filled correctly due to invalid token

SIDE EFFECTS
  None

===========================================================================*/
static boolean fill_pkt_buffer(unsigned char *ptr)
{
	int num_data_fields = *(int *)ptr;
	int i, index = 0, z;
	uint32 count_received_bytes, copy_bytes = 0;
	int status = 0;
	char temp_xml_buf[GUID_LIST_END_XML_TAG_SIZE];
	int ret = 0;
	unsigned char *buffer = NULL;
	unsigned int *bytes_in_buff = NULL;

	ptr += 4;
	for (i = 0; i < num_data_fields; i++) {
		index = valid_token(*(int *)ptr);
		if (index == -1) {
			DIAG_LOGE("diag: In %s, invalid Token number %d\n", __func__, *(int *)ptr);
			return FALSE;
		}
		buffer = pools[curr_read].buffer_ptr[index];
		bytes_in_buff = &pools[curr_read].bytes_in_buff[index];
		if (!buffer || !bytes_in_buff)
			continue;
		if (index > 0) {
			ptr += 4;
			if (buffer_init[index] != READY) {

				/* This check is added as calloc is failing */
				if (index > 2) {
					/* Allocate buffer */
					pool0_buffers[index] =
						calloc(DISK_BUF_SIZE, 1);
					if (!pool0_buffers[index]) {
						DIAG_LOGE("\nbuffer alloc failed \n");
						continue;
					}
					pool1_buffers[index] =
						calloc(DISK_BUF_SIZE, 1);
					if (!pool1_buffers[index]) {
						DIAG_LOGE("\nbuffer alloc failed \n");
						free(pool0_buffers[index]);
						continue;
					}
					pools[0].buffer_ptr[index] =
						pool0_buffers[index];
					pools[1].buffer_ptr[index] =
						pool1_buffers[index];
					pools[0].bytes_in_buff[index] = 0;
					pools[1].bytes_in_buff[index] = 0;
				}

				/* Create directory for this proc */
				(void)strlcat(output_dir[index],
					proc_name[index], FILE_NAME_LEN);
				status = mkdir(output_dir[index], 0770);
				if (status == -1) {
					if (errno == EEXIST) {
						DIAG_LOGE("diag: In %s, Warning output directory already exists: %s\n",
							__func__, output_dir[index]);
						DIAG_LOGE("diag: Proceeding...\n");
					} else {
						DIAG_LOGE("diag: In %s, Unable to create directory: %s, errno: %d\n",
							__func__, output_dir[index], errno);
						continue;
					}
				}
				if (!hdlc_disabled) {
					if (fd_qsr4_xml[index] < 0) {
						(void)std_strlprintf(qsr4_xml_file_name,
								     FILE_NAME_LEN, "%s%s%s%s",
								     output_dir[index],"/diag_qsr4_guid_list_",
								     (char *)buffer, ".xml");
						fd_qsr4_xml[index] =  open(qsr4_xml_file_name, O_CREAT | O_RDWR | O_SYNC | O_TRUNC, 0644);
						if (fd_qsr4_xml[index] < 0)
							DIAG_LOGE("diag: In %s failed to create xml file with err %d", __func__, errno);
						ret = std_strlprintf(temp_xml_buf, GUID_LIST_XML_TAG_SIZE, "%s\n", "<guidlist>");
						if ((fd_qsr4_xml[index] >= 0) && (ret > 0)) {
							status = write(fd_qsr4_xml[index], temp_xml_buf, ret);
							if (status != ret)
								DIAG_LOGE("diag: In %s failed to write to xml file with err %d", __func__, errno);
						}
					}

				}
				buffer_init[index] = READY;
				buffer = pools[curr_read].buffer_ptr[index];
				*bytes_in_buff =
					pools[curr_read].bytes_in_buff[index];
			}
		}

		count_received_bytes = *(uint32*)ptr;
		ptr += sizeof(uint32);
		while (count_received_bytes >= (DISK_BUF_SIZE - *bytes_in_buff)) {
			copy_bytes = DISK_BUF_SIZE - *bytes_in_buff;
			if (parser_state)
				parse_data_for_qsr4_db_file_op_rsp(ptr, count_received_bytes);
			memcpy(buffer + *bytes_in_buff, ptr, copy_bytes);
			count_received_bytes -= copy_bytes;
			pools[curr_read].data_ready = 1;
			pools[curr_read].free = 0;
			ptr += copy_bytes;
			*bytes_in_buff += copy_bytes;
			for (z = 0; z < NUM_PROC; z++) {
				if (curr_read)
					pools[curr_read].buffer_ptr[z] =
							pool1_buffers[z];
				else
					pools[curr_read].buffer_ptr[z] =
							pool0_buffers[z];
			}
			pthread_cond_signal(&pools[curr_read].write_cond);
			pthread_mutex_unlock(&(pools[curr_read].write_mutex));
			curr_read = !curr_read;
			pthread_mutex_lock(&(pools[curr_read].read_mutex));
			if (!pools[curr_read].free) {
				pthread_mutex_unlock(&(pools[curr_read].write_mutex));
				pthread_cond_wait(
					&(pools[curr_read].read_cond),
					&(pools[curr_read].read_mutex));
				pthread_mutex_lock(&(pools[curr_read].write_mutex));
			}
			pthread_mutex_unlock(&(pools[curr_read].read_mutex));
			buffer = pools[curr_read].buffer_ptr[index];
			bytes_in_buff =
					&pools[curr_read].bytes_in_buff[index];
		}
		if (count_received_bytes > 0) {
			if (parser_state)
				parse_data_for_qsr4_db_file_op_rsp(ptr, count_received_bytes);

			memcpy(buffer + *bytes_in_buff, ptr, count_received_bytes);
			*bytes_in_buff += count_received_bytes;
			ptr += count_received_bytes;
		}
	}
	return TRUE;
}

/*===========================================================================
FUNCTION   buffer_full

DESCRIPTION
	This function check if buffer flush is required.

DEPENDENCIES
	valid data type to be passed in

RETURN VALUE
	1: buffer flush needed.
	0: no buffer flush.

SIDE EFFECTS
	None

===========================================================================*/
static int buffer_full(unsigned int bytes_in_buff[])
{
	int i;

	for (i = 0; i < NUM_PROC; i++)
		if (buffer_init[i] == READY)
			if (bytes_in_buff[i] >= DISK_FLUSH_THRESHOLD)
				return 1;

	return 0;
}

/*===========================================================================
FUNCTION   process_ext_header_packet

DESCRIPTION
	This function handles parsing an extended header packet.

DEPENDENCIES
	valid data type to be passed in

RETURN VALUE
	Bytes read from read_buffer in this function

SIDE EFFECTS
	None

===========================================================================*/
static int process_ext_header_packet(int dci_proc, unsigned char* read_buffer) {

	//check version of dci application
	int dci_version = dci_client_tbl[dci_proc].version;
	int read_bytes = 0, len = 0;
	uint32 pkt_cmd_type;
	void (*callback_ptr)(unsigned char *, int) = NULL;
	unsigned char* buf_ptr = NULL;

	if (!read_buffer)
		return 0;

	/* Move read_buffer up to account for DCI HEADER for ext header*/
	read_bytes += DCI_HEADER_LENGTH;
	read_buffer += DCI_HEADER_LENGTH;

	/* Default buf_ptr to start of extended header packet */
	buf_ptr = read_buffer;
	read_buffer += DCI_EXT_HDR_LENGTH;

	/* Read type of enclose packet */
	pkt_cmd_type = *(uint32 *)read_buffer;
	read_buffer += DCI_HEADER_LENGTH;

	/* Read length of enclosed packet */
	len  =  *(uint16 *)read_buffer;

	if (dci_version > 0) {
		/* Client supports ext header packets
		 */
		len += DCI_EXT_HDR_LENGTH + DCI_HEADER_LENGTH;
	} else {
		/* Client does not support ext header packets
		 * strip ext header
		 */
		read_bytes += DCI_EXT_HDR_LENGTH + DCI_HEADER_LENGTH;
		buf_ptr = read_buffer;
	}

	switch (pkt_cmd_type) {
	case DCI_LOG_TYPE:
		callback_ptr = dci_client_tbl[dci_proc].func_ptr_logs;
		break;
	case DCI_EVENT_TYPE:
		len += DCI_EVENT_OFFSET;
		callback_ptr = dci_client_tbl[dci_proc].func_ptr_events;
		break;
	default:
		DIAG_LOGE("diag: dci: received invalid packet type %d in extended packet\n",
				pkt_cmd_type);
		break;
	}
	read_bytes += len;

	if (!callback_ptr) {
		DIAG_LOGE("diag: dci: no callback function registered for packet type %d\n",
				pkt_cmd_type);
		return 0;
	}

	(*callback_ptr)(buf_ptr, len);
	return read_bytes;
}

/*===========================================================================
FUNCTION   process_diag_payload

DESCRIPTION
  This looks at the type of data being passed and then calls
  the appropriate function for processing request.

DEPENDENCIES
  valid data type to be passed in

RETURN VALUE
  None

SIDE EFFECTS
  None

===========================================================================*/
static void process_diag_payload(void)
{
	int type = *(int *)read_buffer, read_bytes = 0, z;
	int dci_data_len, dci_proc = DIAG_PROC_MSM;
	int ret, payload_len = 0;
	unsigned char* ptr = read_buffer+4;
	boolean result = FALSE;

	if (num_bytes_read < (int)sizeof(type))
		return;
	payload_len = num_bytes_read - sizeof(type); /* Subtract size of the data type */

	if(type == MSG_MASKS_TYPE) {
		msg_update_mask(ptr, payload_len);
		update_sync_mask(MSG_MASKS_TYPE);
	} else if(type == LOG_MASKS_TYPE) {
		log_update_mask(ptr, payload_len);
		update_sync_mask(LOG_MASKS_TYPE);
	} else if(type == EVENT_MASKS_TYPE) {
		event_update_mask(ptr, payload_len);
		update_sync_mask(EVENT_MASKS_TYPE);
	} else if(type == DCI_LOG_MASKS_TYPE)
		log_update_dci_mask(ptr, payload_len);
	else if(type == DCI_EVENT_MASKS_TYPE)
		event_update_dci_mask(ptr, payload_len);
	else if (type == PKT_TYPE || type == DCI_PKT_TYPE) {
		diagpkt_LSM_process_request((void*)ptr, (uint16)payload_len, type);
	} else if (type == HDLC_SUPPORT_TYPE) {
		pthread_mutex_lock(&hdlc_toggle_mutex);
		hdlc_disabled = *(uint8*)ptr;
		pthread_mutex_unlock(&hdlc_toggle_mutex);
	}
	else if(type == USER_SPACE_DATA_TYPE){
		if (logging_mode != MEMORY_DEVICE_MODE)
			log_to_device(ptr, logging_mode, payload_len, -1);
		else {
			pthread_mutex_lock(&(pools[curr_read].write_mutex));

			/* Wait if no buffer is free */
			pthread_mutex_lock(&(pools[curr_read].read_mutex));
			if (!pools[curr_read].free) {
				pthread_mutex_unlock(&(pools[curr_read].write_mutex));
				pthread_cond_wait(
					&(pools[curr_read].read_cond),
					&(pools[curr_read].read_mutex));
				pthread_mutex_lock(&(pools[curr_read].write_mutex));
			}
			pthread_mutex_unlock(&(pools[curr_read].read_mutex));

			/* One buffer got free continue writing */
			result = fill_pkt_buffer(ptr);
			if (result == FALSE) {
				pthread_mutex_unlock(&pools[curr_read].write_mutex);
				return;
			}

			pthread_mutex_unlock(&pools[curr_read].write_mutex);
		}
	}
	else if(type == DCI_DATA_TYPE) {
		if (!dci_client_tbl)
			return;

		dci_proc = *(int *)ptr;
		if (!IS_VALID_DCI_PROC(dci_proc))
			return;
		ptr += sizeof(int);
		dci_data_len = *(int *)ptr;
		ptr += DCI_LEN_FIELD_LENGTH;
		while (read_bytes < dci_data_len) {
			if (dci_client_tbl->data_signal_flag == ENABLE)
				if (raise(dci_client_tbl->data_signal_type))
					DIAG_LOGE("diag: dci: signal sending failed");
			if (*(int *)ptr == DCI_PKT_RSP_TYPE) {
				ptr += DCI_HEADER_LENGTH;
				read_bytes += DCI_HEADER_LENGTH;
				lookup_pkt_rsp_transaction(ptr, dci_proc);
				read_bytes += DCI_LEN_FIELD_LENGTH + DCI_DEL_FLAG_LEN + *(int *)(ptr);
				ptr += DCI_LEN_FIELD_LENGTH + DCI_DEL_FLAG_LEN + *(int *)(ptr);
			} else if (*(int *)ptr == DCI_LOG_TYPE) {
				ptr += DCI_HEADER_LENGTH;
				read_bytes += DCI_HEADER_LENGTH;
				if (dci_client_tbl[dci_proc].func_ptr_logs)
					(*(dci_client_tbl[dci_proc].func_ptr_logs))(ptr, *(uint16 *)(ptr));
				else
					DIAG_LOGE("diag: dci: no callback function registered for received log stream\n");
				read_bytes += *(uint16 *)(ptr);
				ptr += *(uint16 *)(ptr);
			} else if (*(int *)ptr == DCI_EVENT_TYPE) {
				ptr += DCI_HEADER_LENGTH;
				read_bytes += DCI_HEADER_LENGTH;
				if (dci_client_tbl[dci_proc].func_ptr_events)
					(*(dci_client_tbl[dci_proc].func_ptr_events))(ptr, DCI_EVENT_OFFSET + *(uint16 *)(ptr));
				else
					DIAG_LOGE("diag: dci: no callback function registered for received event stream\n");
				read_bytes += DCI_EVENT_OFFSET + *(uint16 *)(ptr);
				ptr += DCI_EVENT_OFFSET + *(uint16 *)(ptr);
			} else if (*(int *)ptr == DCI_EXT_HDR_TYPE) {
				ret = process_ext_header_packet(dci_proc, ptr);
				read_bytes += ret;
				ptr += ret;
			} else {
				DIAG_LOGE("diag: dci: unknown log type %d\n", *(int *)ptr);
				break;
			}
		}
	}
}

static void get_time_string(char *buffer, int len)
{
	struct timeval tv;
	struct tm *tm;
	unsigned long long milliseconds = 0;
	char timestamp_buf[30];

	if (!buffer || len <= 0)
		return;

	gettimeofday(&tv, NULL);
	tm = localtime(&tv.tv_sec);

	if (!tm)
		return;

	milliseconds = (tv.tv_sec * 1000LL) + (tv.tv_usec / 1000);
	strftime(timestamp_buf, 30, "%Y%m%d_%H%M%S", tm);

	(void)std_strlprintf(buffer, len, "%s%lld",
		timestamp_buf, milliseconds);
}

static void rename_logging_directory(void)
{
	char timestamp_buf[30];
	char new_dirname[FILE_NAME_LEN];
	char rename_cmd[RENAME_CMD_LEN];
	int replace_pos = -1;
	int index = 0;
	int len = 0;
	int status = 0;
	int i;

	if (!rename_dir_name)
		return;

	/* Loop backwards through the MSM directory name to find the date/time sub-directory */
	len = strlen(output_dir[0]);
	index = len - 1;
	for (i = 0; i < len, index >= 0; i++) {
		if (output_dir[0][index] == '/') {
			if (index != len - 1) {
				replace_pos = index + 1;
				break;
			}
		} else if ((output_dir[0][index] != '_') &&
				(output_dir[0][index] < '0') &&
				(output_dir[0][index] > '9')) {
			/* The last subdirectory is not in date/time format,
			 * do not rename the directory */
			replace_pos = -1;
			break;
		}
		index--;
	}

	if (replace_pos < 0) {
		DIAG_LOGE("diag: In %s, Not able to rename directory, invalid directory format, dir: %s\n",
			__func__, output_dir[0]);
		return;
	}

	get_time_string(timestamp_buf, sizeof(timestamp_buf));

	strlcpy(new_dirname, output_dir[0], FILE_NAME_LEN);
	new_dirname[replace_pos] = '\0';
	strlcat(new_dirname, timestamp_buf, FILE_NAME_LEN);

	/* Create rename command and issue it */
	(void)std_strlprintf(rename_cmd, RENAME_CMD_LEN, "mv %s %s",
			output_dir[0], new_dirname);

	status = system(rename_cmd);
	if (status == -1) {
		DIAG_LOGE("diag: In %s, Directory rename error (mv), errno: %d\n",
			__func__, errno);
		DIAG_LOGE("diag: Unable to rename directory %s to %s\n",
			output_dir[0], new_dirname);
	} else {
		/* Update current directory names */
		for (i = 0; i < NUM_PROC; i++)
			strlcpy(output_dir[i], new_dirname, FILE_NAME_LEN);
		DIAG_LOGE("diag: Renamed logging directory to: %s\n",
				output_dir[0]);
	}
}

void diag_get_peripheral_name_from_mask(char *buf,
					unsigned int len,
					unsigned int peripheral_mask)
{

	if (!buf || !len)
		return;

	if (peripheral_mask & DIAG_CON_APSS) {
		strlcat(buf, "_apps", len);
		peripheral_mask  = peripheral_mask ^ DIAG_CON_APSS;
	}
	if (peripheral_mask & DIAG_CON_MPSS) {
		strlcat(buf, "_mpss", len);
		peripheral_mask  = peripheral_mask ^ DIAG_CON_MPSS;
	}
	if (peripheral_mask & DIAG_CON_LPASS) {
		strlcat(buf, "_adsp", len);
		peripheral_mask  = peripheral_mask ^ DIAG_CON_LPASS;
	}
	if (peripheral_mask & DIAG_CON_WCNSS) {
		strlcat(buf, "_wcnss", len);
		peripheral_mask  = peripheral_mask ^ DIAG_CON_WCNSS;
	}
	if (peripheral_mask & DIAG_CON_SENSORS) {
		strlcat(buf, "_slpi", len);
		peripheral_mask = peripheral_mask ^ DIAG_CON_SENSORS;
	}

	if (peripheral_mask > 0)
		DIAG_LOGE("diag: Invalid peripheral mask set %d", peripheral_mask);

}

static void close_logging_file(int type)
{
	close(fd_md[type]);
	fd_md[type] = -1;

	if (rename_file_names && file_name_curr[type][0] != '\0') {
		int status;
		char timestamp_buf[30];
		char new_filename[FILE_NAME_LEN];
		char rename_cmd[RENAME_CMD_LEN];

		get_time_string(timestamp_buf, sizeof(timestamp_buf));

		if (!hdlc_disabled) {
			if (diag_peripheral_mask)
				(void)std_strlprintf(new_filename,
					FILE_NAME_LEN, "%s%s%s%s%s%s",
					output_dir[type], "/diag_log",
					peripheral_name, "_",
					timestamp_buf, ".qmdl");
			else
				(void)std_strlprintf(new_filename,
					FILE_NAME_LEN, "%s%s%s%s",
					output_dir[type], "/diag_log_",
					timestamp_buf, ".qmdl");
		}
		else {
			if (diag_peripheral_mask)
				(void)std_strlprintf(new_filename,
					FILE_NAME_LEN, "%s%s%s%s%s%s",
					output_dir[type], "/diag_log",
					peripheral_name, "_", timestamp_buf, ".qmdl2");
			else
				(void)std_strlprintf(new_filename,
					FILE_NAME_LEN, "%s%s%s%s",
					output_dir[type],"/diag_log_",
					timestamp_buf, ".qmdl2");
		}
		/* Create rename command and issue it */
		(void)std_strlprintf(rename_cmd, RENAME_CMD_LEN, "mv %s %s",
				file_name_curr[type], new_filename);

		status = system(rename_cmd);
		if (status == -1) {
			DIAG_LOGE("diag: In %s, File rename error (mv), errno: %d\n",
				__func__, errno);
			DIAG_LOGE("diag: Unable to rename file %s to %s\n",
				file_name_curr[type], new_filename);
		} else {
			/* Update current filename */
			strlcpy(file_name_curr[type], new_filename, FILE_NAME_LEN);
		}
	}
}

#define S_64K (64*1024)
void *WriteToDisk (void *ptr)
{
	unsigned int i;
	int z;
	unsigned int chunks, last_chunk;
	(void) ptr;

	while (1) {
		pthread_mutex_lock(&(pools[curr_write].write_mutex));
		if (!pools[curr_write].data_ready){
			pthread_cond_wait(&(pools[curr_write].write_cond),
						&(pools[curr_write].write_mutex));
		}
		write_in_progress = 1;

		for (z = 0; z < NUM_PROC; z++) {
			if (buffer_init[z] == READY) {
				chunks = pools[curr_write].bytes_in_buff[z] /
									S_64K;
				last_chunk =
					pools[curr_write].bytes_in_buff[z] %
									S_64K;
				for(i = 0; i < chunks; i++){
					log_to_device(
					pools[curr_write].buffer_ptr[z],
					MEMORY_DEVICE_MODE, S_64K, z);
					pools[curr_write].buffer_ptr[z] +=
									S_64K;
					}
				if( last_chunk > 0)
					log_to_device(
					pools[curr_write].buffer_ptr[z],
					MEMORY_DEVICE_MODE, last_chunk, z);
				if ((logging_mode == MEMORY_DEVICE_MODE) &&
					(count_written_bytes[z] >= max_file_size)) {
					close_logging_file(z);
					fd_dev = fd_md[z];
					count_written_bytes[z] = 0;
				}
			}
		}

		write_in_progress = 0;

		/* File pool structure */
		pools[curr_write].free = 1;
		pools[curr_write].data_ready = 0;

		for (z = 0; z < NUM_PROC; z++) {
			if (buffer_init[z] == READY) {
				pools[curr_write].bytes_in_buff[z] = 0;
				if(curr_write)
					pools[curr_write].buffer_ptr[z] =
							pool1_buffers[z];
				else
					pools[curr_write].buffer_ptr[z] =
							pool0_buffers[z];
			}
		}

		if ( flush_log == 2){
			for (z = 0; z < NUM_PROC; z++) {
				if (buffer_init[z] == READY) {
					close_logging_file(z);
				}
			}

			if (rename_dir_name) {
				rename_logging_directory();
			}

			pthread_mutex_unlock(
				&(pools[curr_write].write_mutex));
			DIAG_LOGE(" Exiting....%s \n", __func__);
			pthread_mutex_lock(&stop_mutex);
			flush_log++;
			pthread_mutex_unlock(&stop_mutex);
			pthread_exit(NULL);
		}
		/* Free Read thread if waiting on same buffer */
		pthread_mutex_lock(&(pools[curr_write].read_mutex));
		pthread_cond_signal(&(pools[curr_write].read_cond));
		pthread_mutex_unlock(&(pools[curr_write].read_mutex));
		pthread_mutex_unlock(&(pools[curr_write].write_mutex));

		curr_write = !curr_write;
	}
	return NULL;
}

static void delete_oldest_file_list(int type)
{
	if (type >= 0 && type < NUM_PROC) {
		if (file_list[type])
			free(file_list[type]);
		file_list[type] = NULL;
		file_list_index[type] = -1;
		file_list_size[type] = 0;
	}
}

#define LOG_FILENAME_PREFIX_LEN 9

static int create_oldest_file_list(char *oldest_dir, int type)
{
	struct dirent **dirent_list = NULL;
	int num_entries = 0;
	int num_entries_capped = 0;
	char *name_ptr;
	int i;
	int status = 1;
	int num_bytes = 0;

	if (type < 0 || type >= NUM_PROC) {
		DIAG_LOGE("diag: In %s, Invalid type: %d, for directory: %s\n",
			__func__, type, oldest_dir);
		return 0;
	}

	/* If we need to find what files are in the directory */
	if (NULL == file_list[type]) {
		DIAG_LOGE("diag: Determining contents of directory %s for circular logging ...\n",
			oldest_dir);
		num_entries = scandir(oldest_dir, &dirent_list, 0,
			(int(*)(const struct dirent **, const struct dirent **))alphasort);
		if(!dirent_list) {
			DIAG_LOGE("diag: In %s, couldn't get the dirent_list, errno: %d, directory: %s\n",
				__func__, errno, oldest_dir);
			return 0;
		} else if (num_entries < 0) {
			DIAG_LOGE("diag: In %s, error determining directory entries, errno: %d, directory: %s\n",
				__func__, errno, oldest_dir);
			return 0;
		}

		/* Limit the size of the list so we aren't working with too many files */
		num_entries_capped = (num_entries > MAX_FILES_IN_FILE_LIST) ?
					MAX_FILES_IN_FILE_LIST : num_entries;

		/* We don't need the files "." and ".." */
		if (num_entries_capped - 2 > 0) {
			file_list_size[type] = num_entries_capped - 2;

			num_bytes = FILE_LIST_NAME_SIZE * file_list_size[type];
			file_list[type] = malloc(num_bytes);
		}

		/* If the directory contains some files */
		if (file_list[type]) {
			file_list_index[type] = 0;

			/* Copy the file names into our list */
			for (i = 0; i < num_entries_capped; i++)
			{
				/* Don't copy any file names that are not logging files */
				if (strncmp(dirent_list[i]->d_name, "diag_log_",
					LOG_FILENAME_PREFIX_LEN) != 0)
					continue;

				if (file_list_index[type] < file_list_size[type]) {
					name_ptr = file_list[type] +
						(file_list_index[type] * FILE_LIST_NAME_SIZE);
					strlcpy(name_ptr, dirent_list[i]->d_name, FILE_LIST_NAME_SIZE);
					*(name_ptr + (FILE_LIST_NAME_SIZE - 1)) = 0;
					file_list_index[type]++;
				}
			}
			if (file_list_index[type] > 0) {
				if (file_list_index[type] < file_list_size[type]) {
					/* There are files in the directory that are
					 * not logging files. Reduce the memory size
					 * allocated to the list */
					int new_size = FILE_LIST_NAME_SIZE *file_list_index[type];
					char *temp_ptr = realloc(file_list[type], new_size);
					if (temp_ptr)
						file_list[type] = temp_ptr;
				}
				file_list_size[type] = file_list_index[type];
				file_list_index[type] = 0;
			} else {
				/* There were no logging files in the directory. Clean up */
				delete_oldest_file_list(type);
			}
		} else if (num_bytes > 0) {
			DIAG_LOGE("diag: In %s, memory allocation error for directory: %s, type: %d\n",
				__func__, oldest_dir, type);
			status = 0;
		}

		/* Deallocate directory entry list */
		i = num_entries;
		while (i--) {
			free(dirent_list[i]);
		}
		free(dirent_list);
	}

	return status;
}

static int get_oldest_file(char* oldest_file, char *oldest_dir, int type)
{
	int status = 0;

	if (type < 0 || type >= NUM_PROC) {
		DIAG_LOGE("diag: In %s, Invalid type: %d, for directory: %s\n",
			__func__, type, oldest_dir);
		return 0;
	}

	/* If we need to find what files are in the directory */
	if (NULL == file_list[type])
		status = create_oldest_file_list(oldest_dir, type);

	if (file_list[type]) {
		if (oldest_file) {
			strlcpy(oldest_file, (file_list[type] +
				(file_list_index[type] * FILE_LIST_NAME_SIZE)),
				FILE_LIST_NAME_SIZE);
			file_list_index[type]++;
			/* If we have exhausted the list */
			if (file_list_index[type] >= file_list_size[type]) {
				/* Deallocate the file list and set up for determining
				 * directory entries on next call */
				delete_oldest_file_list(type);
			}
			status = 1;
		} else {
			DIAG_LOGE("diag: In %s, oldest_file is NULL\n", __func__);
		}
	} else {
		status = 0;
		if (file_list_size[type] == 0) {
			/* The directory does not have any logging files in it,
			 * so we must return an error status since we cannot return
			 * an oldest file name */
			DIAG_LOGE("diag: In %s, Directory %s contains no logging files\n",
			__func__, oldest_dir);
		} else {
			DIAG_LOGE("diag: In %s, Error determining directory file list for directory: %s, type: %d\n",
				__func__, oldest_dir, type);
		}
	}

	return status;
}

int delete_log(int type)
{
	int status;
	char oldest_file[FILE_LIST_NAME_SIZE] = "";
	struct stat file_stat = {};

	status = get_oldest_file(oldest_file,
				output_dir[type], type);
	if (0 == status) {
		DIAG_LOGE("diag: In %s, Unable to determine oldest file for deletion\n",
			__func__);
		return -1;
	}

	std_strlprintf(file_name_del,
			FILE_NAME_LEN, "%s%s%s",
			output_dir[type], "/", oldest_file);

	if (!strncmp(file_name_curr[type], file_name_del, FILE_NAME_LEN)) {
		DIAG_LOGE("diag: In %s, Cannot delete file, file %s is in use \n",
			__func__, file_name_curr[type]);
		return -1;
	}

	stat(file_name_del, &file_stat);

	/* Convert size to KB */
	file_stat.st_size /= 1024;

	if (unlink(file_name_del)) {
		DIAG_LOGE("diag: In %s, Unable to delete file: %s, errno: %d\n",
				__func__, file_name_del, errno);
		return -1;
	} else {
		DIAG_LOGE("diag: In %s, Deleting logfile %s of size %lld KB\n",
				__func__, file_name_del,
				(long long int) file_stat.st_size);
	}
	return 0;
}

static void get_circular_logging_info(int type)
{
	if (type < 0 || type >= NUM_PROC)
		return;

	if (!circular_logging_inited && !file_list[type]) {
		char dir_name[FILE_NAME_LEN];
		strlcpy(dir_name, output_dir[type], FILE_NAME_LEN);
		if (buffer_init[type] != READY) {
			/* The output directory name has not been fully constructed yet */
			(void)strlcat(dir_name, proc_name[type], FILE_NAME_LEN);
		}
		create_oldest_file_list(dir_name, type);
	}
}

static void init_circular_logging(void)
{
	int i;
	int num_lists = 0;

	file_count = 0;
	for (i = 0; i < NUM_PROC; i++) {
		if (file_list[i] && file_list_size[i] > 0) {
			file_count += file_list_size[i];
			num_lists++;
		}
	}

	/* If circular logging is enabled and there are too many logging files on the SD card */
	if(max_file_num > 1 && (file_count >= max_file_num)) {
		int status = -1;
		DIAG_LOGE("diag: Initializing circular logging, reducing number of pre-existing logging files, current: %d, max: %d\n",
			file_count, max_file_num);
		while ((file_count >= max_file_num) && (num_lists > 0)) {
			/* Iterate over the lists to reduce the logging files
			 * in each logging directory one by one */
			num_lists = 0;
			for (i = 0; i < NUM_PROC; i++) {
				if (file_list[i] && file_list_size[i] > 0) {
					status = delete_log(i);
					/* Break out if there was an error */
					if (status)
						break;

					file_count--;
					num_lists++;
				}
			}
		}

		if (status) {
			DIAG_LOGE("diag: In %s, Problem initializing circular logging, continuing ...\n",
				__func__);
		}
	}
}

/*
 * NOTE: Please always pass type as -1, type is only valid for mdlog.
 *
 */
void log_to_device(unsigned char *ptr, int logging_mode, int size, int type)
{
	uint32 count_received_bytes;
	int i, ret, rc, proc = MSM, z;
	int num_data_fields = 0;
	unsigned char *base_ptr = ptr;
	char timestamp_buf[30];
	int token;
	unsigned int bytes_read = 0;
	unsigned int bytes_remaining;
	unsigned char *sock_ptr;
	struct stat logging_file_stat;

	if (ptr == NULL || size <= 0) {
		DIAG_LOGE("diag: Invalid ptr in %s size is %d\n",__func__,size);
		return;
	}
	num_data_fields = *(int *)ptr;
	bytes_remaining = size;
	ptr += sizeof(int);
	bytes_read += sizeof(int);
	if (CALLBACK_MODE == logging_mode) {
		for (i = 0; i < num_data_fields && bytes_remaining > 0; i++) {
			token = valid_token(*(int *)ptr);
			if (token == -1) {
				DIAG_LOGE("diag: Invalid token in %s, CALLBACK MODE", __func__);
				return;
			}
			if (token > 0) {
				ptr += sizeof(int);
				bytes_read += sizeof(int);
			}
			count_received_bytes = *(uint32 *)ptr;
			ptr += sizeof(uint32);
			bytes_read += sizeof(uint32);
			if (!cb_clients[token].cb_func_ptr) {
				DIAG_LOGE("diag: In %s, no callback function registered for proc %d\n",
					  __func__, token);
				return;
			}

			if (bytes_remaining < (bytes_read + count_received_bytes)) {
				DIAG_LOGE("diag: In %s received packet of invalid length bytes remaining:%d bytes_read:%d count_received_bytes:%d\n",
					  __func__, bytes_remaining, bytes_read, count_received_bytes);
				return;
			}

			(*(cb_clients[token].cb_func_ptr))((void *) ptr,
							   count_received_bytes,
							   cb_clients[token].context_data);
			ptr += count_received_bytes;
			bytes_read += count_received_bytes;
			bytes_remaining -= bytes_read;
			bytes_read = 0;
		}
		return;
	}

	if (type >= 0 && (buffer_init[type] == READY))
		fd_dev = fd_md[type];
	else if (logging_mode == MEMORY_DEVICE_MODE) {
		DIAG_LOGE("\n Buffer not initialized %d\n", type);
		return;
	}

	if (fd_dev < 0) {
		if(logging_mode == MEMORY_DEVICE_MODE) {
			if (!circular_logging_inited) {
				init_circular_logging();
				circular_logging_inited = 1;
			}
			/* Check if we are to start circular logging on the basis
			 * of maximum number of logging files in the logging
			 * directories on the SD card */
			if(max_file_num > 1 && (file_count >= max_file_num)) {
				DIAG_LOGE("diag: In %s, File count reached max file num %d so deleting oldest file\n",
					__func__, file_count);
				rc = -1;
				for (z = 0; z < NUM_PROC; z++) {
					if (buffer_init[z] == READY)
						if (!delete_log(z)) {
							file_count--;
							rc = 0;
						}
				}

				if (rc) {
					DIAG_LOGE("Delete failed \n");
					return;
				}
			}

			/* Construct the file name using the current time stamp */
			get_time_string(timestamp_buf, sizeof(timestamp_buf));

			if (!hdlc_disabled) {
				if (diag_peripheral_mask)
					(void)std_strlprintf(file_name_curr[type],
							FILE_NAME_LEN, "%s%s%s%s%s%s",
							output_dir[type], "/diag_log",
							peripheral_name, "_", timestamp_buf, ".qmdl");
				else
					(void)std_strlprintf(file_name_curr[type],
							FILE_NAME_LEN, "%s%s%s%s",
							output_dir[type],"/diag_log_",
							timestamp_buf, ".qmdl");
			}
			else {
				if (diag_peripheral_mask)
					(void)std_strlprintf(file_name_curr[type],
							FILE_NAME_LEN, "%s%s%s%s%s%s",
							output_dir[type], "/diag_log",
							peripheral_name, "_", timestamp_buf, ".qmdl2");
				else
					(void)std_strlprintf(file_name_curr[type],
							FILE_NAME_LEN, "%s%s%s%s",
							output_dir[type],"/diag_log_",
							timestamp_buf, ".qmdl2");
			}

			fd_md[type] = open(file_name_curr[type],
					O_CREAT | O_RDWR | O_SYNC | O_TRUNC,
					0644);
			fd_dev = fd_md[type];
			if (fd_md[type] < 0) {
				DIAG_LOGE(" File open error, please check");
				DIAG_LOGE(" memory device %d, errno: %d \n",
							fd_md[type], errno);
			}
			else {
				DIAG_LOGE(" creating new file %s \n",
							file_name_curr[type]);
				file_count++;
				/*
				 * The qshrink4 header needs to be the first
				 * information written in the qmdl2 file
				 */
				 if (hdlc_disabled)
					write_qshrink_header = 1;
			}
		} else if ((logging_mode == UART_MODE) ||
					(logging_mode == SOCKET_MODE))
				DIAG_LOGE(" Invalid file descriptor\n");
	}
	if (fd_dev != -1) {
		if(logging_mode == MEMORY_DEVICE_MODE ){
			errno = 0;
			/*
			 * Write the header information for the QSHRINK4 in
			 * case for qmdl2 file.
			 */
			if (hdlc_disabled && write_qshrink_header) {
				write_qshrink_header = 0;
				ret = write(fd_dev, (const void*)&qshrink4_data,
						qshrink4_data.header_length);
				if (ret > 0)
					count_written_bytes[type] +=
							qshrink4_data.header_length;
			}
			if (!fstat(fd_dev, &logging_file_stat)) {
				ret = write(fd_dev, (const void*) base_ptr, size);
			} else {
				close(fd_md[type]);
				fd_md[type] = -1;
				ret = -EINVAL;
			}

			if ( ret > 0) {
				count_written_bytes[type] += size;
			}else {
				DIAG_LOGE("diag: In %s, error writing to sd card, %s, errno: %d\n",
					__func__, strerror(errno), errno);
				if (errno == ENOSPC) {
					/* Delete oldest file */
					rc = -1;
					for (z = 0; z < NUM_PROC; z++) {
						if (buffer_init[z] == READY)
							if (!delete_log(z))
								rc = 0;
					}

					if (rc) {
						DIAG_LOGE("Delete failed \n");
						return;
					}

					/* Close file if it is big enough */
					if(count_written_bytes[type] >
							min_file_size) {
						close_logging_file(type);
						fd_dev = fd_md[type];
						count_written_bytes[type] = 0;
					}else {
						DIAG_LOGE(" Disk Full "
							"Continuing with "
						"same file [%d] \n", type);
					}

					log_to_device(base_ptr,
						MEMORY_DEVICE_MODE, size,
						type);
					return;
				} else
					DIAG_LOGE(" failed to write "
						"to file, device may"
						" be absent, errno: %d\n",
						errno);
			}
		} else if (SOCKET_MODE == logging_mode) {
			int is_mdm;
			int token;
			for (i = 0; i < num_data_fields; i++) {
				if ((*(int *)ptr < 0)) {
					is_mdm = 1;
					token = *(int *)ptr;
					ptr += 4;
				} else {
					is_mdm = 0;
				}

				memcpy((char*)&count_received_bytes, ptr, sizeof(uint32));
				ptr += sizeof(uint32);
				sock_ptr = ptr;
				bytes_remaining = (int)count_received_bytes;

				if (is_mdm) {
					fd_dev = get_remote_socket_fd(token);
					if (fd_dev == -1) {
						/*
						 * There is currently no socket fd for this token.
						 * Dismiss this data by incrementing through the buffer
						 * to the next data field.
						 */
						ptr += count_received_bytes;
						continue;
					}
				} else {
					fd_dev = fd_socket[MSM];
				}

				if (fd_dev == -1 && device_num && *(char *)(ptr + 1) == 0x0B)
					fd_dev = fd_socket[device_num];

				while ((bytes_remaining > 0) && (fd_dev != -1)) {
					ret = send(fd_dev, (const void*)sock_ptr,
							bytes_remaining, MSG_NOSIGNAL);
					if (ret > 0) {
						/*
						 * There is a possibility that not all
						 * the data was written to the socket.
						 * We must continue sending data on
						 * the socket until all the data is
						 * written.
						 */
						bytes_remaining -= ret;
						count_written_bytes_1 += ret;
						sock_ptr += ret;
					} else {
						if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
							fd_set write_fs;
							int status;
							FD_ZERO(&write_fs);
							FD_SET(fd_dev, &write_fs);
							status = select(fd_dev+1, NULL, &write_fs, NULL, NULL);
							if (status == -1) {
								DIAG_LOGE("In %s: Error calling select for write, %s, errno: %d\n",
									__func__, strerror(errno), errno);
							} else if (!FD_ISSET(fd_dev, &write_fs)) {
								DIAG_LOGE("In %s: FD_ISSET is false after write select call\n",
									__func__);
							}
						} else {
							DIAG_LOGE("%s, Error writing to socket: %s, errno: %d, "
								"count_received_bytes: %u\n",
								__func__, strerror(errno), errno,
								(unsigned int)count_received_bytes);
							break;
						}
					}
				}
				ptr += count_received_bytes;
			}
			fd_dev = fd_socket[device_num];
		} else if (UART_MODE == logging_mode) {
			for (i = 0; i < num_data_fields; i++) {
				/* Check if data is from MDM Proc
				 * For QSC we can check -2 also and
				 * based on that set value of is_mdm
				 */
				proc = valid_token(*(int *)ptr);
				if (proc > 0)
					ptr += 4;
				else
					proc= MSM;

				memcpy((char*)&count_received_bytes,ptr,sizeof(uint32));
				ptr += sizeof(uint32);
				if (proc == uart_logging_proc) {
					ret = write(fd_dev, (const void*) ptr,
							count_received_bytes);
					if ( ret > 0) {
						count_written_bytes_1 +=
							count_received_bytes;
					} else {
						DIAG_LOGE("failed to write"
							"%u bytes to file,"
							"device may be"
							"absent,errno: %d \n",
						(unsigned int)count_received_bytes,
						errno);
					}
				}

				ptr += count_received_bytes;
			}
		} else
			DIAG_LOGE("diag: Incorrect logging mode\n");
	}
}
/*===========================================================================
FUNCTION   diag_read_mask_file

DESCRIPTION
  This reads the mask file

DEPENDENCIES
  valid data type to be passed in

RETURN VALUE
  None

SIDE EFFECTS
  None

===========================================================================*/

int diag_read_mask_file(void)
{
	FILE *read_mask_fp;
	char *open_mask_file = NULL;
	unsigned char mask_buf[8192];
	int count_mask_bytes = 0;
	int ch, index, i = 0;
	int payload = 0, flag = 0;
	int found_cmd = 0;

	*(int *)mask_buf = USER_SPACE_DATA_TYPE;
	count_mask_bytes = 4;

	index = valid_token(-1 * proc_type);
	if (!hdlc_disabled) {
		if (!index)
			open_mask_file = mask_file;
		else if (index > 0)
			open_mask_file = mask_file_mdm;
	} else {
		if (!index)
			open_mask_file = mask_file2;
		else if (index > 0)
			open_mask_file = mask_file_mdm2;
	}

	if (open_mask_file) {
		if ((read_mask_fp = fopen(open_mask_file, "rb")) == NULL) {
				DIAG_LOGE("Sorry, can't open mask file: %s,"
				" for index: %d, please check the device, errno: %d\n",
					open_mask_file, index, errno);
			return -1;
		}
	} else {
		DIAG_LOGE("Invalid proc type\n");
		return -1;
	}
	DIAG_LOGE("Reading the mask file: %s\n", open_mask_file);
	get_circular_logging_info(proc_type);

	if (!hdlc_disabled) {
		while (1) {
			ch = fgetc(read_mask_fp);
			if (ch == EOF)
				break;
			if ((index > 0) && (count_mask_bytes == 4)) {
				*(int *)(mask_buf + count_mask_bytes) =
								token_list[index];
				count_mask_bytes = 8;
			}
			mask_buf[count_mask_bytes] = ch;
			if (mask_buf[count_mask_bytes] == CONTROL_CHAR) {
#ifdef DIAG_DEBUG
				DIAG_LOGE("********************************** \n");
				for (i = 0; i <= count_mask_bytes; i++) {
					DIAG_LOGE("%2x ", mask_buf[i]);
				}
				DIAG_LOGE("********************************** \n");
#endif
				if (!found_cmd)
					found_cmd = 1;
				diag_send_data(mask_buf, count_mask_bytes+1);
				*(int *)mask_buf = USER_SPACE_DATA_TYPE;
				count_mask_bytes = 4;
			} else
				count_mask_bytes++;
		}
		if (!found_cmd) {
			DIAG_LOGE("Sorry, could not find valid commands in the mask file,"
					"please check the mask file again\n");
			return -1;
		}
	} else {
		while (1) {
			ch = fgetc(read_mask_fp);
			if (ch == EOF)
				break;
			if ((index > 0) && (count_mask_bytes == 4)) {
				*(int *)(mask_buf + count_mask_bytes) =
								token_list[index];
				count_mask_bytes = 8;
			}
			if (i > 3 && !flag) {
				/* Calculate the payload */
				payload = *(uint16 *)(mask_buf + count_mask_bytes - 2);
				flag = 1;
			}
			mask_buf[count_mask_bytes] = ch;
			if (mask_buf[count_mask_bytes] != CONTROL_CHAR && i == 0) {
				DIAG_LOGE("Sorry, the mask file doesn't adhere to framing definition,"
					"please check the mask file again\n");
				return -1;
			}
			if (count_mask_bytes > payload && mask_buf[count_mask_bytes] == CONTROL_CHAR && i != 0) {
#ifdef DIAG_DEBUG
			DIAG_LOGE("********************************** \n");
			for (i = 0; i <= count_mask_bytes; i++) {
				DIAG_LOGE("%2x ", mask_buf[i]);
			}
			DIAG_LOGE("********************************** \n");
#endif
				i = 0;
				flag = 0;
				diag_send_data(mask_buf, count_mask_bytes+1);
				*(int *)mask_buf = USER_SPACE_DATA_TYPE;
				count_mask_bytes = 4;
			} else {
				i++;
				count_mask_bytes++;
			}
		}
	}

	fclose(read_mask_fp);

	return 0;
}

int diag_read_mask_file_list(char *mask_list_file)
{
	FILE *list_fp;
	char line[FILE_NAME_LEN+8];
	char *end_ptr;
	char *file_name;
	long val;
	int num_files = 0;
	int print_help = 0;
	uint16 remote_mask = 0;
	uint16 mask;
	int val_is_valid;
	int status;

	DIAG_LOGE("Mask list file name is: %s\n", mask_list_file);
	list_fp = fopen(mask_list_file, "rb");
	if (list_fp == NULL) {
		DIAG_LOGE("Sorry, can't open mask list file,"
			"please check the device, errno: %d\n", errno);
		return 0;
	}

	diag_has_remote_device(&remote_mask);
	while (fgets(line, (FILE_NAME_LEN+8), list_fp) != NULL) {
		errno = 0;
		val_is_valid = 0;
		/* Discard if the line is a comment */
		if (line[0] == ';')
			continue;
		val = strtol(line, &end_ptr, 0);
		if (((errno == ERANGE) && (val == LONG_MAX || val == LONG_MIN)) ||
			(errno != 0 && val == 0)) {
				DIAG_LOGE("Skipping line. Invalid processor type found. line: %s\n", line);
				print_help = 1;
				continue;
		} else if (end_ptr == line) {
			print_help = 1;
			DIAG_LOGE("Skipping line. No processor type present. line: %s\n", line);
		}

		if (remote_mask) {
			if ((val > 0) && (val < NUM_PROC-1)) {
				mask = 1 << (val-1);
				if (mask & remote_mask) {
					val_is_valid = 1;
				} else {
					DIAG_LOGE("Skipping line. Remote processor: %ld is not present.\n", val);
					continue;
				}
			} else if (val == 0){
				val_is_valid = 1;
			} else {
				DIAG_LOGE("Skipping line. Invalid processor type: %ld specified. line; %s\n", val, line);
				print_help = 1;
				continue;
			}
		} else {
			if (val != 0) {
				DIAG_LOGE("Skipping line. No remote processors present. proc_type: %ld, line: %s\n", val, line);
				continue;
			}
		}
		/*
		 * Determine the name of the mask file. We are counting on the
		 * call to fopen in read_mask_file to do file name validation.
		 */
		file_name = end_ptr;
		while (*file_name != 0) {
			/* Find the first non-blank */
			if (*file_name != ' ')
				break;
			file_name++;
		}
		end_ptr = file_name;
		while (*end_ptr != 0) {
			if (*end_ptr == ';' || *end_ptr == ' ' ||
				isprint(*end_ptr) == 0) {
				*end_ptr = 0;
				break;
			}
			end_ptr++;
		}

		if (file_name == end_ptr) {
			DIAG_LOGE("Skipping line. No file name found. line: %s\n", line);
			print_help = 1;
			continue;
		}
		proc_type = (int)val;
		if (proc_type == 0) {
			strlcpy(mask_file, file_name, FILE_NAME_LEN);
			DIAG_LOGE("Mask list read for proc_type: %d, mask file: %s\n",
					proc_type, mask_file);
		} else {
			strlcpy(mask_file_mdm, file_name, FILE_NAME_LEN);
			DIAG_LOGE("Mask list read for proc_type: %d, mask file: %s\n",
					proc_type, mask_file_mdm);
		}

		status = diag_read_mask_file();
		if (status != 0) {
			if (proc_type == 0) {
				DIAG_LOGE("Error reading mask file: %s\n", mask_file);
			} else {
				DIAG_LOGE("Error reading mask file: %s\n", mask_file_mdm);
			}
		} else {
			num_files++;
		}
	}

	fclose(list_fp);

	DIAG_LOGE("Reading list of mask files complete. Successfully read %d files\n", num_files);
	if (print_help) {
		DIAG_LOGE("File format: proc_type full_path_to_config_file\n");
		DIAG_LOGE("Supported proc_types:\n");
		DIAG_LOGE("0 - MSM\n");
		DIAG_LOGE("Additional proc_types only valid for devices with remote processors\n");
		DIAG_LOGE("1 - MDM\n");
		DIAG_LOGE("2 - MDM2\n");
		DIAG_LOGE("3 - MDM3\n");
		DIAG_LOGE("4 - MDM4\n");
		DIAG_LOGE("5 - QSC (SMUX)\n");
	}

	return num_files;
}

static int __diag_switch_logging(struct diag_logging_mode_param_t *params, char *dir_location_msm)
{
	int z;
	int err = 0;
	int mode = 0;

	if (!params)
		return -1;

	mode = params->req_mode;
	if (mode == logging_mode) {
		DIAG_LOGE("diag: no actual logging switch required\n");
		return 0;
	}

	/*
	 * If previously logging mode is MEMORY DEVICE then flush
	 * the buffer as we are switching to a different
	 * logging mode.
	 */

	if(log_to_memory) {
		flush_buffer(0);
		log_to_memory = 0;
	}

	if(logging_mode == MEMORY_DEVICE_MODE) {
		for (z = 0; z < NUM_PROC; z++) {
			if (buffer_init[z] == READY) {
				close_logging_file(z);
				count_written_bytes[z] = 0;
				pools[0].buffer_ptr[z] = pool0_buffers[z];
				pools[1].buffer_ptr[z] = pool1_buffers[z];
			}
		}
	}

	if(mode == MEMORY_DEVICE_MODE) {
		fd_dev = -1;
		peripheral_name[0] = '\0';
		if (diag_peripheral_mask)
			diag_get_peripheral_name_from_mask(peripheral_name,
							FILE_NAME_LEN,
							diag_peripheral_mask);
		pthread_create( &disk_write_hdl, NULL, WriteToDisk, NULL);
		if (disk_write_hdl == 0) {
			DIAG_LOGE("Failed to create write thread");
			DIAG_LOGE(" Exiting...........\n");
			if (diag_is_wakelock_init()) {
				diag_wakelock_release();
				diag_wakelock_destroy();
			}
			exit(-1);
		}
		log_to_memory = 1;
	} else if(mode == UART_MODE) {
		fd_dev = fd_uart;
		if (dir_location_msm)
			uart_logging_proc = *(int *)dir_location_msm;
	} else if (mode == SOCKET_MODE) {
		fd_dev = fd_socket[device_num];
	} else if (mode == CALLBACK_MODE) {
		/* make sure callback function is registered */
		for (z = 0; z < NUM_PROC; z++) {
			if (cb_clients[z].inited && !(cb_clients[z].cb_func_ptr)) {
				DIAG_LOGE("diag: callback function not registered for proc %d\n", z);
				DIAG_LOGE("diag: unable to change logging mode \n");
				return -1;
			}
		}
	}

	err = ioctl(diag_fd, DIAG_IOCTL_SWITCH_LOGGING, params, sizeof(struct diag_logging_mode_param_t), NULL, 0, NULL, NULL);
	if (err) {
		DIAG_LOGE("diag: unable to switch logging mode to %d, err: %d, errno: %d\n",
			  mode, err, errno);
		return -1;
	} else {
		if (logging_mode == MEMORY_DEVICE_MODE) {
			DIAG_LOGE("diag: Sending signal to thread\n");
			pthread_mutex_lock(&stop_mutex);
			pthread_cond_signal(&stop_cond);
			pthread_mutex_unlock(&stop_mutex);
		}
		DIAG_LOGE(" logging switched \n");
		if (mode == MEMORY_DEVICE_MODE) {
			if (!create_diag_qshrink4_db_parser_thread(diag_peripheral_mask)) {
				if (diag_is_wakelock_init()) {
					diag_wakelock_release();
					diag_wakelock_destroy();
				}
				exit(-1);
			}
		}

		if (dir_location_msm &&
		    (mode == MEMORY_DEVICE_MODE)) {
			strlcpy(output_dir[MSM], dir_location_msm,FILE_NAME_LEN);
			DIAG_LOGE("Output dirs %s --- %s\n", output_dir[MSM], output_dir[MDM]);
		}
		logging_mode = mode;
	}
	return 0;
}

int diag_switch_logging_proc(struct diag_logging_mode_param_t *params)
{
	return __diag_switch_logging(params, output_dir[MSM]);
}

/*===========================================================================
FUNCTION   diag_switch_logging

DESCRIPTION
  This switches the logging mode from default USB to memory device logging

DEPENDENCIES
  valid data type to be passed in:
  In case of ODL second argument is to specify the directory location for logs  In case of UART  logging second argument is to specify PROC type.

RETURN VALUE
  None

SIDE EFFECTS
  None

===========================================================================*/
void diag_switch_logging(int requested_mode, char *dir_location_msm)
{
	uint16 remote_mask;
	int err, dev_idx, device_mask = 1;
	struct diag_logging_mode_param_t params;
	struct diag_con_all_param_t params_con;

	params_con.diag_con_all = DIAG_CON_ALL;

	err = ioctl(diag_fd, DIAG_IOCTL_QUERY_CON_ALL, &params_con,
		    sizeof(struct diag_con_all_param_t), NULL, 0, NULL, NULL);
	if (err) {
		DIAG_LOGE("diag:%s: Querying peripheral info from kernel unsupported, Using default stats, err: %d, errno: %d\n",
			 __func__, err, errno);
		params_con.diag_con_all = DIAG_CON_ALL;
	} else {
		DIAG_LOGE("diag:%s: kernel supported: NUM_PERIPHERALS = %d, DIAG_CON_ALL: %d\n",
			__func__, params_con.num_peripherals, params_con.diag_con_all);
	}

	params.req_mode = requested_mode;
	params.mode_param = DIAG_MD_PERIPHERAL;
	params.peripheral_mask = params_con.diag_con_all;
	params.pd_mask = 0;
	params.diag_id = 0;
	params.pd_val = 0;
	params.peripheral = -EINVAL;
	if (diag_has_remote_device(&remote_mask) == 1) {
		for (dev_idx = 1; dev_idx <= NUM_PROC; dev_idx++)
			if (remote_mask & 1 << (dev_idx - 1))
				device_mask = device_mask | (1 << dev_idx);
	}
	params.device_mask = device_mask;
	__diag_switch_logging(&params, dir_location_msm);
}

void diag_register_callback(int (*client_cb_func_ptr)(unsigned char *ptr, int len, void *context_data), void *context_data)
{
	int err = 0;
	struct diag_callback_reg_t reg;

	if (!client_cb_func_ptr) {
		DIAG_LOGE("diag: Unable to register callback\n");
		return;
	}

	cb_clients[MSM].inited = 1;
	cb_clients[MSM].cb_func_ptr = client_cb_func_ptr;
	cb_clients[MSM].context_data = context_data;
	reg.proc = MSM;

	err = ioctl(diag_fd, DIAG_IOCTL_REGISTER_CALLBACK, &reg, 0, NULL, 0, NULL, NULL);
	if (err)
		DIAG_LOGE("diag: In %s, Unable to register with the driver, err: %d\n", __func__, err);
}

void diag_register_remote_callback(int (*client_rmt_cb_func_ptr)(unsigned char *ptr, int len, void *context_data), int proc, void *context_data)
{
	int err = 0;
	uint16 remote_proc = 0;
	struct diag_callback_reg_t reg;

	if (!client_rmt_cb_func_ptr) {
		DIAG_LOGE("diag: Unable to register callback\n");
		return;
	}
	if (proc <= 0 || proc >= NUM_PROC) {
		DIAG_LOGE("diag: Invalid processor ID\n");
		return;
	}
	diag_has_remote_device(&remote_proc);
	if ((remote_proc & proc) != proc) {
		DIAG_LOGE("diag: Cannot register callback. Processor not supported, requested: %d\n", proc);
		return;
	}
	cb_clients[proc].inited = 1;
	cb_clients[proc].cb_func_ptr = client_rmt_cb_func_ptr;
	cb_clients[proc].context_data = context_data;
	reg.proc = proc;

	err = ioctl(diag_fd, DIAG_IOCTL_REGISTER_CALLBACK, &reg, 0, NULL, 0, NULL, NULL);
	if (err)
		DIAG_LOGE("diag: In %s, Unable to register with the driver, err: %d\n", __func__, err);
}

void send_mask_modem(unsigned char mask_buf[], int count_mask_bytes)
{
	diag_send_data(mask_buf, count_mask_bytes);
}

int diag_send_data(unsigned char buf[], int bytes)
{
	int bytes_written = 0;
	errno = 0;
	if (diag_fd == DIAG_INVALID_HANDLE)
		return 0;

	bytes_written = write(diag_fd,(const void*)buf, bytes);
	if (*(int *)buf == DCI_DATA_TYPE) {
		if (bytes_written != DIAG_DCI_NO_ERROR) {
			DIAG_LOGE(" DCI send data failed, bytes written: %d, error: %d\n", bytes_written, errno);
			return DIAG_DCI_SEND_DATA_FAIL;
		}
	} else if (bytes_written != 0)
		DIAG_LOGE(" Send data failed, bytes written: %d, error: %d\n", bytes_written, errno);

	return bytes_written;
}

static int diag_callback_send_data_int(int type, int proc, unsigned char *buf, int len)
{
	int offset = CALLBACK_TYPE_LEN;
	uint16 remote_proc = 0;
	unsigned char data_buf[len + CALLBACK_TYPE_LEN + CALLBACK_PROC_TYPE_LEN];

	if (proc < MSM || proc >= NUM_PROC) {
		DIAG_LOGE("diag: Invalid processor ID\n");
		return -1;
	}
	if (len <= 0) {
		DIAG_LOGE("diag: Invalid length %d in %s", len, __func__);
		return -1;
	}
	if (!buf) {
		DIAG_LOGE("diag: Invalid buffer in %s", __func__);
		return -1;
	}
	switch (type) {
	case USER_SPACE_RAW_DATA_TYPE:
	case USER_SPACE_DATA_TYPE:
		break;
	default:
		DIAG_LOGE("diag: Invalid type %d in %s\n", type, __func__);
		return -1;
	}

	*(int *)data_buf = type;
	if (proc != MSM) {
		diag_has_remote_device(&remote_proc);
		if ((remote_proc & proc) != proc) {
			DIAG_LOGE("diag: Processor not supported, requested: %d\n", proc);
			return -1;
		}
		*(int *)(data_buf + offset) = -proc;
		offset += CALLBACK_PROC_TYPE_LEN;
	}
	memcpy(data_buf + offset, buf, len);
	return diag_send_data(data_buf, len + offset);
}

int diag_callback_send_data(int proc, unsigned char * buf, int len)
{
	return diag_callback_send_data_int(USER_SPACE_RAW_DATA_TYPE, proc, buf, len);
}

int diag_callback_send_data_hdlc(int proc, unsigned char * buf, int len)
{
	return diag_callback_send_data_int(USER_SPACE_DATA_TYPE, proc, buf, len);
}

int diag_vote_md_real_time(int real_time)
{
	return diag_vote_md_real_time_proc(MSM, real_time);
}

int diag_vote_md_real_time_proc(int proc, int real_time)
{
	int ret = -1;
	struct real_time_vote_t vote;

	if (proc < MSM || proc > MDM) {
		DIAG_LOGE("diag: invalid proc %d in %s\n", proc, __func__);
		return -1;
	}

	if (!(real_time == MODE_REALTIME || real_time == MODE_NONREALTIME)) {
		DIAG_LOGE("diag: invalid mode change request\n");
		return -1;
	}

	vote.client_id = proc;
	vote.proc = DIAG_PROC_MEMORY_DEVICE;
	vote.real_time_vote = real_time;
	ret = ioctl(diag_fd, DIAG_IOCTL_VOTE_REAL_TIME, &vote, 0, NULL, 0, NULL, NULL);
	return ret;
}

int diag_get_real_time_status(int *real_time)
{
	return diag_get_real_time_status_proc(MSM, real_time);
}

int diag_get_real_time_status_proc(int proc, int *real_time)
{
	int err = 0;
	struct real_time_query_t query;

	if (!real_time) {
		DIAG_LOGE("diag: invalid pointer in %s\n", __func__);
		return -1;
	}
	if (proc < MSM || proc > MDM) {
		DIAG_LOGE("diag: invalid proc %d in %s\n", proc, __func__);
		return -1;
	}
	query.proc = proc;
	err = ioctl(diag_fd, DIAG_IOCTL_GET_REAL_TIME, &query, 0, NULL, 0, NULL, NULL);
	if (err != 0) {
		DIAG_LOGE(" diag: error in getting real time status, proc: %d, err: %d, error: %d\n", proc, err, errno);
		return -1;
	}

	*real_time = query.real_time;
	return 0;
}

static void diag_callback_init()
{
	int i;

	for (i = 0; i < NUM_PROC; i++) {
		cb_clients[i].inited = 0;
		cb_clients[i].cb_func_ptr = NULL;
		cb_clients[i].context_data = NULL;
	}
}

/*===========================================================================
FUNCTION   Diag_LSM_Init

DESCRIPTION
  Initializes the Diag Legacy Mapping Layer. This should be called
  only once per process.

DEPENDENCIES
  Successful initialization requires Diag CS component files to be present
  and accessible in the file system.

RETURN VALUE
  FALSE = failure, else TRUE

SIDE EFFECTS
  None

===========================================================================*/

boolean Diag_LSM_Init (byte* pIEnv)
{
   sigset_t set;
   int ret, i;
   (void)pIEnv;

   pthread_mutex_lock(&lsm_init_count_mutex);
	if(lsm_init_count == 0) {
		// Open a handle to the diag driver
		if (diag_fd == DIAG_INVALID_HANDLE) {
#ifdef FEATURE_LOG_STDOUT
			diag_fd = 0; // Don't need to open anything to use stdout, so fake it.
#else // not FEATURE_LOG_STDOUT
			diag_fd = open("/dev/diag", O_RDWR);
#endif // FEATURE_LOG_STDOUT

			if (diag_fd == DIAG_INVALID_HANDLE) {
				DIAG_LOGE(" Diag_LSM_Init: Failed to open handle to diag driver,"
						  " error = %d", errno);
				pthread_mutex_unlock(&lsm_init_count_mutex);
				lsm_init_count++;
				return FALSE;
			}

			pthread_mutex_init(&(pools[0].write_mutex), NULL);
			pthread_cond_init(&(pools[0].write_cond), NULL);
			pthread_mutex_init(&(pools[0].read_mutex), NULL);
			pthread_cond_init(&(pools[0].read_cond), NULL);
			pthread_mutex_init(&(pools[1].write_mutex), NULL);
			pthread_cond_init(&(pools[1].write_cond), NULL);
			pthread_mutex_init(&(pools[1].read_mutex), NULL);
			pthread_cond_init(&(pools[1].read_cond), NULL);
			pthread_mutex_init(&stop_mutex, NULL);
			pthread_cond_init(&stop_cond, NULL);

			flush_in_progress = 0;

			/* Allocate initial buffer of MSM data only */
#if 0
			/* Since calloc allocation is not working */
			pool0_buffers[0] = calloc(DISK_BUF_SIZE, 1);
			if(!pool0_buffers[0])
			return FALSE;

			pool1_buffers[0] = calloc(DISK_BUF_SIZE, 1);
			if(!pool1_buffers[0]) {
				free(pool0_buffers[0]);
				return FALSE;
			}
#endif
			/* Manually doing the initialisation as dynamic alloc is not working */

			pool0_buffers[0] = static_buffer[0];
			pool0_buffers[1] = static_buffer[2];
			pool0_buffers[2] = static_buffer[4];
			pool1_buffers[0] = static_buffer[1];
			pool1_buffers[1] = static_buffer[3];
			pool1_buffers[2] = static_buffer[5];

			pools[0].bytes_in_buff[0] = pools[0].bytes_in_buff[1] = pools[0].bytes_in_buff[2] = 0;
			pools[1].bytes_in_buff[0] = pools[1].bytes_in_buff[1] = pools[1].bytes_in_buff[2] = 0;
			buffer_init[0] = READY;

			pools[0].buffer_ptr[0] = pool0_buffers[0];
			pools[1].buffer_ptr[0] = pool1_buffers[0];

			pools[0].buffer_ptr[1] = pool0_buffers[1];
			pools[1].buffer_ptr[1] = pool1_buffers[1];

			pools[0].buffer_ptr[2] = pool0_buffers[2];
			pools[1].buffer_ptr[2] = pool1_buffers[2];

			/* Block SIGUSR2 for the main application */
			if ((sigemptyset((sigset_t *) &set) == -1) ||
				(sigaddset(&set, SIGUSR2) == -1))
				DIAG_LOGE("diag: Failed to initialize block set\n");

			ret = sigprocmask(SIG_BLOCK, &set, NULL);
			if (ret != 0)
				DIAG_LOGE("diag: Failed to block signal for main thread\n");

			diag_socket_init();
		gdwClientID = getpid();

		/* Initialize buffers needed for Diag event, log, F3 services */

		if (!DiagSvc_Malloc_Init())
			goto failure_case;

		/* Initialize the services */
		if (!Diag_LSM_Pkt_Init())
			goto failure_case;

		if (!Diag_LSM_Log_Init())
			goto failure_case;

		if (!Diag_LSM_Msg_Init())
			goto failure_case;

		if (!Diag_LSM_Event_Init())
			goto failure_case;

		if (diag_lsm_dci_init() != DIAG_DCI_NO_ERROR)
			goto failure_case;

		diag_callback_init();

		diag_qshrink4_init();

		/* Creating read thread which listens for various masks & pkt
		 * requests */
		pthread_create(&read_thread_hdl, NULL, CreateWaitThread, NULL);
		if (read_thread_hdl == 0) {
			DIAG_LOGE("Diag_LSM.c: Failed to create read thread");
			goto failure_case;
		}
		/* Performe mask synchronization to ensure
		 * we have the masks before proceeding */
		if (!do_mask_sync())
			goto failure_case;
		}
	}
	lsm_init_count++;
	pthread_mutex_unlock(&lsm_init_count_mutex);
	return TRUE;

failure_case:
	lsm_init_count++;
	pthread_mutex_unlock(&lsm_init_count_mutex);
	Diag_LSM_DeInit();
	return FALSE;
}


/*===========================================================================

FUNCTION    Diag_LSM_DeInit

DESCRIPTION
  De-Initialize the Diag service.

DEPENDENCIES
  None.

RETURN VALUE
  FALSE = failure, else TRUE.
  Currently all the internal boolean return functions called by
  this function just returns TRUE w/o doing anything.

SIDE EFFECTS
  None

===========================================================================*/

boolean Diag_LSM_DeInit(void)
{
	boolean bReturn = TRUE;
	int i;

	pthread_mutex_lock(&lsm_init_count_mutex);
	if (lsm_init_count > 1) {
		lsm_init_count--;
		pthread_mutex_unlock(&lsm_init_count_mutex);
		return bReturn;
	}

	if (diag_fd != DIAG_INVALID_HANDLE) {
		int ret;

		socket_inited = 0;

		/* Free the buffers mallocated for events, logs, messages and packet req/res */
		DiagSvc_Malloc_Exit();

		if (!Diag_LSM_Pkt_DeInit())
			bReturn  = FALSE;

		if ((ret = ioctl(diag_fd, DIAG_IOCTL_LSM_DEINIT, NULL, 0, NULL, 0, NULL, NULL)) != 1) {
			DIAG_LOGE(" Diag_LSM_DeInit: DeviceIOControl failed. ret: %d, error: %d\n", ret, errno);
			bReturn = FALSE;
		}

		ret = pthread_join(read_thread_hdl, NULL);
		if (ret != 0) {
			DIAG_LOGE("diag: In %s, Error trying to join with thread: %d\n",
					  __func__, ret);
			bReturn = FALSE;
		}

		ret = close(diag_fd);
		if (ret < 0) {
			DIAG_LOGE("diag: In %s, error closing file, ret: %d, errno: %d\n",
					  __func__, ret, errno);
			bReturn = FALSE;
		}

		diag_fd = DIAG_INVALID_HANDLE;

		for (i = 0; i < NUM_PROC; i++)
			delete_oldest_file_list(i);
		circular_logging_inited = 0;

		diag_lsm_dci_deinit();

		Diag_LSM_Log_DeInit();

		Diag_LSM_Event_DeInit();

		Diag_LSM_Msg_DeInit();
	}
	lsm_init_count = 0;
	pthread_mutex_unlock(&lsm_init_count_mutex);
	return bReturn;
}     /* Diag_LSM_DeInit */

/* Internal functions */
static void *CreateWaitThread(void* param)
{
	sigset_t set;
	int rc, z;
	struct  sigaction sact;
	(void)param;

	sigemptyset( &sact.sa_mask );
	sact.sa_flags = 0;
	sact.sa_handler = dummy_handler;
	sigaction(SIGUSR2, &sact, NULL );

	if ((sigemptyset((sigset_t *) &set) == -1) ||
		(sigaddset(&set, SIGUSR2) == -1))
		DIAG_LOGE("diag: Failed to initialize block set\n");

	rc = pthread_sigmask(SIG_UNBLOCK, &set, NULL);
	if (rc != 0)
		DIAG_LOGE("diag: Failed to unbock signal for read thread\n");

	do{
		if (flush_log) {

			while(write_in_progress) {
				sleep(1);
			}

			DIAG_LOGE(" %s exiting ...[%d]..\n",
					__func__, curr_read);
			pools[curr_read].data_ready = 1;
			pools[curr_read].free = 0;

			for (z = 0; z < NUM_PROC; z++) {
				if (curr_read)
					pools[curr_read].buffer_ptr[z] =
						pool1_buffers[z];
				else
					pools[curr_read].buffer_ptr[z] =
						pool0_buffers[z];
			}

			pthread_mutex_lock(
					&pools[curr_read].write_mutex);
			pthread_mutex_lock(&stop_mutex);
			flush_log++;
			pthread_mutex_unlock(&stop_mutex);

			pthread_cond_signal(
					&pools[curr_read].write_cond);
			pthread_mutex_unlock(
					&pools[curr_read].write_mutex);
			curr_read = !curr_read;
			/* As cleanup started now wait for cleanup to
			 * complete.
			 */
			pthread_mutex_lock(&stop_mutex);
			if(flush_log)
				pthread_cond_wait(&stop_cond,
						&stop_mutex);
			pthread_mutex_unlock(&stop_mutex);
			break;
		}else {
			in_read = 1;
			num_bytes_read = read(diag_fd, (void*)read_buffer,
					READ_BUF_SIZE);

			in_read = 0;
			/* read might return 0 across suspend */
			if (!num_bytes_read)
				continue;
			if((*(int *)read_buffer == DEINIT_TYPE) ||
					(num_bytes_read < 0))
				break;
			if(!flush_log)
				process_diag_payload();
		}
	}while(1);

	return 0;
}

int diag_configure_peripheral_buffering_tx_mode(uint8 peripheral, uint8 tx_mode,
						uint8 low_wm_val, uint8 high_wm_val)
{
	int err = 0;
	struct diag_periph_buffering_tx_mode params;

	switch (tx_mode) {
	case DIAG_STREAMING_MODE:
	case DIAG_CIRCULAR_BUFFERING_MODE:
	case DIAG_THRESHOLD_BUFFERING_MODE:
		break;
	default:
		DIAG_LOGE("diag: In %s, invalid tx mode requested %d\n", __func__, tx_mode);
		return -EINVAL;
	}

	if (peripheral >= NUM_PERIPHERALS) {
		DIAG_LOGE("diag: In %s, invalid peripheral %d\n", __func__, peripheral);
		return -EINVAL;
	}

	if ((((high_wm_val > 100) || (low_wm_val  > 100)) || (low_wm_val  > high_wm_val)) ||
	    ((low_wm_val == high_wm_val) && ((low_wm_val != 0) && (high_wm_val != 0)))) {
		DIAG_LOGE("diag: In %s, invalid watermark values, low: %d, high: %d\n",
			  __func__, low_wm_val, high_wm_val);
		return -EINVAL;
	}

	params.peripheral = peripheral;
	params.mode = tx_mode;
	params.low_wm_val = low_wm_val;
	params.high_wm_val = high_wm_val;

	err = ioctl(diag_fd, DIAG_IOCTL_CONFIG_BUFFERING_TX_MODE, &params, sizeof(params), NULL, 0, NULL, NULL);
	if (err) {
		DIAG_LOGE("diag: In %s, unable to set peripheral buffering mode, ret: %d, error: %d\n",
			  __func__, err, errno);
		return err;
	}

	return 1;
}

int diag_peripheral_buffering_drain_immediate(uint8 peripheral)
{
	int err = 0;

	if (peripheral >= NUM_PERIPHERALS) {
		DIAG_LOGE("diag: In %s, invalid peripheral %d\n", __func__, peripheral);
		return -EINVAL;
	}

	err = ioctl(diag_fd, DIAG_IOCTL_BUFFERING_DRAIN_IMMEDIATE, &peripheral,
		    sizeof(peripheral), NULL, 0, NULL, NULL);
	if (err) {
		DIAG_LOGE("diag: In %s, unable to send ioctl to drain immediate, ret: %d, error: %d\n",
			  __func__, err, errno);
		return err;
	}

	return 1;
}

int diag_hdlc_toggle(uint8 hdlc_support)
{
	int err = 0;
	errno = 0;

	if (hdlc_support > 1) {
		DIAG_LOGE("diag: In %s, invalid request %d\n", __func__, hdlc_support);
		return -EINVAL;
	}

	err = ioctl(diag_fd, DIAG_IOCTL_HDLC_TOGGLE, &hdlc_support,
		    sizeof(hdlc_support), NULL, 0, NULL, NULL);
	if (err) {
		DIAG_LOGE("diag: In %s, unable to send ioctl to change hdlc support, ret: %d, error: %d\n",
			  __func__, err, errno);
		return err;
	}

	pthread_mutex_lock(&hdlc_toggle_mutex);
	hdlc_disabled = hdlc_support;
	pthread_mutex_unlock(&hdlc_toggle_mutex);

	return 1;
}

int add_guid_to_qshrink4_header(unsigned char * guid) {
	memcpy(&qshrink4_data.guid[qshrink4_data.guid_list_entry_count], guid, GUID_LEN);
	qshrink4_data.guid_list_entry_count++;
	qshrink4_data.header_length = qshrink4_data.header_length + GUID_LEN;
	return 0;
}
void diag_set_peripheral_mask(unsigned int peripheral_mask)
{
	diag_peripheral_mask = peripheral_mask;

}
