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

#include <unistd.h>
#include <stdlib.h>

#include "evx.h"
#include "const.h"
#include "util.h"
#include "log.h"
#include "daemon.h"

#include "osn_dhcp.h"

#if !defined(CONFIG_OSN_DHCPC_UDHCPC)
#define CONFIG_OSN_UDHCPC_PATH "/sbin/udhcpc"
#endif /* CONFIG_OSN_DHCPC_UDHCPC */

bool osn_dhcp_client_init(osn_dhcp_client_t *self, const char *ifname);
bool osn_dhcp_client_fini(osn_dhcp_client_t *self);

static bool dhcpc_xopt_encode(enum osn_dhcp_option opt, const char *value, char *out, size_t outsz);
static daemon_atexit_fn_t dhcpc_atexit;
/*
 * The option file is created by the udhcpc script and contains
 * key=value pairs of the DHCP options received from the server.
 */
static void dhcpc_opt_watcher(struct ev_loop *loop, ev_stat *w, int revent);
static void dhcpc_opt_read(struct ev_loop *loop, ev_debounce *w, int revent);

/*
 * ===========================================================================
 *  DHCP Client driver for UDHCP; should be used on OpenWRT and derivatives
 * ===========================================================================
 */
struct osn_dhcp_client
{
    char                            dc_ifname[C_IFNAME_LEN];
    bool                            dc_started;
    daemon_t                        dc_proc;
    uint8_t                         dc_option_req[C_SET_LEN(DHCP_OPTION_MAX, uint8_t)];
    char                            *dc_option_set[DHCP_OPTION_MAX];
    osn_dhcp_client_error_fn_t      *dc_err_fn;

    /* Option file related members */
    osn_dhcp_client_opt_notify_fn_t *dc_opt_fn;                     /* Option notification callback */
    void                            *dc_opt_ctx;                    /* Callback context */
    char                            dc_opt_path[C_MAXPATH_LEN];     /* Option file path */
    ev_stat                         dc_opt_stat;                    /* Option file watcher */
    ev_debounce                     dc_opt_debounce;                /* Debounce timer */
};

osn_dhcp_client_t *osn_dhcp_client_new(const char *ifname)
{
    osn_dhcp_client_t *self = calloc(1, sizeof(osn_dhcp_client_t));

    if (!osn_dhcp_client_init(self, ifname))
    {
        free(self);
        return NULL;
    }

    return self;
}

bool osn_dhcp_client_del(osn_dhcp_client_t *self)
{
    bool retval = osn_dhcp_client_fini(self);

    free(self);

    return retval;
}

bool osn_dhcp_client_init(osn_dhcp_client_t *self, const char *ifname)
{
    memset(self, 0, sizeof(*self));

    if (STRSCPY(self->dc_ifname, ifname) < 0)
    {
        return false;
    }

    /* Create the daemon process definition */
    if (!daemon_init(&self->dc_proc, CONFIG_OSN_UDHCPC_PATH, DAEMON_LOG_ALL))
    {
        LOG(ERR, "dhcp_client: Failed to initialize udhcpc daemon.");
        return false;
    }

    /* Register atexit handler */
    daemon_atexit(&self->dc_proc, dhcpc_atexit);

    /* udhcpc should auto-restart if it dies */
    if (!daemon_restart_set(&self->dc_proc, true, 0.0, 0))
    {
        LOG(WARN, "dhcp_client: Failed to set auto-restart policy.");
    }

    /* Remember the options file full path */
    snprintf(self->dc_opt_path, sizeof(self->dc_opt_path), "/var/run/udhcpc_%s.opts", self->dc_ifname);

    /* Setup the debounce timer to trigger on 300ms */
    ev_debounce_init(&self->dc_opt_debounce, dhcpc_opt_read, 0.3);
    self->dc_opt_debounce.data = self;

    /* Setup the options file watcher */
    ev_stat_init(&self->dc_opt_stat, dhcpc_opt_watcher, self->dc_opt_path, 0.0);
    self->dc_opt_stat.data = self;

    return true;
}

bool osn_dhcp_client_fini(osn_dhcp_client_t *self)
{
    int ii;

    bool retval = true;

    if (!osn_dhcp_client_stop(self))
    {
        LOG(WARN, "dhcp_client: %s: Error stopping DHCP.", self->dc_ifname);
        retval = false;
    }

    /* Free option list */
    for (ii = 0; ii < DHCP_OPTION_MAX; ii++)
    {
        if (self->dc_option_set[ii] != NULL)
            free(self->dc_option_set[ii]);
    }

    return retval;
}

bool osn_dhcp_client_start(osn_dhcp_client_t *self)
{
    char pidfile[C_MAXPATH_LEN];

    int ii;

    if (self->dc_started) return true;

    snprintf(pidfile, sizeof(pidfile), "/var/run/udhcpc-%s.pid", self->dc_ifname);

    if (!daemon_pidfile_set(&self->dc_proc, pidfile, false))
    {
        LOG(ERR, "dhcp_client: Error setting the pidfile path. DHCP client on interface %s will not start.", self->dc_ifname);
        return false;
    }

    /* Build the DHCP options here */
    if (!daemon_arg_reset(&self->dc_proc))
    {
        LOG(ERR, "dhcp_client: Error resetting argument list. DHCP client on interface %s will not start.", self->dc_ifname);
        return false;
    }

    daemon_arg_add(&self->dc_proc, "-i", self->dc_ifname);              /* Interface to listen on */
    daemon_arg_add(&self->dc_proc, "-f");                               /* Run in foreground */
    daemon_arg_add(&self->dc_proc, "-p", pidfile);                      /* PID file path */
    daemon_arg_add(&self->dc_proc, "-s", "/usr/plume/bin/udhcpc.sh");   /* DHCP client script */
    daemon_arg_add(&self->dc_proc, "-t", "60");                         /* Send up to N discover packets */
    daemon_arg_add(&self->dc_proc, "-T", "1");                          /* Pause between packets */
    daemon_arg_add(&self->dc_proc, "-S");                               /* Log to syslog too */
#ifndef CONFIG_UDHCPC_OPTIONS_USE_CLIENTID
    daemon_arg_add(&self->dc_proc, "-C");                               /* Do not send MAC as client id */
#endif

    /* Check if we have custom requested options */
    bool have_options;

    for (ii = 0; ii < DHCP_OPTION_MAX; ii++)
    {
        have_options = C_IS_SET(self->dc_option_req, ii);
        if (have_options) break;
    }

    if (have_options)
    {
        char optstr[C_INT8_LEN];

        daemon_arg_add(&self->dc_proc, "--no-default-options");   /* Do not send default options */

        for (ii = 0; ii < DHCP_OPTION_MAX; ii++)
        {
            if (!C_IS_SET(self->dc_option_req, ii)) continue;

            snprintf(optstr, sizeof(optstr), "%d", ii);
            daemon_arg_add(&self->dc_proc, "-O", optstr);
        }
    }

    for (ii = 0; ii < DHCP_OPTION_MAX; ii++)
    {
        char optstr[128];

        /* The vendor class is handled below */
        if (ii == DHCP_OPTION_VENDOR_CLASS) continue;

        if (self->dc_option_set[ii] == NULL) continue;

        if (!dhcpc_xopt_encode(ii, self->dc_option_set[ii], optstr, sizeof(optstr)))
        {
            LOG(WARN, "dhcp_client: Error encoding option %d:%s.", ii, self->dc_option_set[ii]);
            continue;
        }

        daemon_arg_add(&self->dc_proc, "-x", optstr);
    }

    if (self->dc_option_set[DHCP_OPTION_VENDOR_CLASS] != NULL)
    {
        daemon_arg_add(&self->dc_proc, "--vendorclass", self->dc_option_set[DHCP_OPTION_VENDOR_CLASS]);
    }

    if (!daemon_start(&self->dc_proc))
    {
        LOG(ERR, "dhcp_client: Error starting udhcpc on interface %s.", self->dc_ifname);
        return false;
    }

    /* Remove any stale files first -- ignore errors */
    (void)unlink(self->dc_opt_path);

    ev_stat_start(EV_DEFAULT, &self->dc_opt_stat);

    self->dc_started = true;

    return true;
}

bool dhcpc_atexit(daemon_t *proc)
{
    osn_dhcp_client_t *self = container_of(proc, osn_dhcp_client_t, dc_proc);

    LOG(ERR, "dhcp_client: DHCP client exited abnormally on interface: %s.", self->dc_ifname);

    /* Call the error handler, if it exists */
    if (self->dc_err_fn != NULL) self->dc_err_fn(self);

    return true;
}

bool dhcpc_xopt_encode(
        enum osn_dhcp_option opt,
        const char *val,
        char *out,
        size_t outsz)
{
    size_t len;
    size_t ii;

    /* Figure out how to encode the new option */
    switch (opt)
    {
        case DHCP_OPTION_HOSTNAME:
            len = snprintf(out, outsz, "hostname:%s", val);
            if (len >= outsz)
            {
                LOG(WARN, "dhcp_client: Xopt encoding error, buffer too small for hostname.");
                return false;
            }
            break;

        case DHCP_OPTION_PLUME_SWVER:
        case DHCP_OPTION_PLUME_PROFILE:
        case DHCP_OPTION_PLUME_SERIAL_OPT:
            /* HEX encode */
            len = snprintf(out, outsz, "0x%02X:", opt);
            /* Output was truncated */
            if (len >= outsz)
            {
                LOG(WARN, "dhcp_client: Xopt encoding error, buffer too small for hex option.");
                return false;
            }

            out += len; outsz -= len;

            /* Encode value */
            for (ii = 0; ii < strlen(val); ii++)
            {
                len = snprintf(out, outsz, "%02X", val[ii]);
                if (len >= outsz)
                {
                    LOG(WARN, "dhcp_client: Xopt encoding error, buffer too small for hex value: %s\n", val);
                    return false;
                }

                out += len; outsz -= len;
            }
            break;

        default:
            LOG(WARN, "dhcp_client: Don't know how to encode Xopt %d.", opt);
            /* Don't know how to encode the rest of the options */
            return false;
    }

    return true;
}

bool osn_dhcp_client_stop(osn_dhcp_client_t *self)
{
    if (!self->dc_started) return true;

    /* Nuke the client options array */
    if (self->dc_opt_fn)
    {
        self->dc_opt_fn(self->dc_opt_ctx, NOTIFY_SYNC, NULL, NULL);
        self->dc_opt_fn(self->dc_opt_ctx, NOTIFY_FLUSH, NULL, NULL);
    }

    /* Stop watchers */
    ev_stat_stop(EV_DEFAULT, &self->dc_opt_stat);
    ev_debounce_stop(EV_DEFAULT, &self->dc_opt_debounce);

    if (!daemon_stop(&self->dc_proc))
    {
        LOG(WARN, "dhcp_client: Error stopping DHCP client on interface %s.", self->dc_ifname);
        return false;
    }

    self->dc_started = false;

    return true;
}

/**
 * Request option @p opt from server
 */
bool osn_dhcp_client_opt_request(osn_dhcp_client_t *self, enum osn_dhcp_option opt, bool request)
{
    if (opt >= DHCP_OPTION_MAX)
    {
        return false;
    }

    C_SET_VAL(self->dc_option_req, opt, request);

    return true;
}

/**
 * Set a DHCP option -- these options will be sent to the DHCP server, pass a NULL value to unset the option
 */
bool osn_dhcp_client_opt_set(
        osn_dhcp_client_t *self,
        enum osn_dhcp_option opt,
        const char *val)
{
    if (opt >= DHCP_OPTION_MAX) return false;

    /* First, unset the old option */
    if (self->dc_option_set[opt] != NULL)
    {
        free(self->dc_option_set[opt]);
        self->dc_option_set[opt] = NULL;
    }

    if (val == NULL) return true;

    self->dc_option_set[opt] = strdup(val);

    return true;
}

/*
 * Return the request status and value a DHCP options
 */
bool osn_dhcp_client_opt_get(osn_dhcp_client_t *self, enum osn_dhcp_option opt, bool *request, const char **value)
{
    if (opt >= DHCP_OPTION_MAX) return false;

    *value = self->dc_option_set[opt];
    *request = C_IS_SET(self->dc_option_req, opt);

    return true;
}


/**
 * Set the error callback function -- this function is called to signal that an error occurred
 * during normal operation. Should only be called between a _star() and _stop() function, not
 * before nor after.
 *
 * Use NULL to unset it.
 */
bool osn_dhcp_client_error_fn_set(osn_dhcp_client_t *self, osn_dhcp_client_error_fn_t *errfn)
{
    self->dc_err_fn = errfn;

    return true;
}

bool osn_dhcp_client_state_get(osn_dhcp_client_t *self, bool *enabled)
{
    return daemon_is_started(&self->dc_proc, enabled);
}

/*
 * Set option notification callback
 */
bool osn_dhcp_client_opt_notify_set(osn_dhcp_client_t *self, osn_dhcp_client_opt_notify_fn_t *fn, void *ctx)
{
    self->dc_opt_fn = fn;
    self->dc_opt_ctx = ctx;

    return true;
}

/*
 * UDHCPC option file watcher
 */
void dhcpc_opt_watcher(struct ev_loop *loop, ev_stat *w, int revent)
{
    (void)loop;
    (void)revent;

    osn_dhcp_client_t *self = w->data;

    LOG(INFO, "dhcp_client: %s: File changed: %s", self->dc_ifname, self->dc_opt_path);
    ev_debounce_start(loop, &self->dc_opt_debounce);
}

/*
 * Read the options file, parse the key=value pair and emit events
 */
void dhcpc_opt_read(struct ev_loop *loop, ev_debounce *w, int revent)
{
    (void)loop;
    (void)revent;

    osn_dhcp_client_t *self = w->data;

    FILE *fopt;
    char buf[256];

    LOG(INFO, "dhcp_client: %s: Debounced: %s", self->dc_ifname, self->dc_opt_path);

    /* Nothing to do if there's no notification callback */
    if (self->dc_opt_fn == NULL) return;

    self->dc_opt_fn(self->dc_opt_ctx, NOTIFY_SYNC, NULL, NULL);

    fopt = fopen(self->dc_opt_path, "r");
    if (fopt == NULL)
    {
        LOG(ERR, "dhcp_client: %s: Error opening options file: %s",
                self->dc_ifname,
                self->dc_opt_path);
        goto error;
    }

    while (fgets(buf, sizeof(buf), fopt) != NULL)
    {
        char *pbuf = buf;

        char *key = strsep(&pbuf, "=");
        /* Use "\n" as delimiter, so that terminating new lines are also stripped away */
        char *value = strsep(&pbuf, "\n");

        if ((key == NULL) || (value == NULL))
        {
            LOG(ERR, "dhcp_client: %s: Error parsing option file, line skipped.", self->dc_ifname);
            continue;
        }

        /* Skip empty lines */
        if (key[0] == '\0') continue;

        LOG(TRACE, "dhcp_client: %s: Options KEY = %s, VALUE = %s", self->dc_ifname, key, value);

        self->dc_opt_fn(self->dc_opt_ctx, NOTIFY_UPDATE, key, value);
    }

error:
    if (fopt != NULL) fclose(fopt);
    self->dc_opt_fn(self->dc_opt_ctx, NOTIFY_FLUSH, NULL, NULL);
}
