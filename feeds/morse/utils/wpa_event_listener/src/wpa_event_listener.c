/*
 *  copyright (C) 2023-2024 Morse Micro Pty Ltd. All rights reserved.
 */

#include <dirent.h>
#include <errno.h>
#include <libubox/uloop.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wpa_ctrl.h>

#define CONFIG_CTRL_IFACE_DIR "/var/run/wpa_supplicant_s1g"
#define wpa_el_version "V1.0.0"

enum event_value_types {
    TYPE_CONF_RECEIVED,
    TYPE_ENCRYPTION,
    TYPE_SSID,
    TYPE_PSK,
    TYPE_CONNECTOR,
    TYPE_C_SIGN_KEY,
    TYPE_PP_KEY,
    TYPE_NET_ACCESS_KEY,
    TYPE_CTRL_EVENT_TERMINATING,
    TYPE_PB_RESULT,
    TYPE_PB_STATUS,
};

struct event {
    const char *const event_title;
    const enum event_value_types event_type;
};

/* dpp connector, c-sign-key, pp-key and net access key will be received after
 * qrcode provisionning but they are ignored by now.
 */
static struct event interresting_events[] = {
    {"DPP-CONF-RECEIVED", TYPE_CONF_RECEIVED},
    {"DPP-CONFOBJ-AKM", TYPE_ENCRYPTION},
    {"DPP-CONFOBJ-SSID", TYPE_SSID},
    {"DPP-CONFOBJ-PASS", TYPE_PSK},
    // { "DPP-CONNECTOR"       , TYPE_CONNECTOR },
    // { "DPP-C-SIGN-KEY"      , TYPE_C_SIGN_KEY },
    // { "DPP-PP-KEY"          , TYPE_PP_KEY },
    // { "DPP-NET-ACCESS-KEY"  , TYPE_NET_ACCESS_KEY },
    {"DPP-PB-STATUS", TYPE_PB_STATUS},
    {"DPP-PB-RESULT", TYPE_PB_RESULT},
    {"CTRL-EVENT-TERMINATING", TYPE_CTRL_EVENT_TERMINATING},
};

static struct {
    char *encryption;
    char *ssid;
    char *psk;
    // char* connector;
    // char* c_sign_key;
    // char* pp_key;
    // char* net_access_key;
} cached_confs;

static struct wpa_ctrl *ctrl_conn;
static struct uloop_fd listener;

static void led_timeout_cb(struct uloop_timeout*);
static struct uloop_timeout led_timeout = {
    .cb = led_timeout_cb,
};
static void led_failed_cb(struct uloop_timeout *);
static struct uloop_timeout failed_timeout = {
    .cb = led_failed_cb,
};

static char *ctrl_ifname = NULL;
static char *action_script = NULL;
static const char *ctrl_iface_dir = CONFIG_CTRL_IFACE_DIR;
static const char *pid_file = NULL;
static int exit_on_config_receive = 1;

void cache_received_confs(char **config, const char *const value) {
    // check if the config is already allocated.
    if (*config)
        free(*config);
    *config = strdup(value);
}

void clean_pointer(char **ptr) {
    if (*ptr) {
        free(*ptr);
        *ptr = NULL;
    }
}

void clear_cached_confs() {
    clean_pointer(&cached_confs.encryption);
    clean_pointer(&cached_confs.ssid);
    clean_pointer(&cached_confs.psk);
}

void clear_env() {
    unsetenv("iface_name");
    unsetenv("encryption");
    unsetenv("ssid");
    unsetenv("psk");
}

void apply_cached_confs() {
    // make sure that all needed configs are cached.
    if (!cached_confs.encryption || !cached_confs.ssid || !cached_confs.psk)
        return;

    printf("Configs: \n");
    printf("    encryption: %s\n", cached_confs.encryption);
    printf("    ssid: %s\n", cached_confs.ssid);
    printf("    psk: %s\n", cached_confs.psk);

    pid_t pid = 0;
    // only fork if we don't want to exit after receiving configs.
    if (exit_on_config_receive == 0) {
        pid = fork();
        if (pid == -1) {
            printf("Unable to fork action script\n");
            return;
        }
        // forked process will have pid=0.
        if (pid > 0)
            return;
    }

    setenv("iface_name", ctrl_ifname, 1);
    setenv("encryption", cached_confs.encryption, 1);
    setenv("ssid", cached_confs.ssid, 1);
    setenv("psk", cached_confs.psk, 1);
    if (execl(action_script, action_script, "config", (char *)NULL) == -1) {
        perror("Could not execv");
        clear_env();
        return;
    }
    clear_env();
}

void call_action_script(const char *arg) {
    pid_t pid = fork();
    if (pid == -1) {
        printf("Unable to fork action script");
        return;
    }
    if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
    } else if (execl(action_script, action_script, arg, (char *)NULL) == -1) {
        perror("Could not execv");
        exit(1);
    }
}

static void led_timeout_cb(struct uloop_timeout *t) {
    printf("led timeout\n");
    call_action_script("finished");
}

static void led_finished() {
    // Stop the blink.
    printf("Stopping led blink\n");
    uloop_timeout_cancel(&led_timeout);
    call_action_script("finished");
}

static void led_failed_cb(struct uloop_timeout *t) {
    // Start the fail blink with a short timeout.
    printf("Starting led fail blink\n");
    uloop_timeout_set(&led_timeout, 5000);
    call_action_script("failed");
}

static void led_failed() {
    // Issue the failed command after a small delay. This is because
    // wpa_supplicant emits back to back failed and started events when a
    // dpp_push_button occurs while another is still running, so save some time
    // for led_started to cancel the failed_timeout.
    uloop_timeout_set(&failed_timeout, 1000);
}

static void led_started() {
    // Start the blink. Set a long timeout for the blink in case we don't see hostapd PB_RESULT event.
    printf("Starting led blink\n");
    uloop_timeout_cancel(&failed_timeout);
    uloop_timeout_set(&led_timeout, 120000);
    call_action_script("started");
}

static void message_process(char *const message) {
    const int good_events_count = sizeof(interresting_events) / sizeof(struct event);

    for (int i = 0; i < good_events_count; i++) {
        char *ptr = strstr(message, interresting_events[i].event_title);
        if (ptr) {
            const char *const value = ptr + strlen(interresting_events[i].event_title) +
                                      1; // 1 for the space between title and value
            switch (interresting_events[i].event_type) {
            case TYPE_CONF_RECEIVED:
                clear_cached_confs();
                break;
            case TYPE_ENCRYPTION:
                cache_received_confs(&cached_confs.encryption, value);
                break;
            case TYPE_SSID:
                cache_received_confs(&cached_confs.ssid, value);
                break;
            case TYPE_PSK:
                cache_received_confs(&cached_confs.psk, value);
                break;
            case TYPE_PB_RESULT:
                // AP side we wont have confs, so apply_cached_confs does nothing.
                if (strstr(value, "success")) {
                    led_finished();
                    apply_cached_confs();
                    clear_cached_confs();
                } else if (strstr(value, "failed")) {
                    led_failed();
                }
                break;
            case TYPE_PB_STATUS:
                if (strcmp(value, "started") == 0) {
                    led_started();
                }
                break;
            case TYPE_CTRL_EVENT_TERMINATING:
                printf("Connection to wpa_supplicant lost - exiting\n");
                uloop_end();
                break;
            default:
                break;
            }
            return;
        }
    }
}

static void listener_cb(struct uloop_fd *fd, unsigned int events) {
    while (wpa_ctrl_pending(ctrl_conn) > 0) {
        char buf[4096];
        size_t len = sizeof(buf) - 1;
        if (wpa_ctrl_recv(ctrl_conn, buf, &len) == 0) {
            buf[len] = '\0';
            printf("New Message:%s\n", buf);
            message_process(buf);
        } else {
            printf("Could not read pending message.\n");
            break;
        }
    }
    if (wpa_ctrl_pending(ctrl_conn) < 0) {
        printf("Connection to wpa_supplicant lost - exiting\n");
        uloop_end();
    }
}

static char *wpa_cli_get_default_ifname(void) {
    char *ifname = NULL;

    struct dirent *dent;
    DIR *dir = opendir(ctrl_iface_dir);
    if (!dir) {
        return NULL;
    }
    while ((dent = readdir(dir))) {
#ifdef _DIRENT_HAVE_D_TYPE
        /*
         * Skip the file if it is not a socket. Also accept
         * DT_UNKNOWN (0) in case the C library or underlying
         * file system does not support d_type.
         */
        if (dent->d_type != DT_SOCK && dent->d_type != DT_UNKNOWN)
            continue;
#endif /* _DIRENT_HAVE_D_TYPE */
        /* Skip current/previous directory and special P2P Device
         * interfaces. */
        if (strcmp(dent->d_name, ".") == 0 || strcmp(dent->d_name, "..") == 0 ||
            strncmp(dent->d_name, "p2p-dev-", 8) == 0)
            continue;
        printf("Selected interface '%s'\n", dent->d_name);
        ifname = strdup(dent->d_name);
        break;
    }
    closedir(dir);

    return ifname;
}

static void usage(void) {
    printf("wpa_event_listener  [-p<path to ctrl socket dir>] -i<ifname> [-hvBN] "
           "-a<action file> \\\n"
           "                    [-P<pid file>] "
           "\\\n"
           "        "
           "  -h = help (show this usage text)\n"
           "  -v = shown version information\n"
           "  -N = No Exit: don't exit after the configs are received.\n"
           "  -a = run in daemon mode executing the action file based on "
           "events from\n"
           "  -B = run a daemon in the background\n"
           "  default path: " CONFIG_CTRL_IFACE_DIR "\n"
           "  default interface: first interface found in socket path\n");
}

int main(int argc, char *argv[]) {
    int c;
    int daemonize = 0;
    for (;;) {
        c = getopt(argc, argv, "a:Bg:G:hNi:p:P:rs:v");
        if (c < 0)
            break;
        switch (c) {
        case 'a':
            action_script = optarg;
            break;
        case 'B':
            daemonize = 1;
            break;
        case 'h':
            usage();
            return 0;
        case 'v':
            printf("%s\n", wpa_el_version);
            return 0;
        case 'i':
            ctrl_ifname = strdup(optarg);
            break;
        case 'p':
            ctrl_iface_dir = optarg;
            break;
        case 'P':
            pid_file = optarg;
            break;
        case 'N':
            printf("No exit after receive config.\n");
            exit_on_config_receive = 0;
            break;
        default:
            usage();
            return -1;
        }
    }

    if (ctrl_ifname == NULL)
        ctrl_ifname = wpa_cli_get_default_ifname();
    if (ctrl_ifname == NULL) {
        printf("No WPA supplicant ctrl interface found.\n");
        return -1;
    }

    if (daemonize) {
        if (!action_script) {
            printf("Daemonizing requires an action script.\n");
            return -1;
        }

        if (daemon(0, 0)) {
            perror("daemon");
            return -1;
        }
        if (pid_file) {
            FILE *f = fopen(pid_file, "w");
            if (f) {
                fprintf(f, "%u\n", getpid());
                fclose(f);
            }
        }
    }

    char *path = malloc(strlen(ctrl_iface_dir) + strlen(ctrl_ifname) + 2);
    sprintf(path, "%s/%s", ctrl_iface_dir, ctrl_ifname);

    if (!(ctrl_conn = wpa_ctrl_open(path))) {
        printf("unable to open control interface on %s.\n", path);
        return -1;
    }

    uloop_init();

    if (wpa_ctrl_attach(ctrl_conn) == 0) {
        listener.cb = listener_cb;
        listener.fd = wpa_ctrl_get_fd(ctrl_conn);
        uloop_fd_add(&listener, ULOOP_READ);
        listener_cb(NULL, 0);
    } else {
        printf("Failed to attach to wpa_supplicant.\n");
        wpa_ctrl_close(ctrl_conn);
        return -1;
    }

    uloop_run();
    printf("Exiting\n");
    if (pid_file)
        remove(pid_file);

    return 0;
}
