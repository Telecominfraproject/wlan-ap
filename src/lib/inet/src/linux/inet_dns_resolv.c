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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <glob.h>
#include <errno.h>

#include "evx.h"
#include "const.h"
#include "log.h"
#include "util.h"
#include "execsh.h"

#include "inet_dns.h"

#if !defined(CONFIG_USE_KCONFIG)

#define CONFIG_INET_RESOLVCONF_PATH         "/tmp/resolv.conf"
#define CONFIG_INET_RESOLVCONF_TMP          "/tmp/dns"

#endif /* CONFIG_USE_KCONFIG */

#define INET_DNS_DEBOUNCE           0.5                 /* Configuration delay, in seconds */
#define INET_DNS_GLOB               "*.resolv"
#define INET_DNS_LINE_LEN           256                 /* Max. single line length in *.resolv files */

struct __inet_dns
{
    char                dns_ifname[C_IFNAME_LEN];       /* Interface name */
    bool                dns_enabled;                    /* True if service has been started */
    osn_ip_addr_t       dns_primary;                    /* Primary DNS addrss */
    osn_ip_addr_t       dns_secondary;                  /* Secondary DNS addrss */
};

static bool inet_dns_init(inet_dns_t *self, const char *ifname);
static bool inet_dns_fini(inet_dns_t *self);
static bool inet_dns_path(inet_dns_t *self, char *dest, ssize_t destsz);

/* Global debounce timer */
static ev_debounce inet_dns_update_timer;
static bool inet_dns_update(inet_dns_t *self);
static void __inet_dns_update(EV_P_ ev_debounce *w, int revent);

static bool __inet_dns_global_init = false;

/*
 * ===========================================================================
 *  Public API
 * ===========================================================================
 */

/**
 * Constructor
 */
inet_dns_t *inet_dns_new(const char *ifname)
{
    inet_dns_t *self = malloc(sizeof(inet_dns_t));
    if (self == NULL) return NULL;

    if (!inet_dns_init(self, ifname))
    {
        free(self);
        return NULL;
    }

    return self;
}

/**
 * Destructor
 */
bool inet_dns_del(inet_dns_t *self)
{
    bool retval = inet_dns_fini(self);

    free(self);

    return retval;
}

/**
 * Initializer
 */
bool inet_dns_init(inet_dns_t *self, const char *ifname)
{
    if (!__inet_dns_global_init)
    {
        ev_debounce_init(&inet_dns_update_timer, __inet_dns_update, INET_DNS_DEBOUNCE);
        __inet_dns_global_init = true;
    }

    memset(self, 0, sizeof(*self));
    self->dns_primary = OSN_IP_ADDR_INIT;
    self->dns_secondary = OSN_IP_ADDR_INIT;

    if (strscpy(self->dns_ifname, ifname, sizeof(self->dns_ifname)) < 0)
    {
        LOG(ERR, "inet_dns: %s: Interface name too long.", ifname);
        return false;
    }

    return true;
}

/**
 * Finalizer
 */
bool inet_dns_fini(inet_dns_t *self)
{
    return inet_dns_stop(self);
}

/*
 * Start/stop the DNS service
 */
bool inet_dns_start(inet_dns_t *self)
{
    size_t psz;

    char presolv[C_MAXPATH_LEN];
    FILE *fresolv = NULL;
    bool retval = false;

    presolv[0] = '\0';

    if (self->dns_enabled) return true;

    self->dns_enabled = true;

    /* Create the temporary folder if it doesn't exist */
    if (mkdir( CONFIG_INET_RESOLVCONF_TMP, 0700) != 0 && errno != EEXIST)
    {
        LOG(ERR, "inet_dns: %s: Error creating temporary resolv folder: %s (%s)",
                self->dns_ifname,
                 CONFIG_INET_RESOLVCONF_TMP,
                strerror(errno));
        goto exit;
    }

    if (!inet_dns_path(self, presolv, sizeof(presolv)))
    {
        LOG(ERR, "inet_dns: %s: Error generating resolv file path (start).", self->dns_ifname);
        goto exit;
    }

    /*
     * Write out the interface resolve file
     */
    fresolv = fopen(presolv, "w");
    if (fresolv == NULL)
    {
        LOG(ERR, "inet_dns: %s: Error opening interface resolv file: %s", self->dns_ifname, presolv);
        goto exit;
    }

    fprintf(fresolv, "# Interface: %s\n", self->dns_ifname);
    if (osn_ip_addr_cmp(&self->dns_primary, &OSN_IP_ADDR_INIT) != 0)
    {
        psz = fprintf(fresolv, "nameserver "PRI_osn_ip_addr"\n", FMT_osn_ip_addr(self->dns_primary));
        if (psz >= INET_DNS_LINE_LEN)
        {
            LOG(ERR, "inet_eth: %s: primary nameserver string too long.", self->dns_ifname);
            goto exit;
        }
    }

    if (osn_ip_addr_cmp(&self->dns_secondary, &OSN_IP_ADDR_INIT) != 0)
    {
        psz = fprintf(fresolv, "nameserver "PRI_osn_ip_addr"\n", FMT_osn_ip_addr(self->dns_secondary));
        if (psz >= INET_DNS_LINE_LEN)
        {
            LOG(ERR, "inet_eth: %s: secondary nameserver string too long.", self->dns_ifname);
            goto exit;
        }
    }

    if (!inet_dns_update(self))
    {
        LOG(ERR, "inet_eth: %s: Error updating global DNS instance.", self->dns_ifname);
        goto exit;
    }

    retval = true;

exit:
    if (fresolv != NULL)
    {
        fclose(fresolv);
    }

    if (!retval && presolv[0] != '\0')
    {
        unlink(presolv);
    }

    return retval;
}

/**
 * Stop the DNS service -- simply remove the per-interface resolv file and kick off a new
 * global resolv file update
 */
bool inet_dns_stop(inet_dns_t *self)
{
    char presolv[C_MAXPATH_LEN];

    if (!self->dns_enabled) return true;

    self->dns_enabled = false;

    if (!inet_dns_path(self, presolv, sizeof(presolv)))
    {
        LOG(ERR, "inet_dns: %s: Error generating resolv file path (stop).", self->dns_ifname);
        return false;
    }

    if (unlink(presolv) != 0)
    {
        LOG(ERR, "inet_dns: %s: Error removing interface resolv file: %s", self->dns_ifname, presolv);
        return false;
    }

    inet_dns_update(self);

    return true;
}

bool inet_dns_server_set(
        inet_dns_t *self,
        osn_ip_addr_t primary,
        osn_ip_addr_t secondary)
{
    self->dns_primary = primary;
    self->dns_secondary = secondary;

    return true;
}

/*
 * ===========================================================================
 *  Private functions
 * ===========================================================================
 */

/**
 * Return the path of the resolv file for this interface
 */
bool inet_dns_path(inet_dns_t *self, char *dest, ssize_t destsz)
{
    int psz;

    /* XXX: Prefix the name with a _ -- this way NM set hostnames have a higher precedence (comapred to UDHCPC obtained DNS settings) */
    psz = snprintf(dest, destsz, "%s/_%s.resolv",
             CONFIG_INET_RESOLVCONF_TMP,
            self->dns_ifname);
    if (psz >= destsz)
    {
        LOG(ERR, "inet_dns: %s: Not enough buffer room for generating temporary DNS name.", self->dns_ifname);
        return false;
    }

    return true;
}

/**
 * Update the current DNS configuration
 */
bool inet_dns_update(inet_dns_t *self)
{
    (void)self;

    /* Re-arm the debounce timer */
    ev_debounce_start(EV_DEFAULT, &inet_dns_update_timer);

    return true;
}

void __inet_dns_update(EV_P_ ev_debounce *w, int revent)
{
    (void)loop;
    (void)w;
    (void)revent;

    size_t psz;
    size_t ii;
    glob_t gl;
    char dline[INET_DNS_LINE_LEN];
    char presolv[C_MAXPATH_LEN];
    FILE *ftmp;
    int rc;

    FILE *fresolv = NULL;
    int dresolv = -1;
    bool retval = false;

    memset(&gl, 0, sizeof(gl));
    presolv[0] = '\0';

    /* Generate a temporary filename */
    psz = snprintf(presolv, sizeof(presolv), "%s_XXXXXX", CONFIG_INET_RESOLVCONF_PATH);
    if (psz >= sizeof(presolv))
    {
        LOG(ERR, "inet_dns: Unable to generate temporary resolv file name, path too long.");
        goto exit;
    }

    /* Create the temporary file */
    dresolv = mkstemp(presolv);
    if (dresolv < 0)
    {
        LOG(ERR, "inet_dns: Unable to create the temporary resolv file: %s", strerror(errno));
        goto exit;
    }

    fresolv = fdopen(dresolv, "w");
    if (fresolv == NULL)
    {
        LOG(ERR, "inet_dns: fdopen() failed (%s), unable to generate global resolv file: %s", strerror(errno), presolv);
        goto exit;
    }
    dresolv = -1;

    /* Scan the  CONFIG_INET_RESOLVCONF_TMP folder and combine all interface resolve files into one file -- /tmp/resolv.conf */
    rc = glob( CONFIG_INET_RESOLVCONF_TMP "/" INET_DNS_GLOB, 0, NULL, &gl);
    if (rc != 0 && rc != GLOB_NOMATCH)
    {
        LOG(ERR, "inet_dns: Glob error on pattern %s.",  CONFIG_INET_RESOLVCONF_TMP "/" INET_DNS_GLOB);
        goto exit;
    }

    /* Combine all *.resolv files into one */
    for (ii = 0; ii < gl.gl_pathc; ii++)
    {
        ftmp = fopen(gl.gl_pathv[ii], "r");
        if (ftmp == NULL) continue;

        while (fgets(dline, sizeof(dline), ftmp) != NULL)
        {
            if (fputs(dline, fresolv) < 0)
            {
                LOG(ERR, "inet_dns: Error combining %s with %s, write error: %s",
                        gl.gl_pathv[ii],
                        presolv,
                        strerror(errno));
                fclose(ftmp);
                goto exit;
            }
        }

        fputs("\n", ftmp);

        fclose(ftmp);
    }

    retval = true;

exit:
    if (gl.gl_pathv != NULL) globfree(&gl);
    if (fresolv != NULL) fclose(fresolv);
    if (dresolv >= 0) close(dresolv);

    if (presolv[0] != '\0')
    {
        if (retval)
        {
            if (rename(presolv, CONFIG_INET_RESOLVCONF_PATH) != 0)
            {
                LOG(ERR, "inet_dns: Error renaming file %s -> %s: %s",
                        presolv,
                        CONFIG_INET_RESOLVCONF_PATH,
                        strerror(errno));
            }
        }
        else
        {
            if (unlink(presolv) != 0)
            {
                LOG(ERR, "inet_dns: Error deleting file %s: %s",
                        presolv,
                        strerror(errno));
            }
        }
    }
}
