/*
Copyright (c) 2015, Plume Design Inc. All rights reserved.

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

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "os.h"
#include "const.h"
#include "log.h"
#include "os_nif.h"
#include "os_types.h"
#include "os_time.h"
#include "util.h"
#include "ds_list.h"
#include "os_util.h"
#include "os_regex.h"
#include "target.h"

#define MODULE_ID LOG_MODULE_ID_OSA

extern const char *app_build_number_get();
extern const char *app_build_profile_get();

os_ipaddr_t os_ipaddr_any =
{
    .addr = { 0, 0, 0, 0}
};

static int os_nif_ifreq(int cmd, char *ifname, struct ifreq *req);

/*
 * Returns true if the interface @p ifname exists
 */
bool os_nif_exists(char *ifname, bool *exists)
{
    struct ifreq    req;
    int             rc;

    (void)ifname;

    /*
     * Check if the device exists by retrieving the device index.
     * If this fails the device definitely does not exist.
     * */
    rc = os_nif_ifreq(SIOCGIFINDEX, ifname, &req);
    if (rc != 0)
    {
        *exists = false;
    }
    else
    {
        *exists = true;
    }

    return true;
}

/*
 * Retrieve the ip address of the interface @p ifname
 */
bool os_nif_ipaddr_get(char* ifname, os_ipaddr_t* addr)
{
    int             rc;
    struct ifreq    req;

    /* Requesting an internet address */
    req.ifr_addr.sa_family = AF_INET;

    rc = os_nif_ifreq(SIOCGIFADDR, ifname, &req);
    if (rc != 0)
    {
        LOG(DEBUG, "os_nif_ipaddr: SIOCGIFADDR failed.::ifname=%s", ifname);
        return false;
    }

    memcpy(addr,
            &((struct sockaddr_in *)&req.ifr_addr)->sin_addr.s_addr,
            sizeof(*addr));

    return true;
}

/*
 * Retrieve the netmask of the interface @p ifname
 */
bool os_nif_netmask_get(char* ifname, os_ipaddr_t* addr)
{
    int             rc;
    struct ifreq    req;

    /* Requesting an internet address */
    req.ifr_netmask.sa_family = AF_INET;

    rc = os_nif_ifreq(SIOCGIFNETMASK, ifname, &req);
    if (rc != 0)
    {
        LOG(DEBUG, "os_nif_ipaddr: SIOCGIFADDR failed::ifname=%s", ifname);
        return false;
    }

    memcpy(addr,
            &((struct sockaddr_in *)&req.ifr_netmask)->sin_addr.s_addr,
            sizeof(*addr));

    return true;
}

/*
 * Retrieve the broadcast address of the interface @p ifname
 */
bool os_nif_bcast_get(char* ifname, os_ipaddr_t* addr)
{
    int             rc;
    struct ifreq    req;

    /* Requesting an internet address */
    req.ifr_broadaddr.sa_family = AF_INET;

    rc = os_nif_ifreq(SIOCGIFBRDADDR, ifname, &req);
    if (rc != 0)
    {
        LOG(DEBUG, "os_nif_ipaddr: SIOCGIFADDR failed::ifname=%s", ifname);
        return false;
    }

    memcpy(addr,
            &((struct sockaddr_in *)&req.ifr_broadaddr)->sin_addr.s_addr,
            sizeof(*addr));

    return true;
}


/*
 * Set the ip address of the interface @p ifname
 */
bool os_nif_ipaddr_set(char* ifname, os_ipaddr_t addr)
{
    int             rc;
    struct ifreq    req;

    /* Requesting an internet address */
    req.ifr_addr.sa_family = AF_INET;

    memcpy(&((struct sockaddr_in *)&req.ifr_addr)->sin_addr.s_addr,
            &addr,
            sizeof(addr));

    rc = os_nif_ifreq(SIOCSIFADDR, ifname, &req);
    if (rc != 0)
    {
        LOG(DEBUG, "os_nif_ipaddr: SIOCGIFADDR failed::ifname=%s", ifname);
        return false;
    }

    return true;
}

/*
 * Set the netmask of the interface @p ifname
 */
bool os_nif_netmask_set(char* ifname, os_ipaddr_t addr)
{
    int             rc;
    struct ifreq    req;

    /* Requesting an internet address */
    req.ifr_netmask.sa_family = AF_INET;

    memcpy(&((struct sockaddr_in *)&req.ifr_netmask)->sin_addr.s_addr,
            &addr,
            sizeof(addr));

    rc = os_nif_ifreq(SIOCSIFNETMASK, ifname, &req);
    if (rc != 0)
    {
        LOG(DEBUG, "os_nif_ipaddr: SIOCGIFNETMASK failed::ifname=%s", ifname);
        return false;
    }

    return true;
}

/*
 * Set the broadcast address of the interface @p ifname
 */
bool os_nif_bcast_set(char* ifname, os_ipaddr_t addr)
{
    int             rc;
    struct ifreq    req;

    /* Requesting an internet address */
    req.ifr_broadaddr.sa_family = AF_INET;

    memcpy(&((struct sockaddr_in *)&req.ifr_broadaddr)->sin_addr.s_addr,
            &addr,
            sizeof(addr));

    rc = os_nif_ifreq(SIOCSIFBRDADDR, ifname, &req);
    if (rc != 0)
    {
        LOG(DEBUG, "os_nif_ipaddr: SIOCGIFBRDADDR failed::ifname=%s", ifname);
        return false;
    }

    return true;
}

/*
 * Get the MTU of the interface @p ifname
 */
bool os_nif_mtu_get(char* ifname, int *mtu)
{
    int             rc;
    struct ifreq    req;

    /* Requesting an internet address */
    rc = os_nif_ifreq(SIOCGIFMTU, ifname, &req);
    if (rc != 0)
    {
        LOG(DEBUG, "os_nif_ipaddr: SIOCGIFMTU failed::ifname=%s", ifname);
        return false;
    }

    *mtu = req.ifr_mtu;

    return true;
}

/*
 * Set the MTU of the interface @p ifname
 */
bool os_nif_mtu_set(char* ifname, int mtu)
{
    int             rc;
    struct ifreq    req;

    /* Requesting an internet address */
    req.ifr_mtu = mtu;

    rc = os_nif_ifreq(SIOCSIFMTU, ifname, &req);
    if (rc != 0)
    {
        LOG(DEBUG, "os_nif_ipaddr: SIOCSIFMTU failed::ifname=%s", ifname);
        return false;
    }

    return true;
}

/**
 * Retrieve the interface MAC address.
 */
bool os_nif_macaddr(char* ifname, os_macaddr_t* mac)
{
    int             rc;
    struct ifreq    req;

    /* Get the MAC(hardware) address */
    rc = os_nif_ifreq(SIOCGIFHWADDR, ifname, &req);
    if (rc != 0)
    {
        LOG(DEBUG, "os_nif_macaddr: SIOCGIFHWADDR failed::ifname=%s", ifname);
        return false;
    }

    /* Copy the address */
    memcpy(mac, req.ifr_addr.sa_data, sizeof(*mac));

    LOG(DEBUG, "os_nif_macaddr get::ifname=%s|mac=" PRI(os_macaddr_t), ifname, FMT(os_macaddr_t, *mac));

    return true;
}

bool os_nif_macaddr_get(char* ifname, os_macaddr_t* mac)
{
    return os_nif_macaddr(ifname, mac);
}

bool os_nif_macaddr_set(char *ifname, os_macaddr_t mac)
{
    int             rc;
    struct ifreq    req;

    req.ifr_hwaddr.sa_family = ARPHRD_ETHER;

    memcpy(req.ifr_hwaddr.sa_data, &mac, sizeof(mac));

    /* Set the MAC(hardware) address */
    rc = os_nif_ifreq(SIOCSIFHWADDR, ifname, &req);
    if (rc != 0)
    {
        LOG(DEBUG, "%s: SIOCSIFHWADDR failed::ifname=%s", __func__, ifname);
        return false;
    }

    LOG(DEBUG, "%s set::ifname=%s|mac=" PRI(os_macaddr_t), __func__, ifname, FMT(os_macaddr_t, mac));

    return true;
}

bool os_nif_gateway_set(char* ifname, os_ipaddr_t gwaddr)
{
    char iproute[128];

    snprintf(iproute, sizeof(iproute),
            "ip route add default via "PRI(os_ipaddr_t)" dev %s", FMT(os_ipaddr_t, gwaddr), ifname);

    return system(iproute) == 0;
}

bool os_nif_gateway_del(char* ifname, os_ipaddr_t gwaddr)
{
    char iproute[128];

    snprintf(iproute, sizeof(iproute),
            "ip route del default via "PRI(os_ipaddr_t)" dev %s", FMT(os_ipaddr_t, gwaddr), ifname);

    return system(iproute) == 0;
}


/**
 * Utility function to convert a string MAC address to os_macaddr_t type
 *
 * @retval  true     On success
 * @retval  false    On error
 */
bool os_nif_macaddr_from_str(os_macaddr_t* mac, const char* str)
{
    char    pstr[64];
    char*   mstr = NULL;
    char*   mtok = NULL;
    long    cnum = 0;
    int     cmak = 0;

    STRSCPY(pstr, str);

    mstr = pstr;
    while ((mtok = strsep(&mstr, ":-")) != NULL)
    {
        if (os_strtoul(mtok, &cnum, 16) != true)
        {
            return false;
        }

        /* Check if the parsed number is between 0 and 255 */
        if (cnum >= 256)
        {
            return false;
        }

        /* Check if we have more than 6 bytes */
        if (cmak >= 6)
        {
            return false;
        }

        mac->addr[cmak++] = cnum;
    }

    return true;
}

/**
 * Utility function to convert a os_macaddr_t type to string MAC address
 *
 * @param   mac[in]     MAC address
 * @param   str[out]    String MAC address
 * @param   format[in]  Desire MAC address format (PRI_os_macaddr_t, PRI_os_macaddr_lower_t, PRI_os_macaddr_plain_t)
 * @retval  true        On success
 * @retval  false       On error
 */
bool os_nif_macaddr_to_str(os_macaddr_t* mac, char* str, const char* format)
{

    if (!format && (sizeof(str) < OS_MACSTR_PLAIN_SZ)){
        LOG(ERR, "Convert mac addr to string (verify params)");
        return false;
    }

    int n = sprintf(str, format, FMT(os_macaddr_pt, mac));
    if (n < OS_MACSTR_PLAIN_SZ)
    {
        LOG(ERR, "Convert mac addr to string (write size)");
        return false;
    }

    return true;
}

/**
 * Utility function to convert a string IPv4 address to os_ipaddr_t type
 *
 * @retval  true     On success
 * @retval  false    On error
 */
bool os_nif_ipaddr_from_str(os_ipaddr_t *ipaddr, const char* str)
{
    return (inet_pton(AF_INET, str, ipaddr) == 1);
}

/**
 * Equivalent of ifconfig IF up/down
 */
bool os_nif_up(char* ifname, bool ifup)
{
    struct ifreq    req;
    int             rc;

    /* Get current flags */
    rc = os_nif_ifreq(SIOCGIFFLAGS, ifname, &req);
    if (rc != 0)
    {
        LOG(DEBUG, "os_nif_up: SIOCGIFFLAGS failed::ifname=%s", ifname);
        return false;
    }

    /* Set the interface UP bit */
    if (ifup)
    {
        req.ifr_flags |= IFF_UP;
    }
    else
    {
        req.ifr_flags &= ~IFF_UP;
    }

    /* Set new flags */
    rc = os_nif_ifreq(SIOCSIFFLAGS, ifname, &req);
    if (rc != 0)
    {
        LOG(DEBUG, "os_nif_up: SIOCSIFFLAGS failed::ifname=%s", ifname);
        return false;
    }

    return true;
}

/**
 * Returns @p true whether the device is UP
 */
bool os_nif_is_up(char* ifname, bool *up)
{
    struct ifreq    req;
    int             rc;

    *up = false;

    /* Get current flags */
    rc = os_nif_ifreq(SIOCGIFFLAGS, ifname, &req);
    if (rc != 0)
    {
        LOG(DEBUG, "os_nif_up: SIOCGIFFLAGS failed::ifname=%s", ifname);
        return false;
    }

    if (req.ifr_flags & IFF_UP)
    {
        *up = true;
    }

    return true;
}

/**
 * Returns @p true whether the device is UP and is operational (in case of Ethernet, if the cable is connected)
 */
bool os_nif_is_running(char* ifname, bool *running)
{
    struct ifreq    req;
    int             rc;

    *running = false;

    /* Get current flags */
    rc = os_nif_ifreq(SIOCGIFFLAGS, ifname, &req);
    if (rc != 0)
    {
        LOG(DEBUG, "os_nif_up: SIOCGIFFLAGS failed::ifname=%s", ifname);
        return false;
    }

    if (req.ifr_flags & IFF_RUNNING)
    {
        *running = true;
    }

    return true;
}

bool os_nif_softwds_create(
        char* ifname,
        char* parent,
        os_macaddr_t* mac,
        bool wrap)
{
    char cmd[1024];
    int rc;

    snprintf(cmd, sizeof(cmd),
             "ip link add link %s name %s type softwds && "
             "echo " PRI(os_macaddr_t) " > /sys/class/net/%s/softwds/addr && "
             "echo %c > /sys/class/net/%s/softwds/wrap",
             parent, ifname,
             FMT(os_macaddr_t, *mac), ifname,
             wrap ? 'Y' : 'N', ifname);
    rc = cmd_log(cmd);
    if (!WIFEXITED(rc) || WEXITSTATUS(rc) != 0) {
        LOG(ERR, "%s: Failed to create softwds link ('%s')", __func__, cmd);
        snprintf(cmd, sizeof(cmd), "ip link del %s", ifname);
        rc = cmd_log(cmd);
        if (!WIFEXITED(rc) || WEXITSTATUS(rc) != 0)
            LOG(ERR, "%s: Failed to clean up ('%s'), expect trouble", __func__, cmd);
        return false;
    }

    return true;
}

bool os_nif_softwds_destroy(
        char* ifname)
{
    char cmd[1024];
    int rc;

    snprintf(cmd, sizeof(cmd), "ip link del %s", ifname);
    rc = cmd_log(cmd);
    if (!WIFEXITED(rc) || WEXITSTATUS(rc) != 0) {
        LOG(ERR, "%s: Failed to destroy softwds link ('%s')", __func__, cmd);
        return false;
    }

    return true;
}

/**
 * Return a list of network interfaces
 *
 * This function uses /proc/net/dev to get the device list.
 */
bool os_nif_list_get(ds_list_t *list)
{
    struct os_nif_list_entry*   n;

    bool               retval = false;
    FILE*                       fnetdev = NULL;
    char                        buf[256];

    /*
     * Parse the netwokr device list from /proc/net/dev
     */
    static os_reg_list_t os_nif_list_net_dev[] =
    {
        OS_REG_LIST_ENTRY(1, "^[\t ]*([-a-zA-Z0-9]+):"),
        OS_REG_LIST_END(0)
    };

    /* Initialize the list */
    ds_list_init(list, struct os_nif_list_entry, le_node);

    fnetdev = fopen("/proc/net/dev", "r");
    if (fnetdev == NULL)
    {
        LOG(ERR, "os_nif_list_get: Error opening: /proc/net/dev");
        goto error;
    }

    while (fgets(buf, sizeof(buf), fnetdev) != NULL)
    {
        int         match;
        regmatch_t  rm[2];

        match = os_reg_list_match(os_nif_list_net_dev, buf, rm, ARRAY_LEN(rm));
        switch (match)
        {
            case 1:
                n = malloc(sizeof(struct os_nif_list_entry));
                if (n == NULL)
                {
                    LOG(ERR, "Error allocating space for new nif entry.");
                    goto error;
                }

                os_reg_match_cpy(n->le_ifname, sizeof(n->le_ifname), buf, rm[1]);

                ds_list_insert_head(list, n);

                break;

            case 0:
                break;
        }
    }

    retval = true;

error:
    if (fnetdev != NULL)
    {
        fclose(fnetdev);
    }

    if (retval != true)
    {
        os_nif_list_free(list);
    }

    return retval;
}

/**
 * Free a list that was returned by os_nif_list_get()
 */
void os_nif_list_free(ds_list_t* list)
{
    struct os_nif_list_entry*   n;
    ds_list_iter_t              iter;

    for (n = ds_list_ifirst(&iter, list);
         n != NULL;
         n = ds_list_inext(&iter))
    {
        ds_list_iremove(&iter);
        free(n);
    }
}

/**
 * Initiate an ioctl() interface request
 *
 * This function returns 0 on success or an errno-style code on error.
 */
int os_nif_ifreq(int cmd, char *ifname, struct ifreq *req)
{
    STRSCPY(req->ifr_name, ifname);

    return os_nif_ioctl(cmd, req);
}

/**
 * Declare this as private, as it is shared with the WIF code, but it's
 * not really for public use.
 */
int os_nif_ioctl(int cmd, void *buf)
{
    static int fd = -1;

    int rc;

    int retval = -1;

    if (fd < 0)
    {
        /*
         * Open an AF_INET socket
         */
        fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (fd < 0)
        {
            LOG(DEBUG, "os_nif_ioctl: socket() failed.");
            retval = errno;
            goto exit;
        }
    }

    /*
     * Do the IFCFG request
     */
    rc = ioctl(fd, cmd, buf);
    if (rc != 0)
    {
        retval = errno;
        goto exit;
    }

    retval = 0;

exit:
    return retval;
}

/**
 * Return the PID of the udhcpc client serving on interface @p ifname
 */
static int os_nif_dhcpc_pid(char *ifname)
{
    char pid_file[256];
    FILE *f;
    int pid;
    int rc;

    snprintf(pid_file, sizeof(pid_file), "/var/run/udhcpc-%s.pid", ifname);

    f = fopen(pid_file, "r");
    if (f == NULL) return 0;

    rc = fscanf(f, "%d", &pid);
    fclose(f);

    /* We should read exactly 1 element */
    if (rc != 1)
    {
        return 0;
    }

    if (kill(pid, 0) != 0)
    {
        return 0;
    }

    return pid;
}

bool os_nif_dhcpc_start(char* ifname, bool apply, int dhcp_time)
{
    char dhcp_vendor_class[TARGET_BUFF_SZ];
    char pidfile[256];
    char swver[256];
    char profile[256];
    char serial_opt[256];
    char serial_num[100];
    char sku_num[100];
    char hostname[256];
    char *udhcpc_s_option;
    char *pidname;
    char name[128];
    char paramT[256];
    pid_t pid;
    int status;
    int valuelen;
    int i;
    int len;
    const char *bnum;
    const char *bprofile;

    if (!apply) {
        snprintf(name, sizeof(name), "dryrun-%s", ifname);
        pidname = name;
    } else {
        pidname = ifname;
    }

    snprintf(paramT, sizeof(paramT), "%d", dhcp_time);

    pid = os_nif_dhcpc_pid(pidname);
    if (pid > 0)
    {
        LOG(ERR, "DHCP client already running::ifname=%s", ifname);
        return true;
    }

    /* read SERIAL number & SKU, inputs for options 12 & 0xe */
    target_serial_get(serial_num, sizeof(serial_num));

    /* read SKU number, if empty, reset buffer */
    if (false == target_sku_get(sku_num, sizeof(sku_num)))
    {
        snprintf(hostname, sizeof(hostname), "hostname:%s_Pod", serial_num);
    }
    else
    {
        snprintf(hostname, sizeof(hostname), "hostname:%s_Pod_%s", serial_num, sku_num);
    }

    snprintf(pidfile, sizeof(pidfile), "/var/run/udhcpc-%s.pid", pidname);
    snprintf(swver, sizeof(swver), "0xe1:");
    snprintf(profile, sizeof(profile), "0xe2:");
    snprintf(serial_opt, sizeof(serial_opt), "0xe3:");
    if (apply == true) {
	udhcpc_s_option = "/usr/plume/bin/udhcpc.sh";
    } else {
	udhcpc_s_option = "/usr/plume/bin/udhcpc-dryrun.sh";
    }

    /* it looks that udhcpc doesn't support sending string  */
    /* we have to convert it to hexdump */
    /* option 225 - build number    */
    bnum = app_build_number_get();
    valuelen = strlen(bnum);
    for (i = 0; i < valuelen; i++)
    {
        len = strlen(swver);
        snprintf(swver + len, sizeof(swver) - len, "%02X", bnum[i]);
    }

    /* option 225 - profile name */
    bprofile = app_build_profile_get();
    valuelen = strlen(bprofile);
    for (i = 0; i < valuelen; i++)
    {
        len = strlen(profile);
        snprintf(profile + len, sizeof(profile) - len, "%02X", bprofile[i]);
    }

    /* option 227 - serial number */
    valuelen = strlen(serial_num);
    for (i = 0; i < valuelen; i++)
    {
        len = strlen(serial_opt);
        snprintf(serial_opt + len, sizeof(serial_opt) - len, "%02X", serial_num[i]);
    }

    if (false == target_model_get(dhcp_vendor_class, sizeof(dhcp_vendor_class)))
    {
        strcpy(dhcp_vendor_class, TARGET_NAME);
    }

    char *argv_apply[] = {
	"/sbin/udhcpc",
	"-p", pidfile,
	"-s", udhcpc_s_option,
	"-b",
	"-t", "60",
	"-T", paramT,
	"-o",
	"-O", "1",
	"-O", "3",
	"-O", "6",
	"-O", "15",
	"-O", "28",
	"-O", "43",
	"-i", ifname,
	"-S",
	"--vendorclass", dhcp_vendor_class,
	"-x", hostname,
	"-x", swver,
	"-x", profile,
	"-x", serial_opt,
#ifndef CONFIG_UDHCPC_OPTIONS_USE_CLIENTID
	"-C",
#endif
	NULL
    };

/* -T,--timeout SEC - Pause between packets (default 3) */
    char *argv_dry_run[] = {
	"/sbin/udhcpc",
 	"-p", pidfile,
	"-n",
	"-t", "5",
	"-T", paramT,
	"-A", "2",
	"-f",
	"-i", ifname,
	"-s", udhcpc_s_option,
#ifndef CONFIG_UDHCPC_OPTIONS_USE_CLIENTID
	"-C",
#endif
	"-S",
	"-q",
	NULL
    };

    /* Double fork -- disown the process */
    pid = fork();
    if (pid == 0)
    {
        if (fork() == 0)
        {
	    LOGI("%s: %%s option %s", __func__, udhcpc_s_option);
	    if (apply == true) {
		execv("/sbin/udhcpc", argv_apply);
	    } else {
		execv("/sbin/udhcpc", argv_dry_run);
	    }
        }
        exit(0);
    }

    /* Wait for the first child -- it should exit immediately */
    waitpid(pid, &status, 0);

    return true;
}

bool os_nif_dhcpc_stop(char* ifname, bool dryrun)
{
    char *pidname;
    char name[128];

    if (dryrun) {
        snprintf(name, sizeof(name), "dryrun-%s", ifname);
        pidname = name;
    }
    else
        pidname = ifname;

    int pid = os_nif_dhcpc_pid(pidname);
    if (pid <= 0)
    {
        LOG(DEBUG, "DHCP client not running::ifname=%s", ifname);
        return true;
    }

    int signum = SIGTERM;
    int tries = 0;

    while (kill(pid, signum) == 0)
    {
        if (tries++ > 20)
        {
            signum = SIGKILL;
        }

        usleep(100*1000);
    }

    return true;
}

bool os_nif_dhcpc_refresh_lease(char* ifname)
{
    int  pid;
    int  ret;

    pid = os_nif_dhcpc_pid(ifname);
    if (pid <= 0)
    {
        LOG(DEBUG, "DHCP client not running::ifname=%s", ifname);
        return true;
    }

    ret = kill(pid, SIGUSR1);
    return (ret == 0) ? true : false;
}

void closefrom(int fd)
{
    int ii;
    int maxfd;

    maxfd = sysconf(_SC_OPEN_MAX);
    for (ii = fd; ii < maxfd; ii++) close(ii);
}

void devnull(int fd)
{
    int nfd;

    nfd = open("/dev/null", O_RDWR);
    if (fd < 0)
    {
        LOG(WARNING, "Unable to open '/dev/null'.");
        return;
    }

    dup2(nfd, fd);
    close(nfd);
}

/**
 * Retrieve the PID associated with the PPPoE service running on interface @p ifname.
 * Return 0 if there's no service running or in case of an error
 */
pid_t os_nif_pppoe_pidof(const char *ifname)
{
    pid_t pid;
    char pid_path[128];

    snprintf(pid_path, sizeof(pid_path), "/var/run/ppp-%s.pid", ifname);

    pid = os_pid_from_file(pid_path);
    if (pid <= 0) return 0;

    /* Check if the process is alive */
    if (kill(pid, 0) != 0) return 0;

    return pid;
}

/**
 * Start PPPoE service on the interface @p ifparent -- the newly created interface will be @p ifname
 */
bool os_nif_pppoe_start(
        const char *ifname,
        const char *ifparent,
        const char *username,
        const char *password)
{
    pid_t cpid;
    char nic_ifparent[C_IFNAME_LEN];

    /*
     * Add the nic- parameter to the parent interface -- this ensures that pppd understands it as an ethernet interface
     * and not as a TTY device
     */
    snprintf(nic_ifparent, sizeof(nic_ifparent), "nic-%s", ifparent);

    const char *args[] =
    {
        "/usr/sbin/pppd",
        "ifname", ifname,       /* name of PPP interface */
        "linkname", ifname,     /* Create the PID file; it will be named ppp-linkname.pid (in /var/run) */
        "nodetach",             /* Do not detach into background -- we're taking of that in the double fork below */
        "ipparam", "wan",       /* May not be needed -- this parameter is sent to the UP/DOWN scripts */
#if 1
        "nodefaultroute",       /* Do not install default route to this interface */
#else
        "defaultroute",
#endif
        "usepeerdns",
        "persist",              /* Keep conncting */
        "maxfail", "0",         /* Connect retries -- unlimited */
        "user", username,
        "password", password,
        "ip-up-script", "/usr/plume/bin/ppp-up.sh",
        "ip-down-script", "/usr/plume/bin/ppp-down.sh",
#if 0
        "mtu", "1492",
        "mru", "1492",
#endif
        "noccp",                /* Disable CCP (compression) negotiation -- compatibility? */
        "plugin", "rp-pppoe.so", nic_ifparent,
        NULL
    };

    /* Double fork PPPD, so it gets detached from the main process */
    cpid = fork();
    if (cpid == 0)
    {
        if (fork() == 0)
        {
            /* Close all file descriptors */
            devnull(0);
            devnull(1);
            devnull(2);

            closefrom(3);
            /* Spawn the ppp daemon */
            execv("/usr/sbin/pppd", (char *const*)args);

            exit(1);
        }
        exit(0);
    }

    /* Avoid zombies */
    int status;
    waitpid(cpid, &status, 0);

    double val = 0.0;

    /*
     * Wait until the PID file is created
     */
    while(true)
    {
        if (clock_tonce(&val, 5.0))
        {
            LOG(ERR, "netif(pppoe): PID file for interface %s was not created. pppd failed?", ifname);
            return false;
        }

        if (os_nif_pppoe_pidof(ifname) > 0) break;

        clock_sleep(0.25);
    }

    return true;
}

/**
 * Stop the PPPoE service running on interface @p ifname
 */
bool os_nif_pppoe_stop(const char *ifname)
{
    pid_t pid = os_nif_pppoe_pidof(ifname);

    if (pid <= 0) return false;

    if (!os_pid_terminate(pid, 1000))
    {
        LOG(ERR, "netif(pppoe): Unable to kill PPPoE service for %s.", ifname);
        return false;
    }

    return true;
}

bool    os_nif_is_interface_ready(char *if_name)
{
    bool    exists = false;

    /* The interface needs to be both created and operational */
    if (os_nif_exists(if_name, &exists) != true)
    {
        return false;
    }

    if (!exists)
    {
        return false;
    }

    if (os_nif_is_running(if_name, &exists) != true)
    {
        return false;
    }

    if (!exists)
    {
        return false;
    }

    return true;

}
