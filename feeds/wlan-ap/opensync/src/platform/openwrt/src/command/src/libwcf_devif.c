/* SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include <fcntl.h>

#include "websocket/libwcf.h"
#include "websocket/libwcf_debug.h"
#include "websocket/libwcf_private.h"

#define BR_NAME     "br_c"

int br_socket_fd = -1;
unsigned int AppLogMask = 0xFFFFFFFF;
time_t MaskUpdated = 0;

static FILE *fpopen(const char *dir, const char *name)
{
	char path[SYSFS_PATH_MAX];

	snprintf(path, SYSFS_PATH_MAX, "%s/%s", dir, name);
	return fopen(path, "r");
}

static void fetch_id(const char *dev, const char *name, struct bridge_id *id)
{
	FILE *f = fpopen(dev, name);

	fscanf(f, "%2hhx%2hhx.%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx",
	      &id->prio[0], &id->prio[1],
	      &id->addr[0], &id->addr[1], &id->addr[2],
	      &id->addr[3], &id->addr[4], &id->addr[5]);
	fclose(f);
}

/* Fetch an integer attribute out of sysfs. */
static int fetch_int(const char *dev, const char *name)
{
	FILE *f = fpopen(dev, name);
	int value = -1;

	if (!f)
		fprintf(stderr, "%s: %s\n", dev, strerror(errno));
	else {
		fscanf(f, "%i", &value);
		fclose(f);
	}
	return value;
}

/* Get a time value out of sysfs */
static void fetch_tv(const char *dev, const char *name,
		     struct timeval *tv)
{
	__jiffies_to_tv(tv, fetch_int(dev, name));
}

/*
 * Convert device name to an index in the list of ports in bridge.
 *
 * Old API does bridge operations as if ports were an array
 * inside bridge structure.
 */
static int get_portno(const char *brname, const char *ifname)
{
	int i;
	int ifindex = if_nametoindex(ifname);
	int ifindices[MAX_PORTS];
	unsigned long args[4] = { BRCTL_GET_PORT_LIST,
				  (unsigned long)ifindices, MAX_PORTS, 0 };
	struct ifreq ifr;

	if (ifindex <= 0)
		goto error;

    if( br_socket_fd < 0 ) {
        return -1;
	}

	memset(ifindices, 0, sizeof(ifindices));
	strncpy(ifr.ifr_name, brname, IFNAMSIZ-1);
	ifr.ifr_data = (char *) &args;

	if (ioctl(br_socket_fd, SIOCDEVPRIVATE, &ifr) < 0) {
		dprintf("get_portno: get ports of %s failed: %s\n",
			brname, strerror(errno));
		goto error;
	}

	for (i = 0; i < MAX_PORTS; i++) {
		if (ifindices[i] == ifindex)
			return i;
	}

	dprintf("%s is not a in bridge %s\n", ifname, brname);
 error:
	return -1;
}

/* get information via ioctl */
/*static int old_get_bridge_info(const char *bridge, struct bridge_info *info)
{
	struct ifreq ifr;
	struct __bridge_info i;
	unsigned long args[4] = { BRCTL_GET_BRIDGE_INFO,
				  (unsigned long) &i, 0, 0 };

    if( br_socket_fd < 0 ) {
        return -1;
	}

	memset(info, 0, sizeof(*info));
	strncpy(ifr.ifr_name, bridge, IFNAMSIZ-1);
	ifr.ifr_data = (char *) &args;

	if (ioctl(br_socket_fd, SIOCDEVPRIVATE, &ifr) < 0) {
		dprintf("%s: can't get info %s\n",
			bridge, strerror(errno));
		return errno;
	}

	memcpy(&info->designated_root, &i.designated_root, 8);
	memcpy(&info->bridge_id, &i.bridge_id, 8);
	info->root_path_cost = i.root_path_cost;
	info->root_port = i.root_port;
	info->topology_change = i.topology_change;
	info->topology_change_detected = i.topology_change_detected;
	info->stp_enabled = i.stp_enabled;
	__jiffies_to_tv(&info->max_age, i.max_age);
	__jiffies_to_tv(&info->hello_time, i.hello_time);
	__jiffies_to_tv(&info->forward_delay, i.forward_delay);
	__jiffies_to_tv(&info->bridge_max_age, i.bridge_max_age);
	__jiffies_to_tv(&info->bridge_hello_time, i.bridge_hello_time);
	__jiffies_to_tv(&info->bridge_forward_delay, i.bridge_forward_delay);
	__jiffies_to_tv(&info->ageing_time, i.ageing_time);
	__jiffies_to_tv(&info->hello_timer_value, i.hello_timer_value);
	__jiffies_to_tv(&info->tcn_timer_value, i.tcn_timer_value);
	__jiffies_to_tv(&info->topology_change_timer_value,
			i.topology_change_timer_value);
	__jiffies_to_tv(&info->gc_timer_value, i.gc_timer_value);

	return 0;
}
*/
#if 0
/*
 * Get bridge parameters using either sysfs or old
 * ioctl.
 */
int br_get_bridge_info(const char *bridge, struct bridge_info *info)
{
	DIR *dir;
	char path[SYSFS_PATH_MAX];

	snprintf(path, SYSFS_PATH_MAX, SYSFS_CLASS_NET "%s/bridge", bridge);
	dir = opendir(path);
	if (dir == NULL) {
		dprintf("path '%s' is not a directory\n", path);
		goto fallback;
	}

	memset(info, 0, sizeof(*info));
	fetch_id(path, "root_id", &info->designated_root);
	fetch_id(path, "bridge_id", &info->bridge_id);
	info->root_path_cost = fetch_int(path, "root_path_cost");
	fetch_tv(path, "max_age", &info->max_age);
	fetch_tv(path, "hello_time", &info->hello_time);
	fetch_tv(path, "forward_delay", &info->forward_delay);
	fetch_tv(path, "max_age", &info->bridge_max_age);
	fetch_tv(path, "hello_time", &info->bridge_hello_time);
	fetch_tv(path, "forward_delay", &info->bridge_forward_delay);
	fetch_tv(path, "ageing_time", &info->ageing_time);
	fetch_tv(path, "hello_timer", &info->hello_timer_value);
	fetch_tv(path, "tcn_timer", &info->tcn_timer_value);
	fetch_tv(path, "topology_change_timer",
		 &info->topology_change_timer_value);;
	fetch_tv(path, "gc_timer", &info->gc_timer_value);

	info->root_port = fetch_int(path, "root_port");
	info->stp_enabled = fetch_int(path, "stp_state");
	info->topology_change = fetch_int(path, "topology_change");
	info->topology_change_detected = fetch_int(path, "topology_change_detected");

	closedir(dir);
	return 0;

fallback:
	return old_get_bridge_info(bridge, info);
}
#endif

static int old_get_port_info(const char *brname, const char *port,
			     struct port_info *info)
{
	struct __port_info i;
	int index;

	memset(info, 0, sizeof(*info));

	index = get_portno(brname, port);
	if (index < 0)
		return errno;

	else {
		struct ifreq ifr;
		unsigned long args[4] = { BRCTL_GET_PORT_INFO,
					   (unsigned long) &i, index, 0 };

        if( br_socket_fd < 0 )
            return -1;

		strncpy(ifr.ifr_name, brname, IFNAMSIZ-1);
		ifr.ifr_data = (char *) &args;

		if (ioctl(br_socket_fd, SIOCDEVPRIVATE, &ifr) < 0) {
			dprintf("old can't get port %s(%d) info %s\n",
				brname, index, strerror(errno));
			return errno;
		}
	}

	info->port_no = index;
	memcpy(&info->designated_root, &i.designated_root, 8);
	memcpy(&info->designated_bridge, &i.designated_bridge, 8);
	info->port_id = i.port_id;
	info->designated_port = i.designated_port;
	info->path_cost = i.path_cost;
	info->designated_cost = i.designated_cost;
	info->state = i.state;
	info->top_change_ack = i.top_change_ack;
	info->config_pending = i.config_pending;
	__jiffies_to_tv(&info->message_age_timer_value,
			i.message_age_timer_value);
	__jiffies_to_tv(&info->forward_delay_timer_value,
			i.forward_delay_timer_value);
	__jiffies_to_tv(&info->hold_timer_value, i.hold_timer_value);
	return 0;
}

/*
 * Get information about port on bridge.
 */
int br_get_port_info(const char *brname, const char *port,
		     struct port_info *info)
{
	DIR *d;
	char path[SYSFS_PATH_MAX];

	snprintf(path, SYSFS_PATH_MAX, SYSFS_CLASS_NET "%s/brport", port);
	d = opendir(path);
	if (!d)
		goto fallback;

	memset(info, 0, sizeof(*info));

	fetch_id(path, "designated_root", &info->designated_root);
	fetch_id(path, "designated_bridge", &info->designated_bridge);
	info->port_no = fetch_int(path, "port_no");
	info->port_id = fetch_int(path, "port_id");
	info->designated_port = fetch_int(path, "designated_port");
	info->path_cost = fetch_int(path, "path_cost");
	info->designated_cost = fetch_int(path, "designated_cost");
	info->state = fetch_int(path, "state");
	info->top_change_ack = fetch_int(path, "change_ack");
	info->config_pending = fetch_int(path, "config_pending");
	fetch_tv(path, "message_age_timer", &info->message_age_timer_value);
	fetch_tv(path, "forward_delay_timer", &info->forward_delay_timer_value);
	fetch_tv(path, "hold_timer", &info->hold_timer_value);
	closedir(d);

	return 0;
fallback:
	return old_get_port_info(brname, port, info);
}


static int br_set(const char *bridge, const char *name,
		  unsigned long value, unsigned long oldcode)
{
	int ret;
	char path[SYSFS_PATH_MAX];
	FILE *f;

	snprintf(path, SYSFS_PATH_MAX, SYSFS_CLASS_NET "%s/bridge/%s", bridge, name);
	f = fopen(path, "w");
	if (f) {
		ret = fprintf(f, "%ld\n", value);
		fclose(f);
	} else {
		/* fallback to old ioctl */
		struct ifreq ifr;
		unsigned long args[4] = { oldcode, value, 0, 0 };

        if( br_socket_fd < 0 )
            return -1;

		strncpy(ifr.ifr_name, bridge, IFNAMSIZ-1);
		ifr.ifr_data = (char *) &args;
		ret = ioctl(br_socket_fd, SIOCDEVPRIVATE, &ifr);
	}

	return ret < 0 ? errno : 0;
}

#if 0
int br_set_bridge_forward_delay(const char *br, struct timeval *tv)
{
	return br_set(br, "forward_delay", __tv_to_jiffies(tv),
		      BRCTL_SET_BRIDGE_FORWARD_DELAY);
}

int br_set_bridge_hello_time(const char *br, struct timeval *tv)
{
	return br_set(br, "hello_time", __tv_to_jiffies(tv),
		      BRCTL_SET_BRIDGE_HELLO_TIME);
}

int br_set_bridge_max_age(const char *br, struct timeval *tv)
{
	return br_set(br, "max_age", __tv_to_jiffies(tv),
		      BRCTL_SET_BRIDGE_MAX_AGE);
}
#endif

int br_set_ageing_time(const char *br, struct timeval *tv)
{
	return br_set(br, "ageing_time", __tv_to_jiffies(tv),
		      BRCTL_SET_AGEING_TIME);
}

#if 0
int br_set_stp_state(const char *br, int stp_state)
{
	return br_set(br, "stp_state", stp_state, BRCTL_SET_BRIDGE_STP_STATE);
}

int br_set_bridge_priority(const char *br, int bridge_priority)
{
	return br_set(br, "priority", bridge_priority,
		      BRCTL_SET_BRIDGE_PRIORITY);
}
#endif

/*static int port_set(const char *bridge, const char *ifname,
		    const char *name, unsigned long value,
		    unsigned long oldcode)
{
	int ret;
	char path[SYSFS_PATH_MAX];
	FILE *f;

	snprintf(path, SYSFS_PATH_MAX, SYSFS_CLASS_NET "%s/brport/%s", ifname, name);
	f = fopen(path, "w");
	if (f) {
		ret = fprintf(f, "%ld\n", value);
		fclose(f);
	} else {
		int index = get_portno(bridge, ifname);

		if (index < 0)
			ret = index;
		else {
			struct ifreq ifr;
			unsigned long args[4] = { oldcode, index, value, 0 };

            if( br_socket_fd < 0 )
                return -1;

			strncpy(ifr.ifr_name, bridge, IFNAMSIZ-1);
			ifr.ifr_data = (char *) &args;
			ret = ioctl(br_socket_fd, SIOCDEVPRIVATE, &ifr);
		}
	}

	return ret < 0 ? errno : 0;
}*/


int wc_set_socket( void )
{
    if( br_socket_fd == -1 )
        br_socket_fd = socket( AF_LOCAL, SOCK_STREAM, 0 );

    if( br_socket_fd < 0 )
        return -1;

    return( 0 );
}


int wc_add_tunnel( struct add_tun_parms * TunParms )
{
    struct ifreq ifr;
    unsigned long args[4] = { 1111, 1001, 0, 0 };

    if( br_socket_fd < 0 )
        return -1;

    strncpy( ifr.ifr_name, BR_NAME, IFNAMSIZ-1 );
    args[2] = sizeof( struct add_tun_parms );
    args[3] = (unsigned long)TunParms;
    ifr.ifr_data = (char *) &args;
    return( ioctl( br_socket_fd, SIOCDEVPRIVATE, &ifr ) );
}


int wc_del_tunnel( struct del_tun_parms * TunParms )
{
    struct ifreq ifr;
    unsigned long args[4] = { 1111, 1002, 0, 0 };

    if( br_socket_fd < 0 )
        return -1;
    strncpy( ifr.ifr_name, BR_NAME, IFNAMSIZ-1 );
    args[2] = sizeof( struct del_tun_parms );
    args[3] = (unsigned long)TunParms;
    ifr.ifr_data = (char *) &args;
    return( ioctl( br_socket_fd, SIOCDEVPRIVATE, &ifr ) );
}


int wc_get_logline( struct wc_log_line * LogLine )
{
    struct ifreq ifr;
    unsigned long args[4] = { 1111, 1003, 0, 0 };

    if( br_socket_fd < 0 )
        return -1;
    strncpy( ifr.ifr_name, BR_NAME, IFNAMSIZ-1 );
    args[2] = sizeof( struct wc_log_line );
    args[3] = (unsigned long)LogLine;
    ifr.ifr_data = (char *) &args;
    return( ioctl( br_socket_fd, SIOCDEVPRIVATE, &ifr ) );
}


int wc_get_fr( struct fr_record * LogLine )
{
    struct ifreq ifr;
    unsigned long args[4] = { 1111, 1007, 0, 0 };

    if( br_socket_fd < 0 )
        return -1;
    strncpy( ifr.ifr_name, BR_NAME, IFNAMSIZ-1 );
    args[2] = sizeof( struct fr_record );
    args[3] = (unsigned long)LogLine;
    ifr.ifr_data = (char *) &args;
    return( ioctl( br_socket_fd, SIOCDEVPRIVATE, &ifr ) );
}


int wc_put_logline( unsigned int Mask, const char * format, ... )
{
    int Log = 1;

    if( Mask != 0xFFFFFFFF )
    {
        time_t TempT = time( NULL );

        if( TempT != MaskUpdated )
        {
            FILE * MaskFd;
            int Mask;
            //unsigned int MaskHex;

            MaskFd = fopen( "/proc/sys/kernel/wcf/app_logmask", "r" );
            if( MaskFd )
            {
                fscanf( MaskFd, "%d", &Mask );
                AppLogMask = (unsigned int)Mask;
                fclose( MaskFd );
                MaskUpdated = TempT;
            }
        }
        if( !(Mask & 0x40000000) && ((AppLogMask & Mask) != Mask) )
            Log = 0;
    }
    if( Log )
    {
        struct ifreq ifr;
        unsigned long args[4] = { 1111, 1004, 0, 0 };
        struct wc_log_line LogLine;
        va_list args_log;
        char logBuf[200];

        va_start( args_log, format );
        vsnprintf( logBuf, sizeof( logBuf ), format, args_log );
        logBuf[199] = 0;
        va_end( args_log );

        LogLine.Num = Mask;
        memcpy( &(LogLine.Message[0]), logBuf, 200 );
        strncpy( ifr.ifr_name, BR_NAME, IFNAMSIZ-1 );
        args[2] = sizeof( struct wc_log_line );
        args[3] = (unsigned long)&(LogLine);
        ifr.ifr_data = (char *) &args;
        if( br_socket_fd == -1 )
            br_socket_fd = socket( AF_LOCAL, SOCK_STREAM, 0 );

        if( br_socket_fd < 0 )
            return -1;

        return( ioctl( br_socket_fd, SIOCDEVPRIVATE, &ifr ) );
    }
    else
        return( 0 );
}


int wc_get_tunstats( struct wc_tunnel_stats * TunStats )
{
    struct ifreq ifr;
    unsigned long args[4] = { 1111, 1005, 0, 0 };

    if( br_socket_fd < 0 )
        return -1;
    strncpy( ifr.ifr_name, BR_NAME, IFNAMSIZ-1 );
    args[2] = sizeof( struct wc_tunnel_stats );
    args[3] = (unsigned long)TunStats;
    ifr.ifr_data = (char *) &args;
    return( ioctl( br_socket_fd, SIOCDEVPRIVATE, &ifr ) );
}


int wc_switch_wireless( int Dir )
{
    struct ifreq ifr;
    unsigned long args[4] = { 1111, 1006, 0, 0 };

    if( br_socket_fd < 0 )
        return -1;
    strncpy( ifr.ifr_name, BR_NAME, IFNAMSIZ-1 );
    args[2] = sizeof( int );
    args[3] = (unsigned long)&Dir;
    ifr.ifr_data = (char *) &args;
    return( ioctl( br_socket_fd, SIOCDEVPRIVATE, &ifr ) );
}


int wc_report_radius( unsigned int Code, unsigned int Addr, unsigned int Time )
{
    struct ifreq ifr;
    unsigned long args[4] = { 1111, 1008, 0, 0 };
    unsigned int Parms[3];

    if( br_socket_fd < 0 )
        return -1;
    Parms[0] = Code;
    Parms[1] = Addr;
    Parms[2] = Time;
    strncpy( ifr.ifr_name, BR_NAME, IFNAMSIZ-1 );
    args[2] = sizeof( Parms );
    args[3] = (unsigned long)Parms;
    ifr.ifr_data = (char *) &args;
    return( ioctl( br_socket_fd, SIOCDEVPRIVATE, &ifr ) );
}


int wc_get_radius_stats( struct radius_report_stats * Stats )
{
    struct ifreq ifr;
    unsigned long args[4] = { 1111, 1009, 0, 0 };

    if( br_socket_fd < 0 )
        return -1;
    strncpy( ifr.ifr_name, BR_NAME, IFNAMSIZ-1 );
    args[2] = sizeof( int );                    /* Dummy length for GET only */
    args[3] = (unsigned long)Stats;
    ifr.ifr_data = (char *) &args;
    return( ioctl( br_socket_fd, SIOCDEVPRIVATE, &ifr ) );
}


int wc_local_mac( struct local_mac_set * Parms )
{
    struct ifreq ifr;
    unsigned long args[4] = { 1111, 1010, 0, 0 };

    if( br_socket_fd < 0 )
        return -1;
    strncpy( ifr.ifr_name, BR_NAME, IFNAMSIZ-1 );
    args[2] = sizeof( struct local_mac_set );
    args[3] = (unsigned long)Parms;
    ifr.ifr_data = (char *) &args;
    return( ioctl( br_socket_fd, SIOCDEVPRIVATE, &ifr ) );
}


int wc_get_capture( struct wc_capt_buf * Capture )
{
    struct ifreq ifr;
    unsigned long args[4] = { 1111, 1011, 0, 0 };

    if( br_socket_fd < 0 )
        return -1;
    strncpy( ifr.ifr_name, BR_NAME, IFNAMSIZ-1 );
    args[2] = sizeof( int );                    /* Dummy length for GET only */
    args[3] = (unsigned long)Capture;
    ifr.ifr_data = (char *) &args;
    return( ioctl( br_socket_fd, SIOCDEVPRIVATE, &ifr ) );
}


int wc_get_perf_stats( struct wc_perf_stats * PerfStats )
{
    struct ifreq ifr;
    unsigned long args[4] = { 1111, 1012, 0, 0 };

    if( br_socket_fd < 0 )
        return -1;
    strncpy( ifr.ifr_name, BR_NAME, IFNAMSIZ-1 );
    args[2] = sizeof( int );                    /* Dummy length for GET only */
    args[3] = (unsigned long)PerfStats;
    ifr.ifr_data = (char *) &args;
    return( ioctl( br_socket_fd, SIOCDEVPRIVATE, &ifr ) );
}


int wc_get_clt_info( struct clt_hash_info * CltInfo )
{
    struct ifreq ifr;
    unsigned long args[4] = { 1111, 1013, 0, 0 };

    if( br_socket_fd < 0 )
        return -1;
    strncpy( ifr.ifr_name, BR_NAME, IFNAMSIZ-1 );
    args[2] = 12;                               /* Should include Num and EntryPtr */
    args[3] = (unsigned long)CltInfo;
    ifr.ifr_data = (char *) &args;
    return( ioctl( br_socket_fd, SIOCDEVPRIVATE, &ifr ) );
}


int wc_get_mac_info( struct mac_hash_info * MacInfo )
{
    struct ifreq ifr;
    unsigned long args[4] = { 1111, 1014, 0, 0 };

    if( br_socket_fd < 0 )
        return -1;
    strncpy( ifr.ifr_name, BR_NAME, IFNAMSIZ-1 );
    args[2] = 12;                               /* Should include Num and EntryPtr */
    args[3] = (unsigned long)MacInfo;
    ifr.ifr_data = (char *) &args;
    return( ioctl( br_socket_fd, SIOCDEVPRIVATE, &ifr ) );
}


int wc_get_ngbrCapture( struct wc_ngbr_capt_buf * NgbrCapture )
{
    struct ifreq ifr;
    unsigned long args[4] = { 1111, 1015, 0, 0 };

    if( br_socket_fd < 0 )
        return -1;
    strncpy( ifr.ifr_name, BR_NAME, IFNAMSIZ-1 );
    args[2] = sizeof( int );                    /* Dummy length for GET only */
    args[3] = (unsigned long)NgbrCapture;
    ifr.ifr_data = (char *) &args;
    return( ioctl( br_socket_fd, SIOCDEVPRIVATE, &ifr ) );
}


int wc_register_iac( struct wc_iac_register * IacRegister )
{
    struct ifreq ifr;
    unsigned long args[4] = { 1111, 1016, 0, 0 };

    if( br_socket_fd < 0 )
        return -1;
    strncpy( ifr.ifr_name, BR_NAME, IFNAMSIZ-1 );
    args[2] = sizeof( struct wc_iac_register );
    args[3] = (unsigned long)IacRegister;
    ifr.ifr_data = (char *) &args;
    return( ioctl( br_socket_fd, SIOCDEVPRIVATE, &ifr ) );
}


int wc_send_iac( struct wc_iac_send * IacSend )
{
    struct ifreq ifr;
    unsigned long args[4] = { 1111, 1017, 0, 0 };

    if( br_socket_fd < 0 )
        return -1;
    strncpy( ifr.ifr_name, BR_NAME, IFNAMSIZ-1 );
    //args[2] = sizeof( struct wc_iac_send );
    args[2] = IacSend->MessLen + 3 * sizeof( unsigned int );
    args[3] = (unsigned long)IacSend;
    ifr.ifr_data = (char *) &args;
    return( ioctl( br_socket_fd, SIOCDEVPRIVATE, &ifr ) );
}


int wc_set_apc( struct wc_apc_spec * ApcSpec )
{
    struct ifreq ifr;
    unsigned long args[4] = { 1111, 1018, 0, 0 };
    int iii, Lcopy = 3 * sizeof( unsigned int );

    if( br_socket_fd < 0 )
        return -1;

    for( iii=0; iii<APC_MAX_NEIGHBORS; iii++ )
    {
        Lcopy += sizeof( struct wc_apc_neigh );
        if( ApcSpec->Neighbors[iii].Ip == 0 )
            break;
    }

    strncpy( ifr.ifr_name, BR_NAME, IFNAMSIZ-1 );
    //args[2] = sizeof( struct wc_apc_spec );
    args[2] = Lcopy;
    args[3] = (unsigned long)ApcSpec;
    ifr.ifr_data = (char *) &args;
    return( ioctl( br_socket_fd, SIOCDEVPRIVATE, &ifr ) );
}


int wc_get_apc( struct wc_apc_spec * ApcSpec )
{
    struct ifreq ifr;
    unsigned long args[4] = { 1111, 1019, 0, 0 };

    if( br_socket_fd < 0 )
        return -1;
    strncpy( ifr.ifr_name, BR_NAME, IFNAMSIZ-1 );
    args[2] = sizeof( int );                    /* Dummy length for GET only */
    args[3] = (unsigned long)ApcSpec;
    ifr.ifr_data = (char *) &args;
    return( ioctl( br_socket_fd, SIOCDEVPRIVATE, &ifr ) );
}


int wc_set_mac_ident( struct wc_mac_ident * MacIdent )
{
    struct ifreq ifr;
    unsigned long args[4] = { 1111, 1020, 0, 0 };

    if( br_socket_fd < 0 )
        return -1;
    strncpy( ifr.ifr_name, BR_NAME, IFNAMSIZ-1 );
    args[2] = sizeof( struct wc_mac_ident );
    args[3] = (unsigned long)MacIdent;
    ifr.ifr_data = (char *) &args;
    return( ioctl( br_socket_fd, SIOCDEVPRIVATE, &ifr ) );
}


int wc_get_mac_ident( struct wc_mac_ident * MacIdent )
{
    struct ifreq ifr;
    unsigned long args[4] = { 1111, 1021, 0, 0 };

    if( br_socket_fd < 0 )
        return -1;
    strncpy( ifr.ifr_name, BR_NAME, IFNAMSIZ-1 );
    args[2] = sizeof( struct wc_mac_ident );
    args[3] = (unsigned long)MacIdent;
    ifr.ifr_data = (char *) &args;
    return( ioctl( br_socket_fd, SIOCDEVPRIVATE, &ifr ) );
}


int wc_subnet_filter( struct wc_subnet_filter * Filter, int Mode )
{
    struct ifreq ifr;
    unsigned long args[4] = { 1111, 1022, 0, 0 };

    if( br_socket_fd < 0 )
        return -1;
    strncpy( ifr.ifr_name, BR_NAME, IFNAMSIZ-1 );
    args[2] = sizeof( struct wc_subnet_filter );
    args[3] = (unsigned long)Filter;
    if( !Mode )
        args[1] = 1023;
    ifr.ifr_data = (char *) &args;
    return( ioctl( br_socket_fd, SIOCDEVPRIVATE, &ifr ) );
}


int wc_send_dhcp_release( struct wc_dhcp_release * Release )
{
    struct ifreq ifr;
    unsigned long args[4] = { 1111, 1024, 0, 0 };

    if( br_socket_fd < 0 )
        return -1;
    strncpy( ifr.ifr_name, BR_NAME, IFNAMSIZ-1 );
    args[2] = sizeof( struct wc_dhcp_release );
    args[3] = (unsigned long)Release;
    ifr.ifr_data = (char *) &args;
    return( ioctl( br_socket_fd, SIOCDEVPRIVATE, &ifr ) );
}


int wc_get_eth_link_state( int * State )
{
    struct ifreq ifr;
    unsigned long args[4] = { 1111, 1025, 0, 0 };

    if( br_socket_fd < 0 )
        return -1;
    strncpy( ifr.ifr_name, BR_NAME, IFNAMSIZ-1 );
    args[2] = sizeof( int );
    args[3] = (unsigned long)State;
    ifr.ifr_data = (char *) &args;
    return( ioctl( br_socket_fd, SIOCDEVPRIVATE, &ifr ) );
}


int wc_add_cp_whitelist( struct wc_cp_whitelist * Entry )
{
    struct ifreq ifr;
    unsigned long args[4] = { 1111, 1027, 0, 0 };

    if( br_socket_fd < 0 )
        return -1;
    strncpy( ifr.ifr_name, BR_NAME, IFNAMSIZ-1 );
    args[2] = sizeof( struct wc_cp_whitelist );
    args[3] = (unsigned long)Entry;
    ifr.ifr_data = (char *) &args;
    return( ioctl( br_socket_fd, SIOCDEVPRIVATE, &ifr ) );
}


int wc_update_cp_whitelist( int * Flag )
{
    struct ifreq ifr;
    unsigned long args[4] = { 1111, 1028, 0, 0 };

    if( br_socket_fd < 0 )
        return -1;
    strncpy( ifr.ifr_name, BR_NAME, IFNAMSIZ-1 );
    args[2] = sizeof( int );
    args[3] = (unsigned long)Flag;
    ifr.ifr_data = (char *) &args;
    return( ioctl( br_socket_fd, SIOCDEVPRIVATE, &ifr ) );
}


int wc_change_log_control( int * enable )
{
    struct ifreq ifr;
    unsigned long args[4] = { 1111, 1030, 0, 0 };

    if( br_socket_fd < 0 )
        return -1;
    strncpy( ifr.ifr_name, BR_NAME, IFNAMSIZ-1 );
    args[2] = sizeof( int );
    args[3] = (unsigned long)enable;
    ifr.ifr_data = (char *) &args;
    return( ioctl( br_socket_fd, SIOCDEVPRIVATE, &ifr ) );
}


int wc_get_bss_upstream_stats( struct bss_upstream_stats * Stats )
{
    struct ifreq ifr;
    unsigned long args[4] = { 1111, 1032, 0, 0 };

    if( br_socket_fd < 0 )
        return -1;
    strncpy( ifr.ifr_name, BR_NAME, IFNAMSIZ-1 );
    args[2] = sizeof( int );                    /* Dummy length for GET only */
    args[3] = (unsigned long)Stats;
    ifr.ifr_data = (char *) &args;
    return( ioctl( br_socket_fd, SIOCDEVPRIVATE, &ifr ) );
}


int wc_send_arp( struct wc_arp_sweep * Arp )
{
    struct ifreq ifr;
    unsigned long args[4] = { 1111, 1033, 0, 0 };

    if( br_socket_fd < 0 )
        return -1;
    strncpy( ifr.ifr_name, BR_NAME, IFNAMSIZ-1 );
    args[2] = sizeof( struct wc_arp_sweep );
    args[3] = (unsigned long)Arp;
    ifr.ifr_data = (char *) &args;
    return( ioctl( br_socket_fd, SIOCDEVPRIVATE, &ifr ) );
}


int wc_get_arp_cache( struct wc_arp_entry * Arp )
{
    struct ifreq ifr;
    unsigned long args[4] = { 1111, 1034, 0, 0 };

    if( br_socket_fd < 0 )
        return -1;
    strncpy( ifr.ifr_name, BR_NAME, IFNAMSIZ-1 );
    args[2] = sizeof( struct wc_arp_entry );
    args[3] = (unsigned long)Arp;
    ifr.ifr_data = (char *) &args;
    return( ioctl( br_socket_fd, SIOCDEVPRIVATE, &ifr ) );
}


int wc_cp_mac_whitelist( struct local_mac_set * Parms )
{
    struct ifreq ifr;
    unsigned long args[4] = { 1111, 1035, 0, 0 };

    if( br_socket_fd < 0 )
        return -1;
    strncpy( ifr.ifr_name, BR_NAME, IFNAMSIZ-1 );
    args[2] = sizeof( struct local_mac_set );
    args[3] = (unsigned long)Parms;
    ifr.ifr_data = (char *) &args;
    return( ioctl( br_socket_fd, SIOCDEVPRIVATE, &ifr ) );
}


int wc_cp_mac_whitelist_erase( void )
{
    struct ifreq ifr;
    unsigned long args[4] = { 1111, 1036, 0, 0 };
    int Tmp = 0;

    if( br_socket_fd < 0 )
        return -1;
    strncpy( ifr.ifr_name, BR_NAME, IFNAMSIZ-1 );
    args[2] = sizeof( int );
    args[3] = (unsigned long)&Tmp;
    ifr.ifr_data = (char *) &args;
    return( ioctl( br_socket_fd, SIOCDEVPRIVATE, &ifr ) );
}


int wc_add_bj_forw_rule( struct bonjour_forw_rule * Entry )
{
    struct ifreq ifr;
    unsigned long args[4] = { 1111, 1037, 0, 0 };

    if( br_socket_fd < 0 )
        return -1;
    strncpy( ifr.ifr_name, BR_NAME, IFNAMSIZ-1 );
    args[2] = sizeof( struct bonjour_forw_rule );
    args[3] = (unsigned long)Entry;
    ifr.ifr_data = (char *) &args;
    return( ioctl( br_socket_fd, SIOCDEVPRIVATE, &ifr ) );
}


int wc_bj_rules_erase( void )
{
    struct ifreq ifr;
    unsigned long args[4] = { 1111, 1040, 0, 0 };
    int Tmp = 0;

    if( br_socket_fd < 0 )
        return -1;
    strncpy( ifr.ifr_name, BR_NAME, IFNAMSIZ-1 );
    args[2] = sizeof( int );
    args[3] = (unsigned long)&Tmp;
    ifr.ifr_data = (char *) &args;
    return( ioctl( br_socket_fd, SIOCDEVPRIVATE, &ifr ) );
}


int wc_get_rogue_ap_macs( struct rogue_ap_macs * Entry )
{
    struct ifreq ifr;
    unsigned long args[4] = { 1111, 1041, 0, 0 };

    if( br_socket_fd < 0 )
        return -1;
    strncpy( ifr.ifr_name, BR_NAME, IFNAMSIZ-1 );
    args[2] = 200 * sizeof( struct rogue_ap_macs );
    args[3] = (unsigned long)Entry;
    ifr.ifr_data = (char *) &args;
    return( ioctl( br_socket_fd, SIOCDEVPRIVATE, &ifr ) );
}


int wc_set_raw_packet_throttle( struct raw_pkt_throttle * Throttle )
{
    struct ifreq ifr;
    unsigned long args[4] = { 1111, 1042, 0, 0 };

    if( br_socket_fd < 0 )
        return -1;
    strncpy( ifr.ifr_name, BR_NAME, IFNAMSIZ-1 );
    args[2] = sizeof( struct raw_pkt_throttle );
    args[3] = (unsigned long)Throttle;
    ifr.ifr_data = (char *) &args;
    return( ioctl( br_socket_fd, SIOCDEVPRIVATE, &ifr ) );
}


int wc_get_ethports_stats( struct wc_ethports_stats * PortStats )
{
    struct ifreq ifr;
    unsigned long args[4] = { 1111, 1044, 0, 0 };

    if( br_socket_fd < 0 )
        return -1;
    strncpy( ifr.ifr_name, BR_NAME, IFNAMSIZ-1 );
    args[2] = sizeof( int );                    /* Dummy length for GET only */
    args[3] = (unsigned long)PortStats;
    ifr.ifr_data = (char *) &args;
    return( ioctl( br_socket_fd, SIOCDEVPRIVATE, &ifr ) );
}


int wc_upd_uaTimeout( struct ua_timeout_upd * upd )
{
    struct ifreq ifr;
    unsigned long args[4] = { 1111, 1045, 0, 0 };

    if( br_socket_fd < 0 )
        return -1;

    strncpy( ifr.ifr_name, BR_NAME, IFNAMSIZ-1 );
    args[2] = sizeof( struct ua_timeout_upd );
    args[3] = (unsigned long)upd;
    ifr.ifr_data = (char *) &args;
    return( ioctl( br_socket_fd, SIOCDEVPRIVATE, &ifr ) );
}


int wc_delete_client( struct delClient * client )
{
    struct ifreq ifr;
    unsigned long args[4] = { 1111, 1046, 0, 0 };

    if( br_socket_fd < 0 )
        return -1;

    strncpy( ifr.ifr_name, BR_NAME, IFNAMSIZ-1 );
    args[2] = sizeof( struct delClient );
    args[3] = (unsigned long)client;
    ifr.ifr_data = (char *) &args;
    return( ioctl( br_socket_fd, SIOCDEVPRIVATE, &ifr ) );
}


int wc_set_mesh_mode( int * Mode )
{
    struct ifreq ifr;
    unsigned long args[4] = { 1111, 1047, 0, 0 };

    if( br_socket_fd < 0 )
        return -1;
    strncpy( ifr.ifr_name, BR_NAME, IFNAMSIZ-1 );
    args[2] = sizeof( int );
    args[3] = (unsigned long)Mode;
    ifr.ifr_data = (char *) &args;
    return( ioctl( br_socket_fd, SIOCDEVPRIVATE, &ifr ) );
}


int wc_get_mesh_mode( int * Mode )
{
    struct ifreq ifr;
    unsigned long args[4] = { 1111, 1048, 0, 0 };

    if( br_socket_fd < 0 )
        return -1;
    strncpy( ifr.ifr_name, BR_NAME, IFNAMSIZ-1 );
    args[2] = sizeof( int );
    args[3] = (unsigned long)Mode;
    ifr.ifr_data = (char *) &args;
    return( ioctl( br_socket_fd, SIOCDEVPRIVATE, &ifr ) );
}


int wc_set_mesh_eth_protection( int * Mode )
{
    struct ifreq ifr;
    unsigned long args[4] = { 1111, 1049, 0, 0 };

    if( br_socket_fd < 0 )
        return -1;
    strncpy( ifr.ifr_name, BR_NAME, IFNAMSIZ-1 );
    args[2] = sizeof( int );
    args[3] = (unsigned long)Mode;
    ifr.ifr_data = (char *) &args;
    return( ioctl( br_socket_fd, SIOCDEVPRIVATE, &ifr ) );
}


int wc_set_eth_blocking( int * Block )
{
    struct ifreq ifr;
    unsigned long args[4] = { 1111, 1050, 0, 0 };

    /* Block=1 means block ethernet, zer0 - unblock */
    if( br_socket_fd < 0 )
        return -1;
    strncpy( ifr.ifr_name, BR_NAME, IFNAMSIZ-1 );
    args[2] = sizeof( int );
    args[3] = (unsigned long)Block;
    ifr.ifr_data = (char *) &args;
    return( ioctl( br_socket_fd, SIOCDEVPRIVATE, &ifr ) );
}


int wc_video_server_get( struct stream_srvr * Srvr )
{
    struct ifreq ifr;
    unsigned long args[4] = { 1111, 1052, 0, 0 };

    if( br_socket_fd < 0 )
        return -1;
    strncpy( ifr.ifr_name, BR_NAME, IFNAMSIZ-1 );
    args[2] = sizeof( int );                    /* Dummy length for GET only */
    args[3] = (unsigned long)Srvr;
    ifr.ifr_data = (char *) &args;
    return( ioctl( br_socket_fd, SIOCDEVPRIVATE, &ifr ) );
}


int wc_video_servers_clear( unsigned int * Type )
{
    struct ifreq ifr;
    unsigned long args[4] = { 1111, 1053, 0, 0 };

    if( br_socket_fd < 0 )
        return -1;
    strncpy( ifr.ifr_name, BR_NAME, IFNAMSIZ-1 );
    args[2] = sizeof( int );
    args[3] = (unsigned long)Type;
    ifr.ifr_data = (char *) &args;
    return( ioctl( br_socket_fd, SIOCDEVPRIVATE, &ifr ) );
}


int wc_video_server_set( struct stream_srvr * Srvr )
{
    struct ifreq ifr;
    unsigned long args[4] = { 1111, 1054, 0, 0 };

    if( br_socket_fd < 0 )
        return -1;
    strncpy( ifr.ifr_name, BR_NAME, IFNAMSIZ-1 );
    args[2] = sizeof( struct stream_srvr );
    args[3] = (unsigned long)Srvr;
    ifr.ifr_data = (char *) &args;
    return( ioctl( br_socket_fd, SIOCDEVPRIVATE, &ifr ) );
}


#if 0
int br_set_port_priority(const char *bridge, const char *port, int priority)
{
	return port_set(bridge, port, "priority", priority, BRCTL_SET_PORT_PRIORITY);
}

int br_set_path_cost(const char *bridge, const char *port, int cost)
{
	return port_set(bridge, port, "path_cost", cost, BRCTL_SET_PATH_COST);
}
#endif

static inline void __copy_fdb(struct fdb_entry *ent,
			      const struct __fdb_entry *f)
{
	memcpy(ent->mac_addr, f->mac_addr, 6);
	ent->port_no = f->port_no;
	ent->is_local = f->is_local;
	__jiffies_to_tv(&ent->ageing_timer_value, f->ageing_timer_value);
}

#if 0
int br_read_fdb(const char *bridge, struct fdb_entry *fdbs,
		unsigned long offset, int num)
{
	FILE *f;
	int i, n;
	struct __fdb_entry fe[num];
	char path[SYSFS_PATH_MAX];

	/* open /sys/class/net/brXXX/brforward */
	snprintf(path, SYSFS_PATH_MAX, SYSFS_CLASS_NET "%s/brforward", bridge);
	f = fopen(path, "r");
	if (f) {
		fseek(f, offset*sizeof(struct __fdb_entry), SEEK_SET);
		n = fread(fe, sizeof(struct __fdb_entry), num, f);
		fclose(f);
	} else {
		/* old kernel, use ioctl */
		unsigned long args[4] = { BRCTL_GET_FDB_ENTRIES,
					  (unsigned long) fe,
					  num, offset };
		struct ifreq ifr;
		int retries = 0;

		strncpy(ifr.ifr_name, bridge, IFNAMSIZ-1);
		ifr.ifr_data = (char *) args;

	retry:
		n = ioctl(br_socket_fd, SIOCDEVPRIVATE, &ifr);

		/* table can change during ioctl processing */
		if (n < 0 && errno == EAGAIN && ++retries < 10) {
			sleep(0);
			goto retry;
		}
	}

	for (i = 0; i < n; i++)
		__copy_fdb(fdbs+i, fe+i);

	return n;
}
#endif
