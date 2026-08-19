/**
 * @file main.c
 * @brief ble-provisiond daemon entry point.
 *
 * Uses libble for all BLE operations. Integrates with uloop event loop,
 * ubus for IPC, and UCI for configuration.
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include <unistd.h>
#include <syslog.h>
#include <errno.h>
#include <poll.h>

#include <libubox/uloop.h>
#include <ble.h>

#define BLE_PROVISIOND_VERSION "1.0.0"

/* Forward declarations */
int ubus_api_init(void);
void ubus_api_deinit(void);
int uci_config_load(ble_config_t *config);
void app_plugins_init(void);
void app_plugins_deinit(void);

#include "uci_app_config.h"

static struct uloop_fd ble_ufd;
static volatile sig_atomic_t g_running = 1;

static struct option long_options[] = {
    { "foreground", no_argument,       NULL, 'f' },
    { "verbose",    no_argument,       NULL, 'v' },
    { "version",    no_argument,       NULL, 'V' },
    { "help",       no_argument,       NULL, 'h' },
    { NULL, 0, NULL, 0 }
};

static void signal_handler(int sig)
{
    (void)sig;
    g_running = 0;
    /* Remove status file early — in case cleanup path is interrupted by SIGKILL */
    unlink("/var/run/ble-provision.status");
    unlink("/var/run/ble-provision.pid");
    uloop_end();
}

/**
 * @brief uloop callback when libble fd is readable.
 */
static void ble_fd_cb(struct uloop_fd *fd, unsigned int events)
{
    (void)fd; (void)events;
    ble_process();
}

/**
 * @brief Global event callback — forwards libble events to ubus.
 */
static void daemon_event_cb(const ble_event_t *event, void *user_data)
{
    (void)user_data;
    if (!event) return;

    /* Forward to ubus event publishing (implemented in ubus_api.c) */
    extern void ubus_publish_event(const ble_event_t *event);
    ubus_publish_event(event);
}

static void print_usage(const char *progname)
{
    fprintf(stderr,
        "Usage: %s [OPTIONS]\n\n"
        "BLE provision daemon for OpenWrt (uses libble)\n\n"
        "Options:\n"
        "  -f, --foreground  Run in foreground\n"
        "  -v, --verbose     Debug logging\n"
        "  -V, --version     Show version\n"
        "  -h, --help        Show this help\n\n"
        "Config: /etc/config/ble-provision\n"
        "ubus:   ble\n", progname);
}

static int daemonize(void)
{
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid > 0) exit(0);
    if (setsid() < 0) return -1;
    freopen("/dev/null", "r", stdin);
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);
    return 0;
}

int main(int argc, char *argv[])
{
    int opt;
    bool foreground = false;
    bool verbose = false;

    while ((opt = getopt_long(argc, argv, "fvVh", long_options, NULL)) != -1) {
        switch (opt) {
        case 'f': foreground = true; break;
        case 'v': verbose = true; break;
        case 'V':
            printf("ble-provisiond %s (libble %d.%d.%d)\n",
                   BLE_PROVISIOND_VERSION,
                   LIBBLE_VERSION_MAJOR, LIBBLE_VERSION_MINOR,
                   LIBBLE_VERSION_PATCH);
            return 0;
        case 'h': print_usage(argv[0]); return 0;
        default:  print_usage(argv[0]); return 1;
        }
    }

    openlog("ble-provisiond", LOG_PID | LOG_NDELAY,
            foreground ? LOG_USER : LOG_DAEMON);
    setlogmask(LOG_UPTO(verbose ? LOG_DEBUG : LOG_INFO));
    syslog(LOG_INFO, "ble-provisiond %s starting", BLE_PROVISIOND_VERSION);

    if (!foreground && daemonize() < 0) {
        syslog(LOG_ERR, "Failed to daemonize");
        return 1;
    }

    /* Signal handlers */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);

    /* Load UCI config */
    ble_config_t config;
    memset(&config, 0, sizeof(config));
    config.transport = BLE_TRANSPORT_AUTO;
    config.baud_rate = 115200;
    strncpy(config.adapter, "hci0", sizeof(config.adapter) - 1);
    int ret = uci_config_load(&config);
    if (ret == -ENODEV) {
        syslog(LOG_INFO, "BLE chip disabled in UCI — exiting");
        closelog();
        return 0;
    }

    /* Initialize libble */
    ret = ble_init(&config);
    if (ret != BLE_OK) {
        syslog(LOG_ERR, "ble_init failed: %s", ble_strerror(ret));
        closelog();
        return 1;
    }

    /* Load app config from /etc/config/ble */
    ble_app_config_t app_cfg;
    uci_app_config_load(&app_cfg);

    /* Set log level from daemon config */
    if (strcmp(app_cfg.daemon.log_level, "debug") == 0)
        setlogmask(LOG_UPTO(LOG_DEBUG));
    else if (strcmp(app_cfg.daemon.log_level, "warn") == 0)
        setlogmask(LOG_UPTO(LOG_WARNING));
    else if (strcmp(app_cfg.daemon.log_level, "error") == 0)
        setlogmask(LOG_UPTO(LOG_ERR));

    /*
     * Autostart for ibeacon/scan is handled by app_plugins_init()
     * AFTER gatt_server starts (so ble_adv module is ready).
     * Do NOT start beacon/scan here — ble_adv is not initialized yet.
     */

    /* Subscribe to all events */
    ble_subscribe(daemon_event_cb, NULL);

    /* Initialize uloop */
    uloop_init();

    /* Add libble fd to uloop */
    int ble_fd = ble_get_fd();
    if (ble_fd >= 0) {
        ble_ufd.fd = ble_fd;
        ble_ufd.cb = ble_fd_cb;
        uloop_fd_add(&ble_ufd, ULOOP_READ);
    }

    /* Initialize ubus API */
    ret = ubus_api_init();
    if (ret < 0)
        syslog(LOG_WARNING, "ubus init failed (continuing): %d", ret);

    /* Initialize application plugins */
    app_plugins_init();

    /* Run event loop */
    syslog(LOG_INFO, "Entering main event loop (transport=%s)",
           ble_get_transport_name());
    uloop_run();

    /* Cleanup */
    app_plugins_deinit();
    ubus_api_deinit();
    ble_deinit();
    uloop_done();

    syslog(LOG_INFO, "ble-provisiond exiting");
    closelog();
    return 0;
}
