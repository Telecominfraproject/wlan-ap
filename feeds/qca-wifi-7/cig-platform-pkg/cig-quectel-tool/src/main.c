/*
    Copyright 2025 Quectel Wireless Solutions Co.,Ltd

    Quectel hereby grants customers of Quectel a license to use, modify,
    distribute and publish the Software in binary form provided that
    customers shall have no right to reverse engineer, reverse assemble,
    decompile or reduce to source code form any portion of the Software. 
    Under no circumstances may customers modify, demonstrate, use, deliver 
    or disclose any portion of the Software in source code form.
*/

#include "QMIThread.h"
#include <sys/wait.h>
#include <sys/utsname.h>
#include <sys/time.h>
#include <dirent.h>

#include "util.h"

static PROFILE_T s_profile;
int debug_qmi = 0;
int qmidevice_control_fd[2];
static int signal_control_fd[2];
#ifdef USE_IPC_MSG_STATUS_IND
static int qcm_status_indication_fd[2];
#endif
int g_donot_exit_when_modem_hangup = 0;
extern int ql_ifconfig(int argc, char *argv[]);
extern int ql_get_netcard_driver_info(const char*);
extern int ql_capture_usbmon_log(PROFILE_T *profile, const char *log_path);
extern void ql_stop_usbmon_log(PROFILE_T *profile);

extern int cdc_wdm_fd;
static int s_link = -1;
static void usbnet_link_change(int link, PROFILE_T *profile) {
    if (s_link == link)
        return;

    s_link = link;

    if (!(link & (1<<IpFamilyV4)))
        memset(&profile->ipv4, 0, sizeof(IPV4_T));

    if (!(link & (1<<IpFamilyV6)))
        memset(&profile->ipv6, 0, sizeof(IPV6_T));

    if (link) {
        udhcpc_start(profile);
    } else {
        udhcpc_stop(profile);
    }
}

static int check_ipv4_address(PROFILE_T *profile) {
    uint32_t oldAddress = profile->ipv4.Address;

    if (profile->request_ops == &mbim_request_ops)
        return 1; //we will get a new ipv6 address per requestGetIPAddress()
    if (profile->request_ops == &atc_request_ops) {
       if (!profile->udhcpc_ip) return 1;
       oldAddress = profile->udhcpc_ip;
    }

    if (profile->request_ops->requestGetIPAddress(profile, IpFamilyV4) == 0) {
         if (profile->ipv4.Address != oldAddress || debug_qmi) {
             unsigned char *l = (unsigned char *)&oldAddress;
             unsigned char *r = (unsigned char *)&profile->ipv4.Address;
             dbg_time("localIP: %d.%d.%d.%d VS remoteIP: %d.%d.%d.%d",
                     l[3], l[2], l[1], l[0], r[3], r[2], r[1], r[0]);
        }
        return (profile->ipv4.Address == oldAddress);
    }

    return 0;
}

static void main_send_event_to_qmidevice(int triger_event) {
     if (write(qmidevice_control_fd[0], &triger_event, sizeof(triger_event)) == -1) {};
}

static void send_signo_to_main(int signo) {
     if (write(signal_control_fd[0], &signo, sizeof(signo)) == -1) {};
}

#ifdef USE_IPC_MSG_STATUS_IND
static void main_send_status_to_fifo(uint8_t status) {
     if (write(qcm_status_indication_fd[0], &status, sizeof(status)) == -1) {};
}
#endif

void qmidevice_send_event_to_main(int triger_event) {
     if (write(qmidevice_control_fd[1], &triger_event, sizeof(triger_event)) == -1) {};
}

void qmidevice_send_event_to_main_ext(int triger_event, void *data, unsigned len) {
     if (write(qmidevice_control_fd[1], &triger_event, sizeof(triger_event)) == -1) {};
     if (write(qmidevice_control_fd[1], data, len) == -1) {};
}

#ifdef USE_IPC_MSG_STATUS_IND
static int msg_get()
{
    key_t key = ftok(MSG_FILE, 'a');
    int msgid = msgget(key, IPC_CREAT | 0644);

    if (msgid < 0)
    {
        dbg_time("msgget fail: key %d, %s", key, strerror(errno));
        return -1;
    }
    return msgid;
}

static int msg_rm(int msgid)
{
    return msgctl(msgid, IPC_RMID, 0);
}

static int msg_send(int msgid, long type, const char *msg)
{
    struct message info;
    info.mtype = type;
    snprintf(info.mtext, MSGBUFFSZ, "%s", msg);
    if (msgsnd(msgid, (void *)&info, MSGBUFFSZ, IPC_NOWAIT) < 0)
    {
        dbg_time("msgsnd faild: msg %s, %s", msg, strerror(errno));
        return -1;
    }
    return 0;
}

void* qcm_status_thread(void *arg)
{
    uint8_t status = 0;
    int msgid = -1;
    char buf[MSGBUFFSZ] = {0};

    uint8_t* tmp = arg;
    if (tmp == NULL)
    {
        dbg_time("%s qcm start report status", __func__);
    }

    if (access(MSG_FILE, F_OK))
    {
        dbg_time("%s %s isn't exit", __func__, MSG_FILE);
        pthread_exit(NULL);
        return (void*)0;
    }
    else
    {
        msgid = msg_get();
        if (msgid < 0)
        {
            dbg_time("%s msg_get fail", __func__);
            pthread_exit(NULL);
            return (void*)0;
        }
    }

    while (1)
    {
        struct pollfd pollfds[] = {{qcm_status_indication_fd[1], POLLIN, 0}};
        int ret;

        do
        {
            ret = poll(pollfds, 1,  -1);
        } while ((ret < 0) && (errno == EINTR));

        if (ret <= 0) {
            dbg_time("%s poll=%d, errno: %d (%s)", __func__, ret, errno, strerror(errno));
            break;
        }

        if (pollfds[0].revents & (POLLERR | POLLHUP | POLLNVAL))
        {
            dbg_time("%s poll err/hup", __func__);
            dbg_time("epoll pollfds[0].fd = %d, pollfds[0].events = 0x%04x", pollfds[0].fd, pollfds[0].revents);
            break;
        }

        if ((pollfds[0].revents & POLLIN) == 0)
        {
            continue;
        }

        if (read(pollfds[0].fd, &status, sizeof(status)) == sizeof(status))
        {
            memset(buf, 0, sizeof(buf));
            snprintf(buf, sizeof(buf), "%d", status);

            if (msg_send(msgid, MSG_TYPE_IPC, buf) >= 0)
            {
                //dbg_time("%s msg_send success msgid = %d  status = %d", __func__, msgid, status);
            }
            else
            {
                dbg_time("%s msg_send fail", __func__);
                break;
            }

            if (status == QCM_PROCESS_EXIT)
            {
                dbg_time("%s qcm process exit", __func__);
                usleep(100000);
                break;
            }
        }
        else
        {
            dbg_time("%s read fail", __func__);
            break;
        }
    }

    if (msg_rm(msgid) < 0)
    {
        dbg_time("%s msg_rm fail", __func__);
    }

    dbg_time("%s exit", __func__);
    pthread_exit(NULL);
    return (void*)0;
}
#endif

#define MAX_PATH 256

static int ls_dir(const char *dir, int (*match)(const char *dir, const char *file, void *argv[]), void *argv[])
{
    DIR *pDir;
    struct dirent* ent = NULL;
    int match_times = 0;

    pDir = opendir(dir);
    if (pDir == NULL)  {
        dbg_time("Cannot open directory: %s, errno: %d (%s)", dir, errno, strerror(errno));
        return 0;
    }

    while ((ent = readdir(pDir)) != NULL)  {
        match_times += match(dir, ent->d_name, argv);
    }
    closedir(pDir);

    return match_times;
}

static int is_same_linkfile(const char *dir, const char *file,  void *argv[])
{
    const char *qmichannel = (const char *)argv[1];
    char linkname[MAX_PATH*2+6];
    char filename[MAX_PATH];
    int linksize;

    snprintf(linkname, sizeof(linkname), "%.256s/%s", dir, file);
    linksize = readlink(linkname, filename, sizeof(filename));
    if (linksize <= 0)
        return 0;

    filename[linksize] = 0;
    if (strcmp(filename, qmichannel))
        return 0;

    dbg_time("%s -> %s", linkname, filename);
    return 1;
}

static int is_brother_process(const char *dir, const char *file, void *argv[])
{
    char linkname[MAX_PATH*2+6];
    char filename[MAX_PATH];
    int linksize;
    int i = 0, kill_timeout = 15;
    pid_t pid;

    //dbg_time("%s", file);
    while (file[i]) {
        if (!isdigit(file[i]))
            break;
        i++;
    }

    if (file[i]) {
        //dbg_time("%s not digit", file);
        return 0;
    }

    snprintf(linkname, sizeof(linkname), "%s/%s/exe", dir, file);
    linksize = readlink(linkname, filename, sizeof(filename));
    if (linksize <= 0)
        return 0;

    filename[linksize] = 0;

    pid = atoi(file);
    if (pid >= getpid())
        return 0;

    snprintf(linkname, sizeof(linkname), "%s/%s/fd", dir, file);
    if (!ls_dir(linkname, is_same_linkfile, argv))
        return 0;

    dbg_time("%s/%s/exe -> %s", dir, file, filename);
    while (kill_timeout-- && !kill(pid, 0))
    {
        kill(pid, SIGTERM);
        sleep(1);
    }
    if (!kill(pid, 0))
    {
        dbg_time("force kill %s/%s/exe -> %s", dir, file, filename);
        kill(pid, SIGKILL);
        sleep(1);
    }

    return 1;
}

static int kill_brothers(const char *qmichannel)
{
    char myself[MAX_PATH];
    int filenamesize;
    void *argv[2] = {myself, (void *)qmichannel};

    filenamesize = readlink("/proc/self/exe", myself, sizeof(myself));
    if (filenamesize <= 0)
        return 0;
    myself[filenamesize] = 0;

    if (ls_dir("/proc", is_brother_process, argv))
        sleep(1);

    return 0;
}

static int kill_data_call_pdp(int pdp, char *self) {
    int pid;
    char *p = NULL;

    p = self;
    while (*self) {
        if (*self == '/')
            p = self+1;
        self++;
    }

    pid = getpid_by_pdp(pdp, p);
    if (pid > 0) {
        dbg_time("send SIGINT to process %d", pid);
        return kill(pid, SIGINT);
    }

    return -1;
}

static void ql_sigaction(int signo) {
     if (SIGALRM == signo)
         send_signo_to_main(SIG_EVENT_START);
     else
     {
        g_donot_exit_when_modem_hangup = 0;
        send_signo_to_main(SIG_EVENT_STOP);
        main_send_event_to_qmidevice(SIG_EVENT_STOP); //main may be wating qmi response
    }
}

static int usage(const char *progname) {
    dbg_time("Usage: %s [options]", progname);
    dbg_time("-s [apn [user password auth]]          Set apn/user/password/auth get from your network provider. auth: 1~pap, 2~chap, 3~MsChapV2");
    dbg_time("-p pincode                             Verify sim card pin if sim card is locked");
    dbg_time("-p [quectel-][qmi|mbim]-proxy          Request to use proxy");
    dbg_time("-f logfilename                         Save log message of this program to file");
    dbg_time("-u usbmonlog filename                  Save usbmon log to file");
    dbg_time("-i interface                           Specify which network interface to setup data call when multi-modems exits");
    dbg_time("-o interface                           Specify which logical link network interface to setup ip");
    dbg_time("-4                                     Setup IPv4 data call (default)");
    dbg_time("-6                                     Setup IPv6 data call");
    dbg_time("-n pdn                                 Specify which pdn to setup data call (default 1 for QMI, 0 for MBIM)");
    dbg_time("-k pdn                                 Specify which pdn to hangup data call (by send SIGINT to 'quectel-CM -n pdn')");
    dbg_time("-m iface-idx                           Bind QMI data call to wwan0_<iface idx> when QMAP used. E.g '-n 7 -m 1' bind pdn-7 data call to wwan0_1");
    dbg_time("-b                                     Enable network interface bridge function (default 0)");
    dbg_time("-v                                     Verbose log mode, for debug purpose.");
    dbg_time("-d                                     Obtain the IP address and dns through qmi");
    dbg_time("-a                                     1:Device attempts to bring up a call with the APN name,if -a 1 need add -s apn_name ;2:Device attempts to bring up a call with the APN type,if -a 2 need add -y apn_type");
    dbg_time("-y                                     Set APN type 0:APN type unspecified 1:internet traffic. 2:IMS");
    dbg_time("[Examples]");
    dbg_time("Example 1: %s ", progname);
    dbg_time("Example 2: %s -s 3gnet ", progname);
    dbg_time("Example 3: %s -s 3gnet carl 1234 1 -p 1234 -f gobinet_log.txt", progname);
    return 0;
}

static int qmi_main(PROFILE_T *profile)
{
    int triger_event = 0;
    int signo;
#ifdef CONFIG_SIM
    SIM_Status SIMStatus = SIM_ABSENT;
#endif
    UCHAR PSAttachedState = 0;
    UCHAR  IPv4ConnectionStatus = QWDS_PKT_DATA_UNKNOW;
    UCHAR  IPv6ConnectionStatus = QWDS_PKT_DATA_UNKNOW; 
    unsigned SetupCallFail = 0;
    unsigned long SetupCallAllowTime = clock_msec();
#ifdef REBOOT_SIM_CARD_WHEN_LONG_TIME_NO_PS
    unsigned PsAttachFail = 0;
    unsigned long PsAttachTime = clock_msec();
#endif
    int qmierr = 0;
    const struct request_ops *request_ops = profile ->request_ops;
    pthread_t gQmiThreadID = 0;
#ifdef USE_IPC_MSG_STATUS_IND
    pthread_t gQcmStatusThreadID = 0;
#endif

    if (profile->reattach_flag) {
        if (!reattach_driver(profile)) 
            sleep(2);
    }

    g_donot_exit_when_modem_hangup = 0;

    /* try to recreate FDs*/
    if (socketpair( AF_LOCAL, SOCK_STREAM, 0, signal_control_fd) < 0 ) {
        dbg_time("%s Faild to create main_control_fd: %d (%s)", __func__, errno, strerror(errno));
        return -1;
    }

    if ( socketpair( AF_LOCAL, SOCK_STREAM, 0, qmidevice_control_fd ) < 0 ) {
        dbg_time("%s Failed to create thread control socket pair: %d (%s)", __func__, errno, strerror(errno));
        return 0;
    }

#ifdef USE_IPC_MSG_STATUS_IND
    if ( socketpair( AF_LOCAL, SOCK_STREAM, 0, qcm_status_indication_fd ) < 0 ) {
        dbg_time("%s Failed to create thread qcm_status_indication_fd socket pair: %d (%s)", __func__, errno, strerror(errno));
        return 0;
    }
#endif

    if ((profile->qmap_mode == 0 || profile->qmap_mode == 1)
        && (!profile->proxy[0] || strstr(profile->qmichannel, "_IPCR"))) {
        kill_brothers(profile->qmichannel);
    }

#ifdef USE_IPC_MSG_STATUS_IND
    if (pthread_create( &gQcmStatusThreadID, 0, qcm_status_thread, NULL) != 0) {
        dbg_time("%s Failed to create QMIThread: %d (%s)", __func__, errno, strerror(errno));
        return 0;
    }
#endif

    if (pthread_create( &gQmiThreadID, 0, profile->qmi_ops->read, (void *)profile) != 0) {
        dbg_time("%s Failed to create QMIThread: %d (%s)", __func__, errno, strerror(errno));
        return 0;
    }

    if ((read(qmidevice_control_fd[0], &triger_event, sizeof(triger_event)) != sizeof(triger_event))
        || (triger_event != RIL_INDICATE_DEVICE_CONNECTED)) {
        dbg_time("%s Failed to init QMIThread: %d (%s)", __func__, errno, strerror(errno));
        return 0;
    }

    if (profile->qmi_ops->init && profile->qmi_ops->init(profile)) {
        dbg_time("%s Failed to qmi init: %d (%s)", __func__, errno, strerror(errno));
        return 0;
    }

#ifdef USE_IPC_MSG_STATUS_IND
    main_send_status_to_fifo(QCM_PARA_CONFIG_QUERY);
#endif

    if (request_ops->requestBaseBandVersion)
        request_ops->requestBaseBandVersion(profile);

    if (request_ops->requestSetEthMode)
        request_ops->requestSetEthMode(profile);

    if (request_ops->requestSetLoopBackState && profile->loopback_state) {
    	qmierr = request_ops->requestSetLoopBackState(profile->loopback_state, profile->replication_factor);
    	if (qmierr != QMI_ERR_INVALID_QMI_CMD) //X20 return this error 
            profile->loopback_state = 0; //wait for RIL_UNSOL_LOOPBACK_CONFIG_IND
    }

    if (request_ops->requestGetSIMStatus) {
        qmierr = request_ops->requestGetSIMStatus(&SIMStatus);

        while (qmierr == QMI_ERR_OP_DEVICE_UNSUPPORTED) {
            sleep(1);
            qmierr = request_ops->requestGetSIMStatus(&SIMStatus);
        }

        if ((SIMStatus == SIM_PIN) && profile->pincode && request_ops->requestEnterSimPin) {
            request_ops->requestEnterSimPin(profile->pincode);
        }
    }

    if (SIMStatus == SIM_READY) {
        if (request_ops->requestGetICCID)
            request_ops->requestGetICCID();

        if (request_ops->requestGetIMSI)
            request_ops->requestGetIMSI();
    }
if(profile->usb_dev.idProduct != 0x0316)
{
    if (request_ops->requestGetProfile)
        request_ops->requestGetProfile(profile);

    if (request_ops->requestSetProfile && (profile->apn || profile->user || profile->pd)) {
        if (request_ops->requestSetProfile(profile) == 1) {
#ifdef REBOOT_SIM_CARD_WHEN_APN_CHANGE //enable at only when customer asked 
            if (request_ops->requestRadioPower) {
                request_ops->requestRadioPower(0);
                request_ops->requestRadioPower(1);
            }
#endif
        }
    }
}
    request_ops->requestRegistrationState(&PSAttachedState);

#ifdef CONFIG_ENABLE_QOS
    request_ops->requestRegisterQos(profile);
#endif

    //USB disconnnect and re-connect, but not reboot modem, will get this bug
    if (profile->enable_ipv4
        && profile->request_ops == &atc_request_ops
        && !request_ops->requestQueryDataCall(&IPv4ConnectionStatus, IpFamilyV4)
        && IPv4ConnectionStatus == QWDS_PKT_DATA_CONNECTED) {
        request_ops->requestDeactivateDefaultPDP(profile, IpFamilyV4);
    }

    send_signo_to_main(SIG_EVENT_CHECK);

    while (1)
    {
        struct pollfd pollfds[] = {{signal_control_fd[1], POLLIN, 0}, {qmidevice_control_fd[0], POLLIN, 0}};
        int ne, ret, nevents = sizeof(pollfds)/sizeof(pollfds[0]);

        do {
            ret = poll(pollfds, nevents,  15*1000);
        } while ((ret < 0) && (errno == EINTR));

        if (ret == 0)
        {
            send_signo_to_main(SIG_EVENT_CHECK);
            continue;
        }

        if (ret <= 0) {
            dbg_time("%s poll=%d, errno: %d (%s)", __func__, ret, errno, strerror(errno));
            goto __main_quit;
        }

        for (ne = 0; ne < nevents; ne++) {
            int fd = pollfds[ne].fd;
            short revents = pollfds[ne].revents;

            if (revents & (POLLERR | POLLHUP | POLLNVAL)) {
                dbg_time("%s poll err/hup", __func__);
                dbg_time("epoll fd = %d, events = 0x%04x", fd, revents);
                main_send_event_to_qmidevice(RIL_REQUEST_QUIT);
#ifdef USE_IPC_MSG_STATUS_IND
                main_send_status_to_fifo(QCM_PROCESS_EXIT);
#endif
                if (revents & POLLHUP)
                    goto __main_quit;
            }

            if ((revents & POLLIN) == 0)
                continue;

            if (fd == signal_control_fd[1])
            {
                if (read(fd, &signo, sizeof(signo)) == sizeof(signo))
                {
                    alarm(0);
                    switch (signo)
                    {
                        case SIG_EVENT_START:
                            if (PSAttachedState != 1 && profile->loopback_state == 0)
                                break;
                            
                            if (SetupCallAllowTime > clock_msec()) {
                                alarm((SetupCallAllowTime - clock_msec()+999)/1000);
                                break;
                            }

#ifdef USE_IPC_MSG_STATUS_IND
                            main_send_status_to_fifo(QCM_PREPARE_START_DAIL);
#endif

                            if (profile->enable_ipv4 && IPv4ConnectionStatus !=  QWDS_PKT_DATA_CONNECTED) {
                                qmierr = request_ops->requestSetupDataCall(profile, IpFamilyV4);

                                if ((qmierr > 0) && profile->user && profile->user[0] && profile->pd && profile->pd[0]) {
                                    int old_auto =  profile->auth;

                                    //may be fail because wrong auth mode, try pap->chap, or chap->pap
                                    profile->auth = (profile->auth == 1) ? 2 : 1;
                                	qmierr = request_ops->requestSetupDataCall(profile, IpFamilyV4);

                                    if (qmierr)
                                        profile->auth = old_auto; //still fail, restore old auth moe
                                }

                                if (!qmierr) {
                                    qmierr = request_ops->requestGetIPAddress(profile, IpFamilyV4);
                                    if (!qmierr)
                                    {
                                        IPv4ConnectionStatus = QWDS_PKT_DATA_CONNECTED;
#ifdef USE_IPC_MSG_STATUS_IND
                                        main_send_status_to_fifo(QCM_NETWORK_IPV4_CONNECTED);
#endif
                                    }
                                }
                                        
                            }

                            if (profile->enable_ipv6 && IPv6ConnectionStatus !=  QWDS_PKT_DATA_CONNECTED) {
                                if (profile->enable_ipv4 && profile->request_ops != &qmi_request_ops) {
                                    IPv6ConnectionStatus = IPv4ConnectionStatus;
                                }
                                else {
                                    qmierr = request_ops->requestSetupDataCall(profile, IpFamilyV6);

                                    if (!qmierr) {
                                        qmierr = request_ops->requestGetIPAddress(profile, IpFamilyV6);
                                        if (!qmierr)
                                        {
                                            IPv6ConnectionStatus = QWDS_PKT_DATA_CONNECTED;
#ifdef USE_IPC_MSG_STATUS_IND
                                            main_send_status_to_fifo(QCM_NETWORK_IPV6_CONNECTED);
#endif
                                        }
                                    }
                                }
                            }
                                
                            if ((profile->enable_ipv4 && IPv4ConnectionStatus ==  QWDS_PKT_DATA_DISCONNECTED)
                                    || (profile->enable_ipv6 && IPv6ConnectionStatus ==  QWDS_PKT_DATA_DISCONNECTED)) {
                                const unsigned allow_time[] = {5, 10, 20, 40, 60};

                                if (SetupCallFail < (sizeof(allow_time)/sizeof(unsigned)))
                                    SetupCallAllowTime = allow_time[SetupCallFail];
                                else
                                    SetupCallAllowTime = 60;
                                SetupCallFail++;
                                dbg_time("try to requestSetupDataCall %ld second later", SetupCallAllowTime);
                                alarm(SetupCallAllowTime);
                                SetupCallAllowTime = SetupCallAllowTime*1000 + clock_msec();
#ifdef USE_IPC_MSG_STATUS_IND
                                main_send_status_to_fifo(QCM_PREPARE_RESTART_DAIL);
#endif
                            }
                            else if (IPv4ConnectionStatus ==  QWDS_PKT_DATA_CONNECTED || IPv6ConnectionStatus ==  QWDS_PKT_DATA_CONNECTED) {
                                SetupCallFail = 0;
                                SetupCallAllowTime = clock_msec();
#ifdef USE_IPC_MSG_STATUS_IND
                                main_send_status_to_fifo(QCM_PREPARE_DAIL_SUCC);
#endif
                            }
                        break;

                        case SIG_EVENT_CHECK:
                            if (request_ops->requestGetSignalInfo)
                                request_ops->requestGetSignalInfo();
                            
                            if (request_ops->requestGetCellInfoList)
                                request_ops->requestGetCellInfoList();
							
                            if (request_ops->requestGetCoexWWANState)
                                request_ops->requestGetCoexWWANState();
							
                            if (PSAttachedState != 1)
                                request_ops->requestRegistrationState(&PSAttachedState);

#ifdef REBOOT_SIM_CARD_WHEN_LONG_TIME_NO_PS
                            if (PSAttachedState) {
                                PsAttachTime = clock_msec();
                                PsAttachFail = 0;
                            }
                            else {
                                unsigned long diff = (clock_msec() - PsAttachTime) / 1000;
                                unsigned long threshold = REBOOT_SIM_CARD_WHEN_LONG_TIME_NO_PS << PsAttachFail;

                                if (diff > threshold || diff > 960) {
                                    //interval time is 60 -> 120 - > 240 - > 480 -> 960
                                    PsAttachTime = clock_msec();
                                    PsAttachFail++;

                                    if (request_ops->requestRadioPower) {
                                        request_ops->requestRadioPower(0);
                                        request_ops->requestRadioPower(1);
                                    }
                                }
                            }
#endif

                            if (profile->enable_ipv4 && IPv4ConnectionStatus != QWDS_PKT_DATA_DISCONNECTED
                                && !request_ops->requestQueryDataCall(&IPv4ConnectionStatus, IpFamilyV4))
                            {
                                if (QWDS_PKT_DATA_CONNECTED == IPv4ConnectionStatus && profile->ipv4.Address == 0) {
                                    //killall -9 quectel-CM for MBIM and ATC call
                                    qmierr = request_ops->requestGetIPAddress(profile, IpFamilyV4);
                                    if (qmierr)
                                        IPv4ConnectionStatus = QWDS_PKT_DATA_DISCONNECTED;
                                }

                                //local ip is different with remote ip
                                if (QWDS_PKT_DATA_CONNECTED == IPv4ConnectionStatus && check_ipv4_address(profile) == 0) {
                                    request_ops->requestDeactivateDefaultPDP(profile, IpFamilyV4);
                                    IPv4ConnectionStatus = QWDS_PKT_DATA_DISCONNECTED;
                                }
                            }
                            else {
                                IPv4ConnectionStatus = QWDS_PKT_DATA_DISCONNECTED;
                            }

                            if (profile->enable_ipv6 && IPv6ConnectionStatus != QWDS_PKT_DATA_DISCONNECTED) {
                                if (profile->enable_ipv4 && profile->request_ops != &qmi_request_ops) {
                                    IPv6ConnectionStatus = IPv4ConnectionStatus;
                                }
                                else {
                                    request_ops->requestQueryDataCall(&IPv6ConnectionStatus, IpFamilyV6);
                                }
                            }
                            else {
                                IPv6ConnectionStatus = QWDS_PKT_DATA_DISCONNECTED;
                            }

                            if (IPv4ConnectionStatus ==  QWDS_PKT_DATA_DISCONNECTED && IPv6ConnectionStatus ==  QWDS_PKT_DATA_DISCONNECTED) {
                                usbnet_link_change(0, profile);
                            }
                            else if (IPv4ConnectionStatus ==  QWDS_PKT_DATA_CONNECTED || IPv6ConnectionStatus ==  QWDS_PKT_DATA_CONNECTED) {
                                int link = 0;
                                if (IPv4ConnectionStatus == QWDS_PKT_DATA_CONNECTED)
                                    link |= (1<<IpFamilyV4);
                                if (IPv6ConnectionStatus == QWDS_PKT_DATA_CONNECTED)
                                    link |= (1<<IpFamilyV6);
                                usbnet_link_change(link, profile);
                            }

#ifdef USE_IPC_MSG_STATUS_IND
                            if ((profile->enable_ipv4 && IPv4ConnectionStatus ==  QWDS_PKT_DATA_DISCONNECTED))
                            {
                                main_send_status_to_fifo(QCM_NETWORK_IPV4_DISCONNECTED);
                            }
                            else
                            {
                                main_send_status_to_fifo(QCM_NETWORK_IPV4_CONNECTED);
                            }

                            if ((profile->enable_ipv6 && IPv6ConnectionStatus ==  QWDS_PKT_DATA_DISCONNECTED))
                            {
                                main_send_status_to_fifo(QCM_NETWORK_IPV6_DISCONNECTED);
                            }
                            else
                            {
                                main_send_status_to_fifo(QCM_NETWORK_IPV6_CONNECTED);
                            }
#endif

                            if ((profile->enable_ipv4 && IPv4ConnectionStatus ==  QWDS_PKT_DATA_DISCONNECTED)
                                || (profile->enable_ipv6 && IPv6ConnectionStatus ==  QWDS_PKT_DATA_DISCONNECTED)) {
                                send_signo_to_main(SIG_EVENT_START);
                            }
                        break;

                        case SIG_EVENT_STOP:
                            if (profile->enable_ipv4 && IPv4ConnectionStatus ==  QWDS_PKT_DATA_CONNECTED) {
                                request_ops->requestDeactivateDefaultPDP(profile, IpFamilyV4);
                            }
                            if (profile->enable_ipv6 && IPv6ConnectionStatus ==  QWDS_PKT_DATA_CONNECTED) {
                                if (profile->enable_ipv4 && profile->request_ops != &qmi_request_ops) {
                                    
                                }
                                else {
                                    request_ops->requestDeactivateDefaultPDP(profile, IpFamilyV6);
                                }
                            }
                            usbnet_link_change(0, profile);
#ifdef USE_IPC_MSG_STATUS_IND
                            main_send_status_to_fifo(QCM_PROCESS_EXIT);
#endif
                            if (profile->qmi_ops->deinit)
                                profile->qmi_ops->deinit();
                            main_send_event_to_qmidevice(RIL_REQUEST_QUIT);
                            goto __main_quit;
                        break;

                        default:
                        break;
                    }
                }
            }

            if (fd == qmidevice_control_fd[0]) {
                if (read(fd, &triger_event, sizeof(triger_event)) == sizeof(triger_event)) {
                    switch (triger_event) {
                        case RIL_INDICATE_DEVICE_DISCONNECTED:
                            usbnet_link_change(0, profile);                            
                            goto __main_quit;
                        break;

                        case RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED:
                            request_ops->requestRegistrationState(&PSAttachedState);
                            if (PSAttachedState == 1) {
                                if ((profile->enable_ipv4 && IPv4ConnectionStatus ==  QWDS_PKT_DATA_DISCONNECTED)
                                    || (profile->enable_ipv6 && IPv6ConnectionStatus ==  QWDS_PKT_DATA_DISCONNECTED)) {
                                    send_signo_to_main(SIG_EVENT_START);
                                }
                             } else {
                                SetupCallAllowTime = clock_msec();
                             }
                        break;

                        case RIL_UNSOL_DATA_CALL_LIST_CHANGED:
                            if (IPv4ConnectionStatus ==  QWDS_PKT_DATA_CONNECTED || IPv6ConnectionStatus ==  QWDS_PKT_DATA_CONNECTED) {
                                SetupCallAllowTime = clock_msec() + 1000; //from connect -> disconnect, do not re-dail immediately, wait network stable
                            }
                            send_signo_to_main(SIG_EVENT_CHECK);
                        break;

                        case MODEM_REPORT_RESET_EVENT:
                        {
                            dbg_time("main recv MODEM RESET SIGNAL");
                            main_send_event_to_qmidevice(RIL_REQUEST_QUIT);
                            g_donot_exit_when_modem_hangup = 1;
                            goto __main_quit;
                        }
                        break;

                    	case RIL_UNSOL_LOOPBACK_CONFIG_IND:
                        {
                            QMI_WDA_SET_LOOPBACK_CONFIG_IND_MSG SetLoopBackInd;
                        	if (read(fd, &SetLoopBackInd, sizeof(SetLoopBackInd)) == sizeof(SetLoopBackInd)) {
                            	profile->loopback_state = SetLoopBackInd.loopback_state.TLVVaule;
                            	profile->replication_factor = le32_to_cpu(SetLoopBackInd.replication_factor.TLVVaule);
                            	dbg_time("SetLoopBackInd: loopback_state=%d, replication_factor=%u",
                                	profile->loopback_state, profile->replication_factor);
                                    if (profile->loopback_state)
                                        send_signo_to_main(SIG_EVENT_START);
                            }
                        }
                    	break;
#ifdef CONFIG_REG_QOS_IND
                        case RIL_UNSOL_GLOBAL_QOS_FLOW_IND_QOS_ID:
                        {
                            UINT qos_id = 0;
                            if (read(fd, &qos_id, sizeof(qos_id)) == sizeof(qos_id)) {
                                profile->qos_id = qos_id;
                            }
                        }
                    	break;
#endif
                        default:
                        break;
                    }
                }
            }
        }
    }

__main_quit:
    usbnet_link_change(0, profile);
    if (gQmiThreadID && pthread_join(gQmiThreadID, NULL)) {
        dbg_time("%s Error joining to listener thread (%s)", __func__, strerror(errno));
    }

#ifdef USE_IPC_MSG_STATUS_IND
    if (gQcmStatusThreadID && pthread_join(gQcmStatusThreadID, NULL)) {
        dbg_time("%s Error joining to gQcmStatusThreadID (%s)", __func__, strerror(errno));
    }
#endif

    close(signal_control_fd[0]);
    close(signal_control_fd[1]);
    close(qmidevice_control_fd[0]);
    close(qmidevice_control_fd[1]);
#ifdef USE_IPC_MSG_STATUS_IND
    close(qcm_status_indication_fd[0]);
    close(qcm_status_indication_fd[1]);
#endif
    dbg_time("%s exit", __func__);

    return 0;
}

static int quectel_CM(PROFILE_T *profile)
{
    int ret = 0;
    char qmichannel[32] = {'\0'};
    char usbnet_adapter[32] = {'\0'};

    if (profile->expect_adapter[0])
        strncpy(usbnet_adapter, profile->expect_adapter, sizeof(usbnet_adapter));
    
    if (qmidevice_detect(qmichannel, usbnet_adapter, sizeof(qmichannel), profile)) {
    	profile->hardware_interface = HARDWARE_USB;
    }
    else if (mhidevice_detect(qmichannel, usbnet_adapter, profile)) {
        profile->hardware_interface = HARDWARE_PCIE;
    }
    else if (atdevice_detect(qmichannel, usbnet_adapter, profile)) {
        profile->hardware_interface = HARDWARE_PCIE;
    }
#ifdef CONFIG_QRTR
    else if (!access("/sys/class/net/rmnet_mhi0", F_OK)) {
        strcpy(usbnet_adapter, "rmnet_mhi0");
        profile->hardware_interface = HARDWARE_PCIE;
        strcpy(qmichannel, "qrtr");
        profile->software_interface = SOFTWARE_QRTR;
    }
    else if (!access("/sys/class/net/rmnet_usb0", F_OK)) {
        strcpy(usbnet_adapter, "rmnet_usb0");
        profile->hardware_interface = HARDWARE_USB;
        strcpy(qmichannel, "qrtr");
        profile->software_interface = SOFTWARE_QRTR;
    }
    else if (!access("/sys/class/net/rmnet_ipa0", F_OK)) {
        strcpy(usbnet_adapter, "rmnet_ipa0");
        profile->hardware_interface = HARDWARE_IPA;
        strcpy(qmichannel, "qrtr");
        profile->software_interface = SOFTWARE_QRTR;
    }
#endif
    else {
        dbg_time("qmidevice_detect failed");
        goto error;
    }

    strncpy(profile->qmichannel, qmichannel, sizeof(profile->qmichannel));
    strncpy(profile->usbnet_adapter, usbnet_adapter, sizeof(profile->usbnet_adapter));
    ql_get_netcard_driver_info(profile->usbnet_adapter);

    if ((profile->hardware_interface == HARDWARE_USB) && profile->usblogfile)
        ql_capture_usbmon_log(profile, profile->usblogfile);

    if (profile->hardware_interface == HARDWARE_USB) {
        profile->software_interface = get_driver_type(profile);
    }

    ql_qmap_mode_detect(profile);

    if (profile->software_interface == SOFTWARE_MBIM) {
        dbg_time("Modem works in MBIM mode");
        profile->request_ops = &mbim_request_ops;
        profile->qmi_ops = &mbim_dev_ops;
        if (!profile->apn || !profile->apn[0]) {
            //see FAE-51804  FAE-59811
            dbg_time("When MBIM mode, must specify APN with '-s', or setup data call may fail!");
            exit(-404); //if no such issue on your side, please comment this
        }
        ret = qmi_main(profile);
    }
    else if (profile->software_interface == SOFTWARE_QMI) {
        dbg_time("Modem works in QMI mode");
        profile->request_ops = &qmi_request_ops;
        if (qmidev_is_gobinet(profile->qmichannel))
            profile->qmi_ops = &gobi_qmidev_ops;
        else
            profile->qmi_ops = &qmiwwan_qmidev_ops;
        qmidev_send = profile->qmi_ops->send;
        ret = qmi_main(profile);
    }
    else if (profile->software_interface == SOFTWARE_ECM_RNDIS_NCM) {
        dbg_time("Modem works in ECM_RNDIS_NCM mode");
        profile->request_ops = &atc_request_ops;
        profile->qmi_ops = &atc_dev_ops;
        ret = qmi_main(profile);
    }
#ifdef CONFIG_QRTR
    else if (profile->software_interface == SOFTWARE_QRTR) {
        dbg_time("Modem works in QRTR mode");
        profile->request_ops = &qmi_request_ops;
        profile->qmi_ops = &qrtr_qmidev_ops;
        qmidev_send = profile->qmi_ops->send;
        ret = qmi_main(profile);
    }
#endif
    else {
        dbg_time("unsupport software_interface %d", profile->software_interface);
    }

    ql_stop_usbmon_log(profile);

error:
    return ret;
}

static int parse_user_input(int argc, char **argv, PROFILE_T *profile) {
    int opt = 1;
    int apn_name_or_type;
    profile->pdp = CONFIG_DEFAULT_PDP;
    profile->profile_index = CONFIG_DEFAULT_PDP;

    if (!strcmp(argv[argc-1], "&"))
        argc--;

#define has_more_argv() ((opt < argc) && (argv[opt][0] != '-'))
    while  (opt < argc) {
        if (argv[opt][0] != '-') {
            return usage(argv[0]);
        }

        switch (argv[opt++][1])
        {
            case 's':
                profile->apn = profile->user = profile->pd = "";
                if (has_more_argv()) {
                    profile->apn = argv[opt++];
                }
                if (has_more_argv()) {
                    profile->user = argv[opt++];
                }
                if (has_more_argv()) {
                    profile->pd = argv[opt++];
                    if (profile->pd && profile->pd[0])
                        profile->auth = 2; //default chap, customers may miss auth
                }
                if (has_more_argv()) {
                    const char *auth = argv[opt++];

                    if (!strcmp(auth, "0") || !strcasecmp(auth, "none")) {
                        profile->auth = 0;
                    }
                    else if (!strcmp(auth, "1") || !strcasecmp(auth, "pap")) {
                        profile->auth = 1;
                    }
                    else if (!strcmp(auth, "2") || !strcasecmp(auth, "chap")) {
                        profile->auth = 2;
                    }
					else if (!strcmp(auth, "3") || !strcasecmp(auth, "MsChapV2")) {
                        profile->auth = 3;
                    }
                    else {
                        dbg_time("unknow auth '%s'", auth);
                        return usage(argv[0]);
                    }
                }
            break;

            case 'p':
                if (has_more_argv()) {
                    const char *arg = argv[opt++];

                    if (!strcmp(arg, QUECTEL_QMI_PROXY) || !strcmp(arg, QUECTEL_MBIM_PROXY)
                        || !strcmp(arg, LIBQMI_PROXY) || !strcmp(arg, LIBMBIM_PROXY) || !strcmp(arg, QUECTEL_ATC_PROXY)) {
                        strncpy(profile->proxy, arg, sizeof(profile->proxy) - 1);
                    }
                    else if ((999 < atoi(arg)) && (atoi(arg) < 10000)) {
                        profile->pincode = arg;
                    }
                    else {
                        dbg_time("unknow -p '%s'", arg);
                        return usage(argv[0]);
                    }
                }
            break;

            case 'm':
                if (has_more_argv())
                    profile->muxid = argv[opt++][0] - '0' + 0x80;
            break;

            case 'n':
                if (has_more_argv())
                    profile->pdp = argv[opt++][0] - '0';
            break;

            case 'f':
                if (has_more_argv()) {
                    profile->logfile = argv[opt++];
                }
            break;

            case 'i':
                if (has_more_argv()) {
                    strncpy(profile->expect_adapter, argv[opt++], sizeof(profile->expect_adapter) - 1);
                }
            break;

            case 'o':
                if (has_more_argv()) {
                    strncpy(profile->link_adapter, argv[opt++], sizeof(profile->link_adapter) - 1);
                }
            break;

            case 'v':
                debug_qmi = 1;
            break;

            case 'l':
            	if (has_more_argv()) {
                    profile->replication_factor = atoi(argv[opt++]);
                	if (profile->replication_factor > 0) {
                        	profile->loopback_state = 1;
                     }
                }
            break;

            case '4':
                profile->enable_ipv4 = 1; 
            break;

            case '6':
                profile->enable_ipv6 = 1;
            break;

            case 'd':
                profile->no_dhcp = 1;
            break;

            case 'u':
                if (has_more_argv()) {
                    profile->usblogfile = argv[opt++];
                }
            break;

            case 'b':
                profile->enable_bridge = 1;
            break;
			
            case 'k':
                if (has_more_argv()) {
                    profile->kill_pdp = argv[opt++][0] - '0';
                }
                break;

            case 'a':
                if (has_more_argv()) {
                    apn_name_or_type = argv[opt++][0] - '0';
                    if(apn_name_or_type == 1)
                    profile->bring_up_by_apn_name = apn_name_or_type;
                    else if(apn_name_or_type == 2) 
                    profile->bring_up_by_apn_type = apn_name_or_type;   
                }
                break;
            case 'y':
                if (has_more_argv()) {
                    profile->apn_type = argv[opt++][0] - '0';
                }
                break;            
            default:
                return usage(argv[0]);
            break;
        }
    }

    if (profile->enable_ipv4 != 1 && profile->enable_ipv6 != 1) { // default enable IPv4
        profile->enable_ipv4 = 1;
    }

    return 1;
}

int main(int argc, char *argv[])
{
    int ret;
    PROFILE_T *ctx = &s_profile;

    dbg_time("QConnectManager_Linux_V1.6.8");

    ret = parse_user_input(argc, argv, ctx);
    if (!ret)
        return ret;

    if (ctx->kill_pdp) {
        return kill_data_call_pdp(ctx->kill_pdp, argv[0]);
    }

    if (ctx->logfile) {
        logfilefp = fopen(ctx->logfile, "a+");
        if (!logfilefp) {
            dbg_time("Fail to open %s, errno: %d(%s)", ctx->logfile, errno, strerror(errno));
        }
    }

    signal(SIGINT, ql_sigaction);
    signal(SIGTERM, ql_sigaction);
    signal(SIGALRM, ql_sigaction);

    do {
        ret = quectel_CM(ctx);
        if (g_donot_exit_when_modem_hangup > 0)
            sleep(3);
    } while (g_donot_exit_when_modem_hangup > 0);

    if (logfilefp) {
        fclose(logfilefp);
    }

    return ret;
}
